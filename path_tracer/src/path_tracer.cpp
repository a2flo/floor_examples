
#include "path_tracer.hpp"

#if defined(FLOOR_COMPUTE)

// to keep things simple in here, the cornell box model and material data is located in another file
#include "cornell_box.hpp"

struct ray {
	float3 origin;
	float3 direction;
};

// need a larger epsilon in some cases (than the one defined by const_math::EPSILON<float>)
#define INTERSECTION_EPS 0.0001f

class simple_intersector {
public:
	struct intersection {
		const float3 hit_point;
		const float3 normal;
		const float distance;
		const CORNELL_OBJECT object;
	};
	
	// to keep things simple, do a simple O(n) intersection here (well, O(32), so constant ;))
	// NOTE: will assume we're only intersecting the cornell box data here, so the vertices/indices
	// don't have to be carried around the whole time (this will/should work with anything though)
	static intersection intersect(const ray& r) {
		float3 normal;
		float distance { numeric_limits<float>::infinity() };
		CORNELL_OBJECT object { CORNELL_OBJECT::__OBJECT_COUNT };
		
		for(uint32_t triangle_idx = 0; triangle_idx < size(cornell_indices); ++triangle_idx) {
			const auto& index = cornell_indices[triangle_idx];
			const auto& v0 = cornell_vertices[index.x];
			const auto& v1 = cornell_vertices[index.y];
			const auto& v2 = cornell_vertices[index.z];
	
			const auto e1 = v0.xyz - v2.xyz, e2 = v1.xyz - v2.xyz;
			const auto pvec = r.direction.crossed(e2);
			const auto det = e1.dot(pvec);
			if(fabs(det) <= const_math::EPSILON<float>) continue;
			
			const auto inv_det = 1.0f / det;
			const auto tvec = r.origin - v2.xyz;
			const auto qvec = tvec.crossed(e1);
			
			float3 ip;
			ip.x = tvec.dot(pvec) * inv_det;
			ip.y = r.direction.dot(qvec) * inv_det;
			ip.z = 1.0f - ip.x - ip.y;
			
			if((ip < 0.0f).any()) continue;
			
			const auto idist = e2.dot(qvec) * inv_det;
			if(idist >= INTERSECTION_EPS && idist < distance) {
				normal = e1.crossed(e2).normalized();
				distance = idist;
				object = cornell_object_map[triangle_idx];
			}
		}
		
		return intersection {
			.hit_point = r.origin + r.direction * distance,
			.normal = normal,
			.distance = distance,
			.object = object,
		};
	}
	
};

class simple_path_tracer {
public:
	enum : uint32_t { max_recursion_depth = 2 };
	
	simple_path_tracer(const uint32_t& random_seed) : seed(random_seed) {}
	
	// not the best random, but simple and good enough
	// ref: http://iquilezles.org/www/articles/sfrand/sfrand.htm
	//! returns a random value in [0, 1]
	float rand_0_1() {
		float res;
#if defined(FLOOR_COMPUTE_METAL) && 1
		// apple h/w or s/w seems to have trouble with doing 32-bit uint multiplies,
		// so do a software 32-bit * 16-bit multiply instead
		uint32_t low = (seed & 0xFFFFu) * 16807u;
		uint32_t high = ((seed >> 16u) & 0xFFFFu) * 16807u;
		seed = (high << 16u) + low;
#else
		seed *= 16807u;
#endif
		*((uint32_t*)&res) = (seed >> 9u) | 0x3F800000u;
		return (res - 1.0f);
	}
	
	// if you can't do normal recursion, do some template recursion instead! (at the cost of code bloat)
	template <uint32_t depth = 0, enable_if_t<depth < max_recursion_depth, int> = 0>
	float3 compute_radiance(const ray& r, const bool sample_emission) {
		// intersect
		const auto p = simple_intersector::intersect(r);
		if(p.object >= CORNELL_OBJECT::__OBJECT_COUNT) return {};
		
		//
		float3 radiance;
		const auto& mat = cornell_materials[(size_t)p.object];
		
		// emission (don't oversample for indirect illumination)
		if(sample_emission) radiance += mat.emission.xyz;
		
		// compute direct illumination at given point
		radiance += compute_direct_illumination(-r.direction, p, mat);
		
		// indirect
		radiance += compute_indirect_illumination<depth>(r, p, mat);
		
		return radiance;
	}
	
	// terminator
	template <uint32_t depth, enable_if_t<depth >= max_recursion_depth, int> = 0>
	float3 compute_radiance(const ray&, const bool) {
		return {};
	}
	
protected:
	uint32_t seed;
	
	typedef struct { const float3 dir; const float prob; } hemisphere_dir_type;
	
	template <uint32_t depth>
	float3 compute_indirect_illumination(const ray& r,
										 const simple_intersector::intersection& p,
										 const material& mat) {
		// get the normal (check if it has to be flipped - rarely happens, but it does)
		const auto normal = (p.normal.dot(r.direction) > 0.0f ? -p.normal : p.normal);
		
		// albedo (pd, ps) (note that (albedo_diffuse + albedo_specular) will always be <= 1)
		const auto albedo_diffuse = mat.diffuse.dot(float3 { 0.222f, 0.7067f, 0.0713f });
		const auto albedo_specular = mat.specular.dot(float3 { 0.222f, 0.7067f, 0.0713f });
		
		float3 radiance;
		// c++14 generic lambdas work as well ;)
		const auto radiance_recurse = [this, &radiance](const auto& albedo,
														const auto& dir_and_prob,
														const auto& point) {
			// no need to recurse if the radiance is already 0
			if(radiance.x > 0.0f || radiance.y > 0.0f || radiance.z > 0.0f) {
				radiance *= compute_radiance<depth + 1>(ray { point, dir_and_prob.dir }, false);
				radiance /= (albedo * dir_and_prob.prob);
			}
		};
		
		// russian roulette with albedo and random value in [0, 1]
		const auto rrr = rand_0_1();
		if(rrr < albedo_diffuse) {
			const auto tb = get_local_system(normal);
			const auto dir_and_prob = generate_cosine_weighted_direction(normal, tb.first, tb.second,
																		 { rand_0_1(), rand_0_1() });
			
			radiance = mat.diffuse_reflectance.xyz;
			radiance *= std::max(normal.dot(dir_and_prob.dir), 0.0f);
			radiance_recurse(albedo_diffuse, dir_and_prob, p.hit_point);
		}
		else if(rrr < (albedo_diffuse + albedo_specular)) {
			const auto rnormal = float3::reflect(normal, r.direction).normalized();
			const auto tb = get_local_system(rnormal);
			const auto dir_and_prob = generate_power_cosine_weighted_direction(rnormal, tb.first, tb.second,
																			   { rand_0_1(), rand_0_1() },
																			   mat.specular.w);
			
			// "reject directions below the surface" (or in it)
			if(normal.dot(dir_and_prob.dir) <= 0.0f) {
				return {};
			}
			
			radiance = mat.specular_reflectance.xyz;
			radiance *= pow(std::max(rnormal.dot(dir_and_prob.dir), 0.0f), mat.specular.w);
			radiance *= std::max(normal.dot(dir_and_prob.dir), 0.0f);
			radiance_recurse(albedo_specular, dir_and_prob, p.hit_point);
		}
		// else: rrr is 1.0f and the contribution is 0
		
		// we're done here
		return radiance;
	}
	
	float3 compute_direct_illumination(const float3& eye_direction,
									   const simple_intersector::intersection& p,
									   const material& mat) {
		float3 retval;
		
		const auto norm_surface = p.normal.normalized();
		const auto norm_eye_dir = eye_direction.normalized();
		
		// usually a loop over all light sources, but we only have one here
		constexpr const struct light_source {
			const float3 position { 34.3f, 54.8f, 22.7f };
			const float3 normal { 0.0f, -1.0f, 0.0f }; // down
			const float3 edge_1 { 0.0f, 0.0f, 33.2f - 22.7f };
			const float3 edge_2 { 21.3f - 34.3f, 0.0f, 0.0f };
			const float3 intensity {
				cornell_materials[size_t(CORNELL_OBJECT::LIGHT)].emission.x,
				cornell_materials[size_t(CORNELL_OBJECT::LIGHT)].emission.y,
				cornell_materials[size_t(CORNELL_OBJECT::LIGHT)].emission.z
			};
			
			constexpr float3 get_point(const float2& coord) const {
				return position + coord.x * edge_1 + coord.y * edge_2;
			}
			constexpr float get_area() const {
				return edge_1.crossed(edge_2).length();
			}
		} light {};
		{
			// generate a random position on the lightsource
			const auto sample_point = light.get_point(float2 { rand_0_1(), rand_0_1() });
			
			// cast shadow ray towards the light
			const auto dir = sample_point - p.hit_point;
			const auto dist_sqr = std::max(dir.dot(dir), INTERSECTION_EPS);
			const ray r { p.hit_point, dir.normalized() };
			const auto ret = simple_intersector::intersect(r);
			if(ret.object >= CORNELL_OBJECT::__OBJECT_COUNT ||
			   ret.distance >= sqrt(dist_sqr) - INTERSECTION_EPS ||
			   ret.object == CORNELL_OBJECT::LIGHT) {
				// didn't hit anything -> compute contribution
				
				// falloff formula: (.x falloff / dist^2) * intensity
				const auto attenuation = 1.0f / dist_sqr; // here: 1 / dist^2
				const auto elvis = std::max(-light.normal.dot(r.direction), 0.0f); // actually -r.d
				const auto lambert = std::max(norm_surface.dot(r.direction), 0.0f);
				
				// diffuse
				float3 illum = mat.diffuse_reflectance.xyz;
				
				// specular (only do the computation if it actually has a spec coeff > 0)
				if(!mat.specular.is_null()) {
					const auto R = float3::reflect(norm_surface, -r.direction).normalized();
					illum += (mat.specular_reflectance.xyz *
							  pow(std::max(R.dot(norm_eye_dir), 0.0f), mat.specular.w));
				}
				
				// mul by intensity and mul single floats separately
				illum *= light.intensity * (elvis * lambert * light.get_area() * attenuation);
				
				retval += illum;
			}
		}
		
		return retval;
	}
	
	static hemisphere_dir_type generate_cosine_weighted_direction(//local coordinate system
																  const float3& up,
																  const float3& left,
																  const float3& forward,
																  //two random numbers provided from outside
																  const float2& xi) {
		const auto sincos_inner = const_math::PI_MUL_2<float> * xi.x;
		const auto sqrt_term_xy = sqrt(1.0f - xi.y), sqrt_term_z = sqrt(xi.y);
		float3 dir { cos(sincos_inner) * sqrt_term_xy, sqrt_term_z, sin(sincos_inner) * sqrt_term_xy };
		
		// transform dir
		transform_dir(dir, up, left, forward);
		
		// prob = sqrt(r2) / pi
		return { dir, sqrt_term_z * const_math::_1_DIV_PI<float> };
	}
	
	static hemisphere_dir_type generate_power_cosine_weighted_direction(//local coordinate system
																		const float3& up,
																		const float3& left,
																		const float3& forward,
																		//two random numbers provided from outside
																		const float2& xi,
																		//the power of the cosine lobe
																		const float power) {
		const auto sincos_inner = const_math::PI_MUL_2<float> * xi.x;
		const auto r_pow_1 = pow(xi.y, 1.0f / (power + 1.0f));
		const auto sqrt_term = sqrt(1.0f - r_pow_1 * r_pow_1);
		float3 dir { cos(sincos_inner) * sqrt_term, r_pow_1, sin(sincos_inner) * sqrt_term };
		
		// transform dir
		transform_dir(dir, up, left, forward);
		
		// prob = ((n+1) * (r2^(1/(n+1)))^n) / 2pi
		return { dir, ((power + 1.0f) * pow(r_pow_1, power)) * const_math::_1_DIV_2PI<float> };
	}
	
	static void transform_dir(float3& dir,
							  const float3& up,
							  const float3& left,
							  const float3& forward) {
		// X = left, Y = up, Z = forward
		dir.normalize();
		dir = {
			dir.x * left.x + dir.y * up.x + dir.z * forward.x,
			dir.x * left.y + dir.y * up.y + dir.z * forward.y,
			dir.x * left.z + dir.y * up.z + dir.z * forward.z,
		};
		dir.normalize();
	}
	
	static pair<float3, float3> get_local_system(const float3& normal) {
		// compute orthogonal local coordinate system
		const bool norm_x_greater_y = (fabs(normal.x) > fabs(normal.y));
		const float norm_c0 = norm_x_greater_y ? normal.x : normal.y;
		const float sig = norm_x_greater_y ? -1.0f : 1.0f;
		
		const float invLen = sig / (norm_c0 * norm_c0 + normal.z * normal.z);
		
		float3 tangent;
		if(norm_x_greater_y) {
			tangent.x = normal.z * invLen;
			tangent.y = 0.0f;
		}
		else {
			tangent.x = 0.0f;
			tangent.y = normal.z * invLen;
		}
		tangent.z = norm_c0 * -1.0f * invLen;
		tangent.normalize();
		const float3 binormal = normal.crossed(tangent).normalize();
		
		return { tangent, binormal };
	}
	
};

//
namespace camera {
	static constexpr const float3 point { 27.8f, 27.3f, -80.0f };
	static constexpr const float rad_angle = const_math::deg_to_rad(35.0f);
	static constexpr const float3 forward { 0.0f, 0.0f, 1.0f };
	static constexpr const float3 right { forward.crossed(float3 { 0.0f, 1.0f, 0.0f } /* up */).normalized() };
	static constexpr const float3 up { -right.crossed(forward).normalized() };
	static constexpr const float3 right_vector = 2.0f * right * const_math::tan(rad_angle * 0.5f);
	static constexpr const float3 up_vector = 2.0f * up * const_math::tan(rad_angle * 0.5f);
	static constexpr const float aspect_ratio = SCREEN_WIDTH / SCREEN_HEIGHT;
	static constexpr const float3 row_vector = right_vector * aspect_ratio;
	static constexpr const float3 step_x { row_vector / SCREEN_WIDTH };
	static constexpr const float3 step_y { up_vector / SCREEN_HEIGHT };
	static constexpr const float3 screen_origin { forward - row_vector * 0.5f - up_vector * 0.5f };
};

kernel void path_trace(buffer<float4> img,
					   param<uint32_t> iteration,
					   param<uint32_t> seed) {
	const auto idx = global_id.x;
	const uint2 pixel { idx % SCREEN_WIDTH, idx / SCREEN_HEIGHT };
	if(pixel.y >= SCREEN_HEIGHT) return;
	
	// this is hard ... totally random
	uint32_t random_seed = seed;
	random_seed += {
		(random_seed ^ (idx << (random_seed & ((idx + iteration) & 0x1F)))) +
		((idx + random_seed) * SCREEN_WIDTH * SCREEN_HEIGHT) ^ 0x52FBD9EC
	};
	simple_path_tracer pt(random_seed);
	
	//
	const float2 pixel_sample { float2(pixel) + float2(pt.rand_0_1(), pt.rand_0_1()) };
	
	//
	const ray r {
		.origin = camera::point,
		.direction = camera::screen_origin + pixel_sample.x * camera::step_x + pixel_sample.y * camera::step_y
	};
	float3 color = pt.compute_radiance(r, true);

	// red test strip, so that I know if the output is working at all
	//if(idx % img_size->x == 0) color = { 1.0f, 0.0f, 0.0f };
	
	// merge with previous frames (re-weight)
	if(iteration == 0) img[idx] = color;
	else img[idx] = img[idx].interpolate(color, 1.0f / float(iteration + 1));
}

#endif
