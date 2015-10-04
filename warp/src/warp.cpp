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

// if tiles (work-group x/y sizes) overlap the screen size, a check is neccesary to ignore the overlapping work-items
#if (((SCREEN_WIDTH / TILE_SIZE_X) * TILE_SIZE_X) != SCREEN_WIDTH) || (((SCREEN_HEIGHT / TILE_SIZE_Y) * TILE_SIZE_Y) != SCREEN_HEIGHT)
#define screen_check() if(global_id.x >= SCREEN_WIDTH || global_id.y >= SCREEN_HEIGHT) return
#else
#define screen_check() /* nop */
#endif

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
	template <depth_type type = DEFAULT_DEPTH_TYPE>
	constexpr static float linearize_depth(const float& depth) {
		if(type == depth_type::normalized) {
			// reading from the actual depth buffer which is normalized in [0, 1]
			constexpr const float2 near_far_projection {
				-(near_far_plane.y + near_far_plane.x) / (near_far_plane.x - near_far_plane.y),
				(2.0f * near_far_plane.y * near_far_plane.x) / (near_far_plane.x - near_far_plane.y),
			};
			// ignore full depth (this happens when clear depth == 1.0f and no fragments+depth are written for a pixel)
			if(depth == 1.0f) return 0.0f;
			return near_far_projection.y / (depth - near_far_projection.x);
		}
		else if(type == depth_type::z_div_w) {
			// depth is written as z/w in shader -> need to perform a small adjustment to account for near/far plane to get the real world depth
			// (note that this error is almost imperceptible and could just be ignored)
			return depth + near_far_plane.x - (depth * (near_far_plane.x / near_far_plane.y));
		}
		else if(type == depth_type::log) {
			// TODO: implement this
			return 0.0f;
		}
		else if(type == depth_type::linear) {
			// already linear, just pass through
			return depth;
		}
		floor_unreachable();
	}
};

// decodes the encoded input 3D motion vector
// format: [1-bit sign x][1-bit sign y][1-bit sign z][10-bit x][9-bit y][10-bit z]
static float3 decode_3d_motion(const uint32_t& encoded_motion) {
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

// decodes the encoded input 2D motion vector
// format: [16-bit x][16-bit y]
static float2 decode_2d_motion(const uint32_t& encoded_motion) {
	const uint2 shifted_motion {
		(encoded_motion >> 16u) & 0xFFFFu,
		encoded_motion & 0xFFFFu
	};
	// map [0, 65535] -> [0, 1] -> [-1, 1] -> [-width|-height, width|height]
	return ((float2(shifted_motion) * (2.0f / 65535.0f)) - 1.0f) * warp_camera::screen_size;
}

// simple version of the warp kernel, simply reading all pixels + moving them to the predicted screen position (no checks!)
kernel void warp_scatter_simple(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_color,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | COMPUTE_IMAGE_TYPE::D32F> img_depth,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32UI> img_motion,
								wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_out_color,
								param<float> relative_delta, // "current compute/warp delta" divided by "delta between last two frames"
								param<float4> motion_override) {
	screen_check();
	
	const auto coord = global_id.xy;
	auto color = read(img_color, coord);
	const auto linear_depth = warp_camera::linearize_depth(read(img_depth, coord));
	const auto motion = (motion_override.w < 0.0f ? decode_3d_motion(read(img_motion, coord)) : motion_override.xyz);
	
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

// TODO/wip
static constexpr const uint32_t depth_groups { 8u };
static constexpr const float3 dbg_colors[depth_groups] {
	float3 { 1.0f, 0.0f, 0.0f },
	float3 { 0.0f, 1.0f, 0.0f },
	float3 { 0.0f, 0.0f, 1.0f },
	float3 { 1.0f, 1.0f, 0.0f },
	float3 { 1.0f, 0.0f, 1.0f },
	float3 { 0.0f, 1.0f, 1.0f },
	float3 { 1.0f, 1.0f, 1.0f },
	float3 { 0.5f, 0.5f, 0.5f },
};
kernel void warp_scatter_patch(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_color,
							   ro_image<COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | COMPUTE_IMAGE_TYPE::D32F> img_depth,
							   ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32UI> img_motion,
							   wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_out_color,
							   param<float> relative_delta, // "current compute/warp delta" divided by "delta between last two frames"
							   param<float4> motion_override) {
	screen_check();
	
	const auto coord = global_id.xy;
	const auto linear_depth = warp_camera::linearize_depth(read(img_depth, coord));
	
	//
	local_buffer<float, 2> d_min_max;
	local_buffer<uint32_t, depth_groups, 4> rect_min_max;
	if(local_id.x == 0 && local_id.y == 0) {
		d_min_max[0] = numeric_limits<float>::max();
		d_min_max[1] = -numeric_limits<float>::max();
	}
	if(local_id.x < depth_groups) {
		rect_min_max[local_id.x][0] = TILE_SIZE_X;
		rect_min_max[local_id.x][1] = TILE_SIZE_Y;
		rect_min_max[local_id.x][2] = 0;
		rect_min_max[local_id.x][3] = 0;
	}
	local_barrier();
	
	// find min/max linear depth for this tile
	atomic_min(&d_min_max[0], linear_depth);
	atomic_max(&d_min_max[1], linear_depth);
	local_barrier();
	
	const float d_min = d_min_max[0];
	const float d_max = d_min_max[1];
	
	// classify depth, max 8 depth groups
	const float range = d_max - d_min;
	const float step = range * (1.0f / float(depth_groups));
	const float classified_depth = (linear_depth - d_min) / std::max(step, warp_camera::near_far_plane.y / 50.0f);
	const uint32_t idx = std::min(uint32_t(classified_depth), depth_groups - 1u);
	const auto color = dbg_colors[idx];
	
	// find rect min/max coordinates for this group
	atomic_min(&rect_min_max[idx][0], local_id.x);
	atomic_min(&rect_min_max[idx][1], local_id.y);
	atomic_max(&rect_min_max[idx][2], local_id.x);
	atomic_max(&rect_min_max[idx][3], local_id.y);
	local_barrier();
	
	//
	//float3 color;
	write(img_out_color, coord, float4 { color, 1.0f });
}

kernel void warp_gather(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_color,
						ro_image<COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | COMPUTE_IMAGE_TYPE::D32F> img_depth,
						ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_color_prev,
						ro_image<COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | COMPUTE_IMAGE_TYPE::D32F> img_depth_prev,
						ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32UI> img_motion_forward,
						ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32F> img_motion_depth_forward,
						ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32UI> img_motion_backward,
						ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32F> img_motion_depth_backward,
						wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_out_color,
						param<float> relative_delta, // "current compute/warp delta" divided by "delta between last two frames"
						param<float> epsilon_1,
						param<float> epsilon_2
) {
	screen_check();
	
	const auto coord = global_id.xy;
	
	// iterate
	const float2 fcoord = float2 { coord } + 0.5f; // start at pixel center (this is p_t+alpha)
#if 0 // basic, both start at p_t+alpha
	float2 p_fwd = fcoord;
	float2 p_bwd = fcoord;
#else // dual init, opposing init
	float2 p_fwd = fcoord + relative_delta * decode_2d_motion(read(img_motion_backward, coord));
	float2 p_bwd = fcoord + (1.0f - relative_delta) * decode_2d_motion(read(img_motion_forward, coord));
#endif
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(read(img_motion_forward, int2(p_fwd.floored())));
		const auto d = relative_delta * motion;
		p_fwd = fcoord - d;
	}
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(read(img_motion_backward, int2(p_bwd.floored())));
		const auto d = (1.0f - relative_delta) * motion;
		p_bwd = fcoord - d;
	}
	
	//
	const auto color_fwd = read(img_color_prev, int2(p_fwd.floored()));
	const auto color_bwd = read(img_color, int2(p_bwd.floored()));
	
	const auto motion_fwd = decode_2d_motion(read(img_motion_forward, int2(p_fwd.floored())));
	const auto motion_bwd = decode_2d_motion(read(img_motion_backward, int2(p_bwd.floored())));
	
	const auto err_fwd = (p_fwd + relative_delta * motion_fwd - fcoord).dot();
	const auto err_bwd = (p_bwd + (1.0f - relative_delta) * motion_bwd - fcoord).dot();
	//constexpr const float epsilon_1 { 8.0f }; // aka "max offset in px"
	//constexpr const float epsilon_1_sq { epsilon_1 * epsilon_1 };
	const float epsilon_1_sq { epsilon_1 * epsilon_1 };
	
	const auto z_fwd = (warp_camera::linearize_depth(read(img_depth_prev, int2(p_fwd.floored()))) +
						relative_delta * warp_camera::linearize_depth<depth_type::z_div_w>(read(img_motion_depth_forward,
																								int2(p_fwd.floored()))));
	const auto z_bwd = (warp_camera::linearize_depth(read(img_depth, int2(p_bwd.floored()))) +
						(1.0f - relative_delta) * warp_camera::linearize_depth<depth_type::z_div_w>(read(img_motion_depth_backward,
																										 int2(p_bwd.floored()))));
	const auto depth_diff = fabs(z_fwd - z_bwd);
	//constexpr const float epsilon_2 { 4.0f }; // aka "max depth difference between fwd and bwd"
	
	const bool fwd_valid = (err_fwd < epsilon_1_sq);
	const bool bwd_valid = (err_bwd < epsilon_1_sq);
#if 0 // just blended
	const auto color = color_fwd.interpolated(color_bwd, relative_delta);
#elif 0 // dbg
	//const auto color = color_fwd;
	//const auto color = color_bwd;
	//const auto color = ((1.0f - relative_delta) * color_fwd + relative_delta * read(img_color, int2((p_fwd + motion_fwd).floored())));;
	//const auto color = float4 { decode_2d_motion(read(img_motion_forward, coord)), 0.0f, 1.0f };
	const auto color = float4 { float3 { z_fwd }, 1.0f };
	//const auto color = float4 { float3 { fabs(read(img_motion_depth_forward, int2(p_fwd.floored()))) }, 1.0f };
#elif 1 // as in paper
	float4 color;
	const auto proj_color_fwd = ((1.0f - relative_delta) * color_fwd +
								 relative_delta * read(img_color, int2((p_fwd + motion_fwd).floored())));;
	const auto proj_color_bwd = ((1.0f - relative_delta) * read(img_color_prev, int2((p_bwd + motion_bwd).floored())) +
								 relative_delta * color_bwd);
	if(fwd_valid && bwd_valid) {
		if(depth_diff < epsilon_2) {
			// case 1: both fwd and bwd are valid
			if(err_fwd < err_bwd) {
				color = proj_color_fwd;
			}
			else {
				color = proj_color_bwd;
			}
		}
		else {
			// case 2: select the one closer to the camera (occlusion)
			if(z_fwd < z_bwd) {
				// depth from other frame
				const auto z_fwd_other = read(img_depth, int2((p_fwd + motion_fwd).floored())) + (1.0f - relative_delta) * read(img_motion_depth_backward, int2((p_fwd + motion_fwd).floored()));
				color = (fabs(z_fwd - z_fwd_other) < epsilon_2 ? proj_color_fwd : color_fwd);
			}
			else { // bwd < fwd
				const auto z_bwd_other = read(img_depth_prev, int2((p_bwd + motion_bwd).floored())) + relative_delta * read(img_motion_depth_forward, int2((p_bwd + motion_bwd).floored()));
				color = (fabs(z_bwd - z_bwd_other) < epsilon_2 ? proj_color_bwd : color_bwd);
			}
		}
	}
	else if(fwd_valid) {
		color = proj_color_fwd;
	}
	else if(bwd_valid) {
		color = proj_color_bwd;
	}
	// case 3 / else: both are invalid
	else {
		color = proj_color_fwd.interpolated(proj_color_bwd, relative_delta);
	}
#endif
	
	write(img_out_color, coord, color);
}

kernel void single_px_fixup(rw_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> warp_img) {
	screen_check();
	
	const int2 coord { global_id.xy };
	const auto color = read(warp_img, coord);
	
	// 0 if it hasn't been written (needs fixup), 1 if it has been written
	if(color.w < 1.0f) {
		// sample pixels around
		const float4 colors[] {
			read(warp_img, int2 { coord.x, coord.y - 1 }),
			read(warp_img, int2 { coord.x + 1, coord.y }),
			read(warp_img, int2 { coord.x, coord.y + 1 }),
			read(warp_img, int2 { coord.x - 1, coord.y }),
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
		write(warp_img, coord, float4 { avg, 0.0f });
	}
}

kernel void img_clear(wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img,
					  param<float4> clear_color) {
	screen_check();
	write(img, int2 { global_id.xy }, float4 { clear_color.xyz, 0.0f });
}

#endif
