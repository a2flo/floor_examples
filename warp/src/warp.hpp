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

// compile time defines
// SCREEN_WIDTH: screen width in px
// SCREEN_HEIGHT: screen height in px
// SCREEN_FOV: camera/projection-matrix field of view
// WARP_NEAR_PLANE: near plane
// WARP_FAR_PLANE: far plane
#if !defined(SCREEN_WIDTH)
#define SCREEN_WIDTH 1280
#endif
#if !defined(SCREEN_HEIGHT)
#define SCREEN_HEIGHT 720
#endif
#if !defined(SCREEN_FOV)
#define SCREEN_FOV 72.0f
#endif
#if !defined(WARP_NEAR_PLANE)
#define WARP_NEAR_PLANE 0.5f
#endif
#if !defined(WARP_FAR_PLANE)
#define WARP_FAR_PLANE 500.0f
#endif

// screen origin is left bottom for opengl, left top for metal (and directx)
#if !defined(FLOOR_COMPUTE_METAL)
#define SCREEN_ORIGIN_LEFT_BOTTOM 1
#else
#define SCREEN_ORIGIN_LEFT_TOP 1
#endif

// depth buffer types
enum class depth_type {
	// normalized in [0, 1], default for opengl and metal
	normalized,
	// z/w depth
	z_div_w,
	// log depth, computed in software
	// NOTE/TODO: not supported yet
	log,
	// linear depth [0, far-plane]
	linear,
};

#if !defined(DEFAULT_DEPTH_TYPE)
#define DEFAULT_DEPTH_TYPE depth_type::normalized
#endif

// work-group/tile x/y size
#if !defined(TILE_SIZE_X)
#define TILE_SIZE_X 32
#endif
#if !defined(TILE_SIZE_Y)
#define TILE_SIZE_Y 16
#endif


#if defined(FLOOR_COMPUTE)

#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

#endif

#endif
