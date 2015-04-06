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

#ifndef __FLOOR_NBODY_NBODY_HPP__
#define __FLOOR_NBODY_NBODY_HPP__

#include <floor/math/vector_lib.hpp>

#if defined(FLOOR_COMPUTE)

// prototypes
kernel void nbody_compute(buffer<const float4> in_positions,
						  buffer<float4> out_positions,
						  buffer<float3> velocities,
						  param<float> delta);

kernel void nbody_raster(buffer<float4> positions,
						 buffer<float3> img,
						 buffer<float3> img_old,
						 param<uint2> img_size,
						 param<uint32_t> body_count,
						 param<matrix4f> mview);

#endif

#endif
