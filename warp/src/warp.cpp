/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#if defined(FLOOR_COMPUTE)

// compile time defines
// SCREEN_WIDTH: screen width in px
// SCREEN_HEIGHT: screen height in px
// SCREEN_FOV: camera/projection-matrix field of view
#if !defined(SCREEN_WIDTH)
#define SCREEN_WIDTH 1280
#endif
#if !defined(SCREEN_HEIGHT)
#define SCREEN_HEIGHT 720
#endif
#if !defined(SCREEN_FOV)
#define SCREEN_FOV 72.0f
#endif

struct camera {
	// TODO: remove ugliness due to opencl/address space handling
#define screen_size (float2 { float(SCREEN_WIDTH), float(SCREEN_HEIGHT) })
#define inv_screen_size (1.0f / screen_size)
#define aspect_ratio (screen_size.x / screen_size.y)
#define up_vec const_math::tan(const_math::deg_to_rad(SCREEN_FOV) * 0.5f)
#define right_vec (up_vec * aspect_ratio)
	
	static float3 reconstruct_position(const size2& coord, const float& linear_depth) {
		return {
			(float2(coord) * 2.0f * inv_screen_size - 1.0f) * float2(right_vec, up_vec) * linear_depth,
			-linear_depth
		};
	}
	
	static float2 reproject_position(const float3& position) {
		const auto proj_dst_coord = (position.xy * float2 { 1.0f / right_vec, 1.0f / up_vec }) / -position.z;
		return ((proj_dst_coord * 0.5f + 0.5f) * screen_size).round();
	}
};

static float3 decode_motion(const uint32_t& encoded_motion) {
	const float3 signs {
		(encoded_motion & 0x80000000u) != 0u ? -1.0f : 1.0f,
		(encoded_motion & 0x40000000u) != 0u ? -1.0f : 1.0f,
		(encoded_motion & 0x20000000u) != 0u ? -1.0f : 1.0f
	};
	const uint3 shifted_motion {
		(encoded_motion >> 19u) & 0x3FFu,
		(encoded_motion >> 10u) & 0x1FFu,
		encoded_motion & 0x3FFu
	};
	constexpr const float3 adjust {
		const_math::log2(64.0f + 1.0f) / 1024.0f,
		const_math::log2(64.0f + 1.0f) / 512.0f,
		const_math::log2(64.0f + 1.0f) / 1024.0f
	};
	return signs * ((float3(shifted_motion) * adjust).exp2() - 1.0f);
}

// simple version of the warp kernel, simply reading all pixels + moving them to the predicted screen position (no checks!)
kernel void warp_scatter_simple(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> img_color,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32F> img_depth,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32UI> img_motion,
								wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> img_out_color,
								param<float> relative_delta, // "current compute/warp delta" divided by "delta between last two frames"
								param<float4> motion_override) {
	if(global_id.x >= SCREEN_WIDTH || global_id.y >= SCREEN_HEIGHT) return;
	
	const auto coord = global_id.xy;
	auto color = read(img_color, coord);
	const auto linear_depth = read(img_depth, coord); // depth is already linear with z/w in shader
	const auto motion = (motion_override.w < 0.0f ? decode_motion(read(img_motion, coord)) : motion_override.xyz);
	
	// reconstruct 3D position from depth + camera/screen setup
	const float3 reconstructed_pos = camera::reconstruct_position(coord, linear_depth);
	
	// predict/compute new 3D position from current motion and time
	const auto motion_dst = (motion_override.w < 0.0f ? *relative_delta : 1.0f) * motion;
	const auto new_pos = reconstructed_pos + motion_dst;
	// project 3D position back into 2D
	const int2 idst_coord { camera::reproject_position(new_pos) };
	
	// only write if new projected screen position is actually inside the screen
	if(idst_coord.x >= 0 && idst_coord.x < SCREEN_WIDTH &&
	   idst_coord.y >= 0 && idst_coord.y < SCREEN_HEIGHT) {
		color.w = 1.0f; // px fixup
		write(img_out_color, idst_coord, color);
	}
}

// NOTE: r/w image supported by cuda, not officially supported by opencl (works with cpu, doesn't with gpu)
kernel void single_px_fixup(
#if defined(FLOOR_COMPUTE_CUDA)
							rw_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> warp_img
#define warp_img_in warp_img
#define warp_img_out warp_img
#else
							ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> warp_img_in,
							wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> warp_img_out
#endif
							) {
	if(global_id.x >= SCREEN_WIDTH || global_id.y >= SCREEN_HEIGHT) return;
	
	const int2 coord { global_id.xy };
	const auto color = read(warp_img_in, coord);
	
	// 0 if it hasn't been written (needs fixup), 1 if it has been written
	if(color.w < 1.0f) {
		// sample pixels around
		const float4 colors[] {
			read(warp_img_in, int2 { coord.x, coord.y - 1 }),
			read(warp_img_in, int2 { coord.x + 1, coord.y }),
			read(warp_img_in, int2 { coord.x, coord.y + 1 }),
			read(warp_img_in, int2 { coord.x - 1, coord.y }),
		};
		
		float3 avg;
		float sum = 0.0f;
		for(const auto& col : colors) {
			if(col.w == 1.0f) {
				avg += col.xyz;
				sum += 1.0f;
			}
		}
		avg /= sum;
		
		// write new averaged color
		write(warp_img_out, coord, float4 { avg, 0.0f });
	}
}

kernel void img_clear(wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> img,
					  param<float4> clear_color) {
	if(global_id.x >= SCREEN_WIDTH || global_id.y >= SCREEN_HEIGHT) return;
	write(img, int2 { global_id.xy }, float4 { (*clear_color).xyz, 0.0f });
}

#endif
