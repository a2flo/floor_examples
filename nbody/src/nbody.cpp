/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#include "nbody.hpp"

#if defined(FLOOR_COMPUTE)

// ref: http://http.developer.nvidia.com/GPUGems3/gpugems3_ch31.html

static void compute_body_interaction(const float4& shared_body,
									 const float4& this_body,
									 float3& acceleration) {
#if !defined(FLOOR_COMPUTE_INFO_HAS_FMA_1) // (potentially!) non-fma version
	// 3 flops
	const float3 r { shared_body.xyz - this_body.xyz };
	// 5 flops + 1 flop = 6 flops
	const float dist_sq = r.dot(r) + (NBODY_SOFTENING * NBODY_SOFTENING);
	// 1 flop
	const float inv_dist = rsqrt(dist_sq);
	// 3 flops
	const float s = shared_body.w * inv_dist * inv_dist * inv_dist; // .w is body mass
	// 3 flops + 3 flops = 6 flops
	acceleration += r * s;
	// total: 19 flops + 4 shared/local load ops
#else // fma version
	// 3 flops
	const float3 r { shared_body.xyz - this_body.xyz };
	// 3 fma-ops (6 flops)
	const float dist_sq = fma(r.x, r.x, fma(r.y, r.y, fma(r.z, r.z, NBODY_SOFTENING * NBODY_SOFTENING)));
	// 1 flop
	const float inv_dist = rsqrt(dist_sq);
	// 3 flops
	const float s = shared_body.w * inv_dist * inv_dist * inv_dist;
	// 1 fma-op / 2 flops
	acceleration.x = fma(r.x, s, acceleration.x);
	// 1 fma-op / 2 flops
	acceleration.y = fma(r.y, s, acceleration.y);
	// 1 fma-op / 2 flops
	acceleration.z = fma(r.z, s, acceleration.z);
	// total: 13 ops (6 fma, 6 normal ops, 1 special op) + 4 shared/local load ops
	// note on nvidia h/w: these are 12 full speed ops + 1 at lesser speed (sm_20: 1/8, sm_21/sm_3x: 1/6, sm_5x: 1/4)
#endif
}

kernel void nbody_compute(buffer<const float4> in_positions,
						  buffer<float4> out_positions,
						  buffer<float3> velocities,
						  param<float> delta) {
	const auto idx = global_id.x;
	const auto body_count = global_size.x;
	
	float4 position = in_positions[idx];
	float3 velocity = velocities[idx];
	float3 acceleration;
	
#if 1 // local/shared-memory caching + computation
	const auto local_idx = local_id.x;
	local_buffer<float4, NBODY_TILE_SIZE> local_body_positions;
	for(uint32_t i = 0, tile = 0, count = body_count; i < count; i += NBODY_TILE_SIZE, ++tile) {
		local_body_positions[local_idx] = in_positions[tile * NBODY_TILE_SIZE + local_idx];
		local_barrier();

		// TODO: should probably add some kind of "max supported/good unroll count" define
#if (!defined(FLOOR_COMPUTE_METAL) || (defined(FLOOR_COMPUTE_INFO_OS_OSX) && !defined(FLOOR_COMPUTE_INFO_VENDOR_INTEL))) \
	&& !defined(FLOOR_COMPUTE_HOST)
#pragma clang loop unroll_count(NBODY_TILE_SIZE)
#elif defined(FLOOR_COMPUTE_METAL) && !defined(FLOOR_COMPUTE_INFO_VENDOR_INTEL)
#pragma clang loop unroll_count(4)
#elif defined(FLOOR_COMPUTE_HOST)
#pragma clang loop unroll_count(8) vectorize(enable) interleave(enable)
#endif
		for(uint32_t j = 0; j < NBODY_TILE_SIZE; ++j) {
			compute_body_interaction(local_body_positions[j], position, acceleration);
		}
		local_barrier();
	}
#else // global memory only computation
#pragma unroll // good enough
	for(uint32_t i = 0; i < body_count; ++i) {
		compute_body_interaction(in_positions[i], position, acceleration);
	}
#endif
	
	velocity += acceleration * delta;
	velocity *= NBODY_DAMPING;
	position.xyz += velocity * delta;
	
	out_positions[idx] = position;
	velocities[idx] = velocity;
}

kernel void nbody_raster(buffer<const float4> positions,
						 buffer<uint32_t> img,
						 buffer<uint32_t> img_old,
						 param<uint2> img_size,
						 param<uint32_t> body_count,
						 param<matrix4f> mview) {
	const auto idx = global_id.x;
	img_old[idx] = 0;
	
	if(idx < body_count) {
		const matrix4f mproj { matrix4f().perspective(90.0f, float(img_size.x) / float(img_size.y), 0.25f, 2500.0f) };
		
		// transform vector (*TMVP)
		const float3 position = positions[idx].xyz;
		const float3 mview_vec = position * mview;
		float3 proj_vec = mview_vec * mproj;
		
		// check if point is not behind cam
		if(mview_vec.z < 0.0f) {
			proj_vec *= -1.0f / mview_vec.z;
			
			// and finally: compute window position
			const int2 pixel {
				(int32_t)(float(img_size.x) * (proj_vec.x * 0.5f + 0.5f)),
				(int32_t)(float(img_size.y) * (proj_vec.y * 0.5f + 0.5f))
			};
			if(pixel.x < 0 || pixel.y < 0) return;
			
			const uint2 upixel { pixel };
			if(upixel.x < img_size.x && upixel.y < img_size.y) {
#if 0 // just add/override previous value, this will overflow of course
				img[upixel.y * img_size->x + upixel.x] += 0x404040;
#else // use atomics and max out at 0xFFFFFF
				uint32_t color, cur_color;
				do {
					cur_color = img[upixel.y * img_size.x + upixel.x];
					if(cur_color >= 0xFFFFFFu) break; // overflow
					color = std::min(cur_color + 0x404040u, 0xFFFFFFu);
				} while(atomic_cmpxchg(&img[upixel.y * img_size.x + upixel.x], cur_color, color) != cur_color);
#endif
			}
		}
	}
}

// shader: metal only for now
#if defined(FLOOR_COMPUTE_METAL)
static auto compute_gradient(const float& interpolator) {
	static constexpr const float4 gradients[] {
		{ 1.0f, 0.2f, 0.0f, 1.0f },
		{ 1.0f, 1.0f, 0.0f, 1.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		{ 0.5f, 1.0f, 1.0f, 1.0f }
	};
	
	// scale from [0, 1] to [0, range]
	const auto scaled_interp = interpolator * float(size(gradients));
	// determine lower gradient idx
	const auto gradient_idx = min(uint32_t(scaled_interp), uint32_t(size(gradients) - 2));
	// interp range is [0, 1] between gradients, just need to wrap/mod it
	const auto wrapped_interp = math::wrap(scaled_interp, 1.0f);
	// linear interpolation between gradients
	return gradients[gradient_idx].interpolated(gradients[gradient_idx + 1u], wrapped_interp);
}

struct uniforms_t {
	matrix4f mvpm;
	matrix4f mvm;
	float2 mass_minmax;
};

struct stage_in_out {
	float4 position [[position]];
	float size [[point_size]];
	float4 color;
};

vertex auto lighting_vertex(buffer<const float4> vertex_array,
							param<uniforms_t> uniforms) {
	const auto vid = get_vertex_id();
	const auto in_vertex = vertex_array[vid];
	float size = 128.0f / (1.0f - (float4 { in_vertex.xyz, 1.0f } * uniforms.mvm).z);
	float mass_scale = (in_vertex.w - uniforms.mass_minmax.x) / (uniforms.mass_minmax.y - uniforms.mass_minmax.x);
	mass_scale *= mass_scale; // ^2
	size *= mass_scale;
	return stage_in_out {
		.position = float4 { in_vertex.xyz, 1.0f } * uniforms.mvpm,
		.size = math::clamp(size, 2.0f, 255.0f),
		.color = compute_gradient((in_vertex.w - uniforms.mass_minmax.x) / (uniforms.mass_minmax.y - uniforms.mass_minmax.x))
	};
}

fragment auto lighting_fragment(stage_in_out in [[stage_input]],
								const_image_2d<float> tex) {
	return tex.read_linear(get_point_coord()) * in.color;
}
#endif

#endif
