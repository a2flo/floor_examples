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
#if !defined(SCREEN_WIDTH)
#define SCREEN_WIDTH 1280
#endif
#if !defined(SCREEN_HEIGHT)
#define SCREEN_HEIGHT 720
#endif
#if !defined(SCREEN_FOV)
#define SCREEN_FOV 72.0f
#endif

// screen origin is left bottom for opengl, left top for metal (and directx)
#if !defined(FLOOR_COMPUTE_METAL)
#define SCREEN_ORIGIN_LEFT_BOTTOM 1
#else
#define SCREEN_ORIGIN_LEFT_TOP 1
#endif

// normalized depth (in [0, 1]) for opengl/metal
#define DEPTH_NORMALIZED 1
//#define DEPTH_ZW 1 // e.g. if written as z/w by the shader
//#define DEPTH_LOG 1 // TODO: support log depth?

#if defined(FLOOR_COMPUTE)

#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

#endif

#endif
