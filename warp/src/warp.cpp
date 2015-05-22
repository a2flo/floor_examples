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

// simple version of the warp kernel, simply reading all pixels + moving them to the predicted screen position (no checks!)
kernel void warp_scatter_simple(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> img_color,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32F> img_depth,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA32F> img_motion,
								wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> img_out_color,
								param<float> relative_delta, // "current compute/warp delta" divided by "delta between last two frames"
								param<float4> motion_override) {
	if(global_id.x >= SCREEN_WIDTH || global_id.y >= SCREEN_HEIGHT) return;
	
	const int2 coord { global_id.xy };
	auto color = read(img_color, coord);
	const auto linear_depth = read(img_depth, coord).x; // depth is already linear with z/w in shader
	const auto motion = (motion_override.w < 0.0f ? read(img_motion, coord).xyz : motion_override.xyz);
	
	// reconstruct 3D position from depth + camera/screen setup
	constexpr const float2 screen_size { float(SCREEN_WIDTH), float(SCREEN_HEIGHT) };
	constexpr const float2 inv_screen_size { 1.0f / screen_size };
	constexpr const float aspect_ratio { screen_size.x / screen_size.y };
	constexpr const float up_vec = const_math::tan(const_math::deg_to_rad(SCREEN_FOV) * 0.5f);
	constexpr const float right_vec = up_vec * aspect_ratio;
	
	const float3 reconstructed_pos {
		(float2(coord) * 2.0f * inv_screen_size - 1.0f) * float2(right_vec, up_vec) * linear_depth,
		-linear_depth
	};
	
	// predict/compute new 3D position from current motion and time
	const auto motion_dst = (motion_override.w < 0.0f ? *relative_delta : 1.0f) * motion;
	const auto new_pos = reconstructed_pos + motion_dst;
	// project 3D position back into 2D
	const auto proj_dst_coord = (new_pos.xy * float2 { 1.0f / right_vec, 1.0f / up_vec }) / -new_pos.z;
	const auto dst_coord = ((proj_dst_coord * 0.5f + 0.5f) * screen_size).round();
	
	// only write if new projected screen position is actually inside the screen
	if(dst_coord.x >= 0.0f && dst_coord.x < screen_size.x &&
	   dst_coord.y >= 0.0f && dst_coord.y < screen_size.y) {
		color.w = 1.0f; // px fixup
		write(img_out_color, int2 { int(dst_coord.x), int(dst_coord.y) }, color);
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
	const float4 color = read(warp_img_in, coord);
	
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
