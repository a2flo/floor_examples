/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#ifndef __FLOOR_NBODY_NBODY_STATE_HPP__
#define __FLOOR_NBODY_NBODY_STATE_HPP__

#include <floor/math/quaternion.hpp>

#if !defined(NBODY_SOFTENING)
#define NBODY_SOFTENING 0.01f
#endif

#if !defined(NBODY_DAMPING)
#define NBODY_DAMPING 0.999f
#endif

#if !defined(NBODY_TILE_SIZE)
#define NBODY_TILE_SIZE 256u
#endif

using namespace fl;

struct nbody_state_struct {
	uint32_t body_count { 65536 };
	
	// NOTE on iOS: this must be the same variable as used during the kernel compilation (-> build step)
	uint32_t tile_size { NBODY_TILE_SIZE };
	
	float time_step { 0.001f };
	float2 mass_minmax_default { 0.05f, 10.0f };
	float2 mass_minmax { mass_minmax_default };
	float softening { NBODY_SOFTENING }; // 0.1 is also interesting
	float damping { NBODY_DAMPING };
	
	quaternionf cam_rotation;
	bool enable_cam_rotate { false }, enable_cam_move { false };
	float distance { 50.0f };
	const float max_distance { 2500.0f };
	
	bool render_sprites { true };
	bool alpha_mask { false };
	uint32_t render_size { 0 };
	
	//
	bool done { false };
	bool stop { false };
	
	//
	bool no_metal { false };
	bool no_vulkan { false };
	bool benchmark { false };
	bool msaa { true };
	bool no_indirect { false };
	bool no_fubar { false };
	
};
#if !defined(FLOOR_DEVICE) || defined(FLOOR_DEVICE_HOST_COMPUTE)
extern nbody_state_struct nbody_state;
#endif

#endif
