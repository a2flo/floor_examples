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

#include "warp.hpp"

#if defined(FLOOR_COMPUTE)

namespace warp_camera {
	// sceen size in fp
	static constexpr const float2 screen_size { float(SCREEN_WIDTH), float(SCREEN_HEIGHT) };
	// 1 / screen size in fp
	static constexpr const float2 inv_screen_size { 1.0f / screen_size };
	// screen width / height aspect ratio
	static constexpr const float aspect_ratio { screen_size.x / screen_size.y };
	// projection up vector
	static constexpr const float _up_vec { const_math::tan(const_math::deg_to_rad(SCREEN_FOV) * 0.5f) };
	// projection right vector
	static constexpr const float right_vec { _up_vec * aspect_ratio };
#if defined(SCREEN_ORIGIN_LEFT_BOTTOM)
	static constexpr const float up_vec { _up_vec };
#else // flip up vector for "left top" origin
	static constexpr const float up_vec { -_up_vec };
#endif
	// [near, far] plane, needed for depth correction
	constexpr const float2 near_far_plane { 0.5f, 500.0f };
	
	// reconstructs a 3D position from a 2D screen coordinate and its associated real world depth
	static float3 reconstruct_position(const uint2& coord, const float& linear_depth) {
		return {
			((float2(coord) + 0.5f) * 2.0f * inv_screen_size - 1.0f) * float2(right_vec, up_vec) * linear_depth,
			-linear_depth
		};
	}
	
	// reprojects a 3D position back to 2D
	static float2 reproject_position(const float3& position) {
		const auto proj_dst_coord = (position.xy * float2 { 1.0f / right_vec, 1.0f / up_vec }) / -position.z;
		return ((proj_dst_coord * 0.5f + 0.5f) * screen_size).floor();
	}
	
	// linearizes the input depth value according to the depth type and returns the real world depth value
	static float linearize_depth(const float& depth) {
#if defined(DEPTH_ZW)
		// depth is written as z/w in shader -> need to perform a small adjustment to account for near/far plane to get the real world depth
		// (note that this error is almost imperceptible and could just be ignored)
		return depth + near_far_plane.x - (depth * (near_far_plane.x / near_far_plane.y));
		
#elif defined(DEPTH_NORMALIZED)
		// reading from the actual depth buffer which is normalized in [0, 1]
		constexpr const float2 near_far_projection {
			-(near_far_plane.y + near_far_plane.x) / (near_far_plane.x - near_far_plane.y),
			(2.0f * near_far_plane.y * near_far_plane.x) / (near_far_plane.x - near_far_plane.y),
		};
		return near_far_projection.y / (depth - near_far_projection.x);
#endif
	}
};

// decodes the encoded input 3D motion vector
// format: [1-bit sign x][1-bit sign y][1-bit sign z][10-bit x][9-bit y][10-bit z]
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

// depth buffer format: actual depth on metal, color (red-only) on opencl - either will work for cuda and host (identical sampling)
constexpr const COMPUTE_IMAGE_TYPE depth_image_format {
#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_HOST)
	COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | COMPUTE_IMAGE_TYPE::D32F
#else
	COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32F
#endif
};

// simple version of the warp kernel, simply reading all pixels + moving them to the predicted screen position (no checks!)
kernel void warp_scatter_simple(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_color,
								ro_image<depth_image_format> img_depth,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32UI> img_motion,
								wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_out_color,
								param<float> relative_delta, // "current compute/warp delta" divided by "delta between last two frames"
								param<float4> motion_override) {
	if(global_id.x >= SCREEN_WIDTH || global_id.y >= SCREEN_HEIGHT) return;
	
	const auto coord = global_id.xy;
	auto color = read(img_color, coord);
	const auto linear_depth = warp_camera::linearize_depth(read(img_depth, coord));
	const auto motion = (motion_override.w < 0.0f ? decode_motion(read(img_motion, coord)) : motion_override.xyz);
	
	// reconstruct 3D position from depth + camera/screen setup
	const float3 reconstructed_pos = warp_camera::reconstruct_position(coord, linear_depth);
	
	// predict/compute new 3D position from current motion and time
	const auto motion_dst = (motion_override.w < 0.0f ? relative_delta : 1.0f) * motion;
	const auto new_pos = reconstructed_pos + motion_dst;
	// project 3D position back into 2D
	const int2 idst_coord { warp_camera::reproject_position(new_pos) };
	
	// only write if new projected screen position is actually inside the screen
	if(idst_coord.x >= 0 && idst_coord.x < SCREEN_WIDTH &&
	   idst_coord.y >= 0 && idst_coord.y < SCREEN_HEIGHT) {
		color.w = 1.0f; // px fixup
		write(img_out_color, idst_coord, color);
	}
}

kernel void single_px_fixup(
#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_HOST) || defined(FLOOR_COMPUTE_METAL)
							rw_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> warp_img
#define warp_img_in warp_img
#define warp_img_out warp_img
#else
							ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> warp_img_in,
							wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> warp_img_out
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

kernel void img_clear(wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img,
					  param<float4> clear_color) {
	if(global_id.x >= SCREEN_WIDTH || global_id.y >= SCREEN_HEIGHT) return;
	write(img, int2 { global_id.xy }, float4 { clear_color.xyz, 0.0f });
}

#endif
