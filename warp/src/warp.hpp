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

#ifndef __FLOOR_WARP_WARP_HPP__
#define __FLOOR_WARP_WARP_HPP__

#include <floor/core/essentials.hpp>

#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

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

// prototypes
kernel void warp_scatter_simple(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_color,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32F> img_depth,
								ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::R32UI> img_motion,
								wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img_out_color,
								param<float> relative_delta,
								param<float4> motion_override);

kernel void single_px_fixup(
#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_HOST)
							rw_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> warp_img
#else
							ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> warp_img_in,
							wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> warp_img_out
#endif
);

kernel void img_clear(wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8> img,
					  param<float4> clear_color);

#endif
