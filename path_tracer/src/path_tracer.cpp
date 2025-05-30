/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "path_tracer.hpp"

#if defined(FLOOR_DEVICE)
using namespace fl;
using namespace std;

// to keep things simple in here, the cornell box model and material data is located in another file
#include "cornell_box.hpp"

struct ray {
	float3 origin;
	float3 direction;
};

// need a larger epsilon in some cases (than the one defined by const_math::EPSILON<float>)
static constexpr const float intersection_epsilon { 0.0001f };

class simple_intersector {
public:
	struct intersection {
		const float3 hit_point;
		const float3 normal;
		const float distance;
		const CORNELL_OBJECT object;
		const float2 tex_coord;
	};
	
	// to keep things simple, do a simple O(n) intersection here (well, O(32), so constant ;))
	// NOTE: will assume we're only intersecting the cornell box data here, so the vertices/indices
	// don't have to be carried around the whole time (this will/should work with anything though)
	static intersection intersect(const ray& r) {
		float3 normal;
		float distance { numeric_limits<float>::infinity() };
		CORNELL_OBJECT object { CORNELL_OBJECT::__OBJECT_INVALID };
		float2 tex_coord;
		
		for (uint32_t triangle_idx = 0; triangle_idx < size(cornell_indices); ++triangle_idx) {
			const auto index = cornell_indices[triangle_idx];
			const auto v0 = cornell_vertices[index.x];
			const auto v1 = cornell_vertices[index.y];
			const auto v2 = cornell_vertices[index.z];
	
			const auto e1 = v0.xyz - v2.xyz, e2 = v1.xyz - v2.xyz;
			const auto pvec = r.direction.crossed(e2);
			const auto det = e1.dot(pvec);
			if (fabs(det) <= const_math::EPSILON<float>) {
				continue;
			}
			
			const auto inv_det = 1.0f / det;
			const auto tvec = r.origin - v2.xyz;
			const auto qvec = tvec.crossed(e1);
			
			float3 ip;
			ip.x = tvec.dot(pvec) * inv_det;
			ip.y = r.direction.dot(qvec) * inv_det;
			ip.z = 1.0f - ip.x - ip.y;
			
			if ((ip < 0.0f).any()) {
				continue;
			}
			
			const auto idist = e2.dot(qvec) * inv_det;
			if (idist >= intersection_epsilon && idist < distance) {
				normal = e1.crossed(e2); // don't normalize yet
				distance = idist;
				object = cornell_object_map[triangle_idx];
				
				// compute barycentric coord
				const auto hit_point = r.origin + r.direction * idist;
				const auto iv0 = v0.xyz - hit_point;
				const auto iv1 = v1.xyz - hit_point;
				const auto iv2 = v2.xyz - hit_point;
				const auto bary_uv = float2 { iv1.crossed(iv2).length(), iv2.crossed(iv0).length() } * (1.0f / normal.length());
				tex_coord = (cornell_tex_coords[index.x] * bary_uv.x +
							 cornell_tex_coords[index.y] * bary_uv.y +
							 cornell_tex_coords[index.z] * (1.0f - bary_uv.x - bary_uv.y));
				tex_coord.y = tex_scaler - tex_coord.y;
				
				normal.normalize();
			}
		}
		
		return {
			.hit_point = r.origin + r.direction * distance,
			.normal = normal,
			.distance = distance,
			.object = object,
			.tex_coord = tex_coord,
		};
	}
	
};

template <bool with_textures, typename textures_type>
class simple_path_tracer {
public:
	static constexpr const uint32_t max_recursion_depth { 3u };
	
	simple_path_tracer(const uint32_t& random_seed) : seed(random_seed) {}
	
	//! not the best random, but simple and good enough
	//! ref: http://iquilezles.org/www/articles/sfrand/sfrand.htm
	//! ref: https://en.wikipedia.org/wiki/Lehmer_random_number_generator
	//! returns a random value in [0, 1]
	float rand_0_1() {
		seed *= 48271u;
		return (bit_cast<float>((seed >> 9u) | 0x3F800000u) - 1.0f);
	}
	
	// if you can't do normal recursion, do some template recursion instead! (at the cost of code bloat)
	// also: can do this inside a single function now with c++17
	template <uint32_t depth = 0>
	float3 compute_radiance(const ray& r, const bool sample_emission,
							const textures_type& textures) {
		if constexpr (depth >= max_recursion_depth) {
			return {};
		} else {
			// intersect
			const auto p = simple_intersector::intersect(r);
			if (p.object >= CORNELL_OBJECT::__OBJECT_INVALID) {
				return {};
			}
			
			float3 radiance;
			const auto& mat = cornell_materials[(size_t)p.object];
			
			float tex_value = 1.0f;
			if constexpr (with_textures) {
				tex_value = textures[mat.texture_index].read_linear_repeat(p.tex_coord);
			}
			
			// emission (don't oversample for indirect illumination)
			if (sample_emission) {
				radiance += tex_value * mat.emission.xyz;
			}
			
			// compute direct illumination at given point
			radiance += compute_direct_illumination(-r.direction, p, mat, textures);
			
			// indirect
			radiance += compute_indirect_illumination<depth>(r, p, mat, textures, tex_value);
			
			return radiance;
		}
	}
	
protected:
	uint32_t seed;
	
	struct hemisphere_dir_type { const float3 dir; const float prob; };
	
	template <uint32_t depth>
	float3 compute_indirect_illumination(const ray& r,
										 const simple_intersector::intersection& p,
										 const material& mat,
										 const textures_type& textures,
										 const float tex_value) {
		// get the normal (check if it has to be flipped - rarely happens, but it does)
		const auto normal = (p.normal.dot(r.direction) > 0.0f ? -p.normal : p.normal);
		
		// albedo (pd, ps) (note that (albedo_diffuse + albedo_specular) will always be <= 1)
		const auto albedo_diffuse = tex_value * mat.diffuse.dot(float3 { 0.222f, 0.7067f, 0.0713f });
		const auto albedo_specular = tex_value * mat.specular.dot(float3 { 0.222f, 0.7067f, 0.0713f });
		
		float3 radiance;
		// c++14 generic lambdas work as well ;)
		const auto radiance_recurse = [this, &radiance, &textures](const auto& albedo,
																   const auto& dir_and_prob,
																   const auto& point) {
			// no need to recurse if the radiance is already 0
			if (radiance.x > 0.0f || radiance.y > 0.0f || radiance.z > 0.0f) {
				radiance *= compute_radiance<depth + 1>(ray { point, dir_and_prob.dir }, false, textures);
				radiance /= (albedo * dir_and_prob.prob);
			}
		};
		
		// russian roulette with albedo and random value in [0, 1]
		const auto rrr = rand_0_1();
		if (rrr < albedo_diffuse) {
			const auto tb = get_local_system(normal);
			const auto dir_and_prob = generate_cosine_weighted_direction(normal, tb.first, tb.second,
																		 { rand_0_1(), rand_0_1() });
			
			radiance = tex_value * mat.diffuse_reflectance.xyz;
			radiance *= max(normal.dot(dir_and_prob.dir), 0.0f);
			radiance_recurse(albedo_diffuse, dir_and_prob, p.hit_point);
		} else if (rrr < (albedo_diffuse + albedo_specular)) {
			const auto rnormal = r.direction.reflected(normal).normalized();
			const auto tb = get_local_system(rnormal);
			const auto dir_and_prob = generate_power_cosine_weighted_direction(rnormal, tb.first, tb.second,
																			   { rand_0_1(), rand_0_1() },
																			   mat.specular.w);
			
			// "reject directions below the surface" (or in it)
			if (normal.dot(dir_and_prob.dir) <= 0.0f) {
				return {};
			}
			
			radiance = tex_value * mat.specular_reflectance.xyz;
			radiance *= pow(max(rnormal.dot(dir_and_prob.dir), 0.0f), mat.specular.w);
			radiance *= max(normal.dot(dir_and_prob.dir), 0.0f);
			radiance_recurse(albedo_specular, dir_and_prob, p.hit_point);
		}
		// else: rrr is 1.0f and the contribution is 0
		
		// we're done here
		return radiance;
	}
	
	float3 compute_direct_illumination(const float3& eye_direction,
									   const simple_intersector::intersection& p,
									   const material& mat,
									   const textures_type& textures) {
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
			const float area { edge_1.crossed(edge_2).length() };
			
			constexpr float3 get_point(const float2& coord) const {
				return position + coord.x * edge_1 + coord.y * edge_2;
			}
			constexpr float get_area() const {
				return area;
			}
		} light {};
		{
			// generate a random position on the light source
			const auto sample_point = light.get_point(float2 { rand_0_1(), rand_0_1() });
			
			// cast shadow ray towards the light
			const auto dir = sample_point - p.hit_point;
			const auto dist_sqr = max(dir.dot(dir), intersection_epsilon);
			const ray r { p.hit_point, dir.normalized() };
			const auto ret = simple_intersector::intersect(r);
			if (ret.object >= CORNELL_OBJECT::__OBJECT_INVALID ||
				ret.distance >= sqrt(dist_sqr) - intersection_epsilon ||
				ret.object == CORNELL_OBJECT::LIGHT) {
				// didn't hit anything -> compute contribution
				
				// falloff formula: (.x falloff / dist^2) * intensity
				const auto attenuation = 1.0f / dist_sqr; // here: 1 / dist^2
				const auto elvis = max(-light.normal.dot(r.direction), 0.0f); // actually -r.d
				const auto lambert = max(norm_surface.dot(r.direction), 0.0f);
				
				// diffuse
				float3 illum = mat.diffuse_reflectance.xyz;
				float light_tex_value = 1.0f;
				if constexpr (with_textures) {
					light_tex_value = textures[mat.texture_index].read_linear_repeat(ret.tex_coord);
					illum *= light_tex_value;
				}
				
				// specular (only do the computation if it actually has a spec coeff > 0)
				if (!mat.specular.is_null()) {
					const auto R = (-r.direction).reflect(norm_surface).normalize();
					illum += (mat.specular_reflectance.xyz * light_tex_value *
							  pow(max(R.dot(norm_eye_dir), 0.0f), mat.specular.w));
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
		
		const float inv_len = sig / (norm_c0 * norm_c0 + normal.z * normal.z);
		
		float3 tangent;
		if (norm_x_greater_y) {
			tangent.x = normal.z * inv_len;
			tangent.y = 0.0f;
		} else {
			tangent.x = 0.0f;
			tangent.y = normal.z * inv_len;
		}
		tangent.z = norm_c0 * -1.0f * inv_len;
		tangent.normalize();
		const float3 binormal = normal.crossed(tangent).normalize();
		
		return { tangent, binormal };
	}
	
};

namespace camera {
	static constexpr const float3 point { 27.8f, 27.3f, -80.0f };
	static constexpr const float rad_angle { const_math::deg_to_rad(35.0f) };
	static constexpr const float3 forward { 0.0f, 0.0f, 1.0f };
	static constexpr const float3 right { forward.crossed(float3 { 0.0f, 1.0f, 0.0f } /* up */).normalized() };
	static constexpr const float3 up { -right.crossed(forward).normalized() };
	static constexpr const float3 right_vector { 2.0f * right * const_math::tan(rad_angle * 0.5f) };
	static constexpr const float3 up_vector { 2.0f * up * const_math::tan(rad_angle * 0.5f) };
	static constexpr const float aspect_ratio { SCREEN_WIDTH / SCREEN_HEIGHT };
	static constexpr const float3 row_vector { right_vector * aspect_ratio };
	static constexpr const float3 step_x { row_vector / SCREEN_WIDTH };
	static constexpr const float3 step_y { up_vector / SCREEN_HEIGHT };
	static constexpr const float3 screen_origin { forward - row_vector * 0.5f - up_vector * 0.5f };
};

template <bool with_textures, typename textures_type>
static void path_trace(buffer<float4>& img, const uint32_t iteration, const uint32_t seed,
					   const textures_type& textures) {
	const auto idx = global_id.x;
	const uint2 pixel { idx % SCREEN_WIDTH, idx / SCREEN_HEIGHT };
	if (pixel.y >= SCREEN_HEIGHT) {
		return;
	}
	
	// this is hard ... totally random
	uint32_t random_seed = seed;
	random_seed += {
		(random_seed ^ (idx << (random_seed & ((idx + iteration) & 0x1Fu)))) +
		((idx + random_seed) * (SCREEN_WIDTH - 1u) * (SCREEN_HEIGHT - 1u)) ^ 0x52FBD9ECu
	};
	simple_path_tracer<with_textures, textures_type> pt(random_seed);
	
	//
	const float2 pixel_sample { float2(pixel) + float2(pt.rand_0_1(), pt.rand_0_1()) };
	
	//
	const ray r {
		.origin = camera::point,
		.direction = camera::screen_origin + pixel_sample.x * camera::step_x + pixel_sample.y * camera::step_y
	};
	float3 color = pt.compute_radiance(r, true, textures);

	// red test strip, so that I know if the output is working at all
	//if(idx % img_size->x == 0) color = { 1.0f, 0.0f, 0.0f };
	
	// merge with previous frames (re-weight)
	img[idx] = (iteration == 0 ? color : img[idx].interpolate(color, 1.0f / float(iteration + 1)));
}

kernel_1d() void path_trace(buffer<float4> img,
							param<uint32_t> iteration,
							param<uint32_t> seed) {
	path_trace<false>(img, iteration, seed, 0u);
}

kernel_1d() void path_trace_textured(buffer<float4> img,
									 param<uint32_t> iteration,
									 param<uint32_t> seed,
									 array_param<const_image_2d<float1>, 5u> textures) {
	path_trace<true>(img, iteration, seed, textures);
}

#endif
