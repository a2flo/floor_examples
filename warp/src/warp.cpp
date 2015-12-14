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
	static constexpr const float2 near_far_plane { WARP_NEAR_PLANE, WARP_FAR_PLANE };
	
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
		return ((proj_dst_coord * 0.5f + 0.5f) * screen_size);
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
			// special case: clear/full depth, assume this comes from a normalized sky box
			if(depth == 1.0f) return 1.0f;
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

// used by decode_3d_motion
static constexpr const float3 signs_lookup[8] {
	float3 { 1.0f, 1.0f, 1.0f },
	float3 { 1.0f, 1.0f, -1.0f },
	float3 { 1.0f, -1.0f, 1.0f },
	float3 { 1.0f, -1.0f, -1.0f },
	float3 { -1.0f, 1.0f, 1.0f },
	float3 { -1.0f, 1.0f, -1.0f },
	float3 { -1.0f, -1.0f, 1.0f },
	float3 { -1.0f, -1.0f, -1.0f },
};

// decodes the encoded input 3D motion vector
// format: [1-bit sign x][1-bit sign y][1-bit sign z][10-bit x][9-bit y][10-bit z]
floor_inline_always static float3 decode_3d_motion(const uint32_t& encoded_motion) {
	// lookup into constant memory + 1 shift is faster than 3 ANDs + 3 cmps/sels
	const float3 signs = signs_lookup[encoded_motion >> 29u];
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

// computes the "scattered" destination coordinate of the pixel at 'coord',
// according to it's depth value (which is also returned) and motion vector, as well as the current time delta
floor_inline_always static auto scatter(const int2& coord,
										const float& relative_delta,
										const_image_2d_depth<float> img_depth,
										const_image_2d<uint1> img_motion) {
	// read rendered/input depth and linearize it (linear distance from the camera origin)
	const auto linear_depth = warp_camera::linearize_depth(img_depth.read(coord));
	// get 3d motion for this pixel
	const auto motion = decode_3d_motion(img_motion.read(coord));
	// reconstruct 3D position from depth + camera/screen setup,
	// then predict/compute new 3D position from current motion and time
	const auto new_pos = warp_camera::reconstruct_position(coord, linear_depth) + relative_delta * motion;
	// -> return
	const struct {
		const int2 coord;
		const float linear_depth;
	} ret {
		// project 3D position back into 2D
		.coord = warp_camera::reproject_position(new_pos),
		.linear_depth = linear_depth
	};
	return ret;
}

// simple version of the warp kernel, simply reading all pixels + moving them to the predicted screen position (no checks!)
kernel void warp_scatter_simple(const_image_2d<float> img_color,
								const_image_2d_depth<float> img_depth,
								const_image_2d<uint1> img_motion,
								image_2d<float4, true> img_out_color,
								// "current compute/warp delta" divided by "delta between last two frames"
								param<float> relative_delta) {
	screen_check();
	
	const auto coord = global_id.xy;
	auto color = img_color.read(coord);
	const auto scattered = scatter(coord, relative_delta, img_depth, img_motion);
	
	// only write if new projected screen position is actually inside the screen
	if(scattered.coord.x >= 0 && scattered.coord.x < SCREEN_WIDTH &&
	   scattered.coord.y >= 0 && scattered.coord.y < SCREEN_HEIGHT) {
		color.w = 1.0f; // px fixup
		img_out_color.write(scattered.coord, color);
	}
}

//
kernel void warp_scatter_depth(const_image_2d_depth<float> img_depth,
							   const_image_2d<uint1> img_motion,
							   buffer<float> depth_buffer,
							   param<float> relative_delta) {
	screen_check();
	
	const auto scattered = scatter(global_id.xy, relative_delta, img_depth, img_motion);
	if(scattered.coord.x >= 0 && scattered.coord.x < SCREEN_WIDTH &&
	   scattered.coord.y >= 0 && scattered.coord.y < SCREEN_HEIGHT) {
		atomic_min(&depth_buffer[scattered.coord.y * SCREEN_WIDTH + scattered.coord.x], scattered.linear_depth);
	}
}
//
kernel void warp_scatter_color(const_image_2d<float> img_color,
							   const_image_2d_depth<float> img_depth,
							   const_image_2d<uint1> img_motion,
							   image_2d<float4, true> img_out_color,
							   buffer<const float> depth_buffer,
							   param<float> relative_delta) {
	screen_check();
	
	const auto coord = global_id.xy;
	const auto scattered = scatter(coord, relative_delta, img_depth, img_motion);
	if(scattered.coord.x < 0 || scattered.coord.x >= SCREEN_WIDTH ||
	   scattered.coord.y < 0 || scattered.coord.y >= SCREEN_HEIGHT) {
		return;
	}
	
	const auto dst_depth = depth_buffer[scattered.coord.y * SCREEN_WIDTH + scattered.coord.x];
	if(scattered.linear_depth > dst_depth) {
		return;
	}
	
	auto color = img_color.read(coord);
	color.w = 1.0f; // px fixup
	img_out_color.write(scattered.coord, color);
}

//
kernel void warp_scatter_bidir_depth(const_image_2d_depth<float> img_depth,
									 const_image_2d_depth<float> img_depth_prev,
									 const_image_2d<uint1> img_motion,
									 const_image_2d<uint1> img_motion_prev,
									 buffer<float> depth_buffer,
									 param<float> relative_delta) {
	screen_check();
	
	const auto scattered = scatter(global_id.xy, relative_delta, img_depth, img_motion);
	if(scattered.coord.x >= 0 && scattered.coord.x < SCREEN_WIDTH &&
	   scattered.coord.y >= 0 && scattered.coord.y < SCREEN_HEIGHT) {
		atomic_min(&depth_buffer[scattered.coord.y * SCREEN_WIDTH + scattered.coord.x], scattered.linear_depth);
	}
	
	const auto scattered_prev = scatter(global_id.xy, relative_delta, img_depth_prev, img_motion_prev);
	if(scattered_prev.coord.x >= 0 && scattered_prev.coord.x < SCREEN_WIDTH &&
	   scattered_prev.coord.y >= 0 && scattered_prev.coord.y < SCREEN_HEIGHT) {
		atomic_min(&depth_buffer[scattered_prev.coord.y * SCREEN_WIDTH + scattered_prev.coord.x], scattered_prev.linear_depth);
	}
}
//
kernel void warp_scatter_bidir_color(const_image_2d<float> img_color,
									 const_image_2d<float> img_color_prev,
									 const_image_2d_depth<float> img_depth,
									 const_image_2d_depth<float> img_depth_prev,
									 const_image_2d<uint1> img_motion,
									 const_image_2d<uint1> img_motion_prev,
									 image_2d<float4, true> img_out_color,
									 buffer<const float> depth_buffer,
									 param<float> relative_delta) {
	screen_check();
	
	const auto coord = global_id.xy;
	const auto scattered = scatter(coord, relative_delta, img_depth, img_motion);
	const auto scattered_prev = scatter(coord, relative_delta, img_depth_prev, img_motion_prev);
	
	bool fwd_valid = (scattered.coord.x >= 0 && scattered.coord.x < SCREEN_WIDTH &&
					  scattered.coord.y >= 0 && scattered.coord.y < SCREEN_HEIGHT);
	bool bwd_valid = (scattered_prev.coord.x >= 0 && scattered_prev.coord.x < SCREEN_WIDTH &&
					  scattered_prev.coord.y >= 0 && scattered_prev.coord.y < SCREEN_HEIGHT);
	
	if(fwd_valid) {
		fwd_valid &= (depth_buffer[scattered.coord.y * SCREEN_WIDTH + scattered.coord.x] <= scattered.linear_depth);
	}
	if(bwd_valid) {
		bwd_valid &= (depth_buffer[scattered_prev.coord.y * SCREEN_WIDTH + scattered_prev.coord.x] <= scattered_prev.linear_depth);
	}
	
	if(fwd_valid && bwd_valid) {
		if(scattered.coord.x == scattered_prev.coord.x &&
		   scattered.coord.y == scattered_prev.coord.y) {
			if(scattered.linear_depth <= scattered_prev.linear_depth) {
				bwd_valid = false;
				fwd_valid = true;
			}
			else {
				bwd_valid = true;
				fwd_valid = false;
			}
		}
	}
	
	if(fwd_valid) {
		auto color = img_color.read(coord);
		color.w = 1.0f; // px fixup
		img_out_color.write(scattered.coord, color);
	}
	if(bwd_valid) {
		auto color = img_color_prev.read(coord);
		color.w = 1.0f; // px fixup
		img_out_color.write(scattered_prev.coord, color);
	}
}

// decodes the encoded input 2D motion vector
// format: [16-bit y][16-bit x]
static float2 decode_2d_motion(const uint32_t& encoded_motion) {
	const union {
		ushort2 us16;
		short2 s16;
	} shifted_motion {
		.us16 = {
			encoded_motion & 0xFFFFu,
			(encoded_motion >> 16u) & 0xFFFFu
		}
	};
	// map [-32767, 32767] -> [-0.5, 0.5]
#if defined(SCREEN_ORIGIN_LEFT_BOTTOM)
	return float2(shifted_motion.s16) * (0.5f / 32767.0f);
#else // if the origin is at the top left, the y component points in the opposite/wrong direction
	return float2(shifted_motion.s16) * float2 { 0.5f / 32767.0f, -0.5f / 32767.0f };
#endif
}

kernel void warp_gather(const_image_2d<float> img_color,
						const_image_2d_depth<float> img_depth,
						const_image_2d<float> img_color_prev,
						const_image_2d_depth<float> img_depth_prev,
						const_image_2d<uint1> img_motion_forward,
						const_image_2d<uint1> img_motion_backward,
						// packed <forward depth: fwd t-1 -> t (used here), backward depth: bwd t-1 -> t-2 (unused here)>
						const_image_2d<float2> img_motion_depth_forward,
						// packed <forward depth: t+1 -> t (unused here), backward depth: t -> t-1 (used here)>
						const_image_2d<float2> img_motion_depth_backward,
						image_2d<float4, true> img_out_color,
						param<float> relative_delta, // "current compute/warp delta" divided by "delta between last two frames"
						param<float> epsilon_1,
						param<float> epsilon_2,
						param<uint32_t> dbg_render_type) {
	screen_check();
	
	// iterate
	const float2 p_init = (float2(global_id.xy) + 0.5f) * warp_camera::inv_screen_size; // start at pixel center (this is p_t+alpha)
	// dual init, opposing init
	float2 p_fwd = p_init + relative_delta * decode_2d_motion(img_motion_backward.read(p_init));
	float2 p_bwd = p_init + (1.0f - relative_delta) * decode_2d_motion(img_motion_forward.read(p_init));
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(img_motion_forward.read(p_fwd));
		p_fwd = p_init - relative_delta * motion;
	}
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(img_motion_backward.read(p_bwd));
		p_bwd = p_init - (1.0f - relative_delta) * motion;
	}
	
	//
	const auto color_fwd = img_color_prev.read(p_fwd);
	const auto color_bwd = img_color.read(p_bwd);
	
	// read final motion vector + depth (packed)
	const auto motion_fwd = decode_2d_motion(img_motion_forward.read(p_fwd));
	const auto motion_bwd = decode_2d_motion(img_motion_backward.read(p_bwd));
	const auto depth_fwd = img_motion_depth_forward.read(p_fwd).x;
	const auto depth_bwd = img_motion_depth_backward.read(p_bwd).y;
	
	// compute screen space error
	const auto err_fwd = ((p_fwd + relative_delta * motion_fwd - p_init).dot() +
						  // account for out-of-bound access (-> large error so any checks will fail)
						  ((p_fwd < 0.0f).any() || (p_fwd > 1.0f).any() ? 1e10f : 0.0f));
	const auto err_bwd = ((p_bwd + (1.0f - relative_delta) * motion_bwd - p_init).dot() +
						  ((p_bwd < 0.0f).any() || (p_bwd > 1.0f).any() ? 1e10f : 0.0f));
	// TODO: should have a more tangible epsilon, e.g. max pixel offset -> (max_offset / screen_size).max_element()
	const float epsilon_1_sq { epsilon_1 * epsilon_1 };
	
	// NOTE: scene depth type is dependent on the renderer (-> use the default), motion depth is always z/w
	// -> need to linearize both to properly add + compare them
	const auto z_fwd = (warp_camera::linearize_depth(img_depth_prev.read(p_fwd)) +
						relative_delta * warp_camera::linearize_depth<depth_type::z_div_w>(depth_fwd));
	const auto z_bwd = (warp_camera::linearize_depth(img_depth.read(p_bwd)) +
						(1.0f - relative_delta) * warp_camera::linearize_depth<depth_type::z_div_w>(depth_bwd));
	const auto depth_diff = abs(z_fwd - z_bwd);
	//constexpr const float epsilon_2 { 4.0f }; // aka "max depth difference between fwd and bwd"
	
	const bool fwd_valid = (err_fwd < epsilon_1_sq);
	const bool bwd_valid = (err_bwd < epsilon_1_sq);
	float4 color;
#if 0
	if(dbg_render_type > 0) { // dbg rendering
		if(dbg_render_type == 1) color = color_fwd;
		else if(dbg_render_type == 2) color = color_bwd;
		else if(dbg_render_type == 3) {
			color = img_color_prev.read(p_init);
		}
		else if(dbg_render_type == 4) {
			color = img_color.read(p_init);
		}
		else if(dbg_render_type == 5) {
			color = ((fwd_valid && bwd_valid) ? float4 { 1.0f } :
					 (fwd_valid ? float4 { 0.0f, 1.0f, 0.0f, 1.0f} :
					  (bwd_valid ? float4 { 1.0f, 0.0f, 0.0f, 1.0f} : float4 { 0.0f } )));
		}
	}
	else
#endif
	{
		const auto proj_color_fwd = color_fwd.interpolated(img_color.read(p_fwd + motion_fwd), relative_delta);
		const auto proj_color_bwd = img_color_prev.read(p_bwd + motion_bwd).interpolated(color_bwd, relative_delta);
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
					const auto z_fwd_other = (img_depth.read(p_fwd + motion_fwd) +
											  (1.0f - relative_delta) * img_motion_depth_backward.read(p_fwd + motion_fwd).y);
					color = (abs(z_fwd - z_fwd_other) < epsilon_2 ? proj_color_fwd : color_fwd);
				}
				else { // bwd < fwd
					const auto z_bwd_other = (img_depth_prev.read(p_bwd + motion_bwd) +
											  relative_delta * img_motion_depth_forward.read(p_bwd + motion_bwd).x);
					color = (abs(z_bwd - z_bwd_other) < epsilon_2 ? proj_color_bwd : color_bwd);
				}
			}
		}
		else if(fwd_valid) {
			color = color_fwd;
		}
		else if(bwd_valid) {
			color = color_bwd;
		}
		// case 3 / else: both are invalid -> just do a linear interpolation between the two
		else {
			color = color_fwd.interpolated(color_bwd, relative_delta);
		}
	}
	
	img_out_color.write(global_id.xy, color);
}

kernel void single_px_fixup(image_2d<float4> warp_img) {
	screen_check();
	
	const int2 coord { global_id.xy };
	const auto color = warp_img.read(coord);
	
	// 0 if it hasn't been written (needs fixup), 1 if it has been written
	if(color.w < 1.0f) {
		// sample pixels around
		const float4 colors[] {
			warp_img.read(int2 { coord.x, coord.y - 1 }),
			warp_img.read(int2 { coord.x + 1, coord.y }),
			warp_img.read(int2 { coord.x, coord.y + 1 }),
			warp_img.read(int2 { coord.x - 1, coord.y }),
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
		warp_img.write(coord, float4 { avg, 0.0f });
	}
}

kernel void img_clear(image_2d<float4, true> img,
					  param<float4> clear_color) {
	screen_check();
	img.write(global_id.xy, float4 { clear_color.xyz, 0.0f });
}

#endif
