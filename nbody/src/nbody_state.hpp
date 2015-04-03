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

#ifndef __FLOOR_NBODY_NBODY_STATE_HPP__
#define __FLOOR_NBODY_NBODY_STATE_HPP__

#include <floor/math/quaternion.hpp>

struct nbody_state_struct {
	uint32_t body_count { 32768 };
	
	// NOTE on iOS: this must be the same variable as used during the kernel compilation (-> build step)
	uint32_t tile_size { 512 };
	//uint32_t tile_size { 1 }; // TODO: figure out why this is required by intel cpu
	
	float time_step { 0.01f };
	float2 mass_minmax { 0.05f, 2.0f };
	float softening { 0.0001f };
	float damping { 0.999f };
	
	quaternionf cam_rotation;
	bool enable_cam_rotate { false }, enable_cam_move { false };
	float distance { 100.0f };
	const float max_distance { 2500.0f };
	
	bool render_sprites { true };
	bool alpha_mask { false };
	
	//
	bool done { false };
	bool stop { false };
	
	//
	bool no_opengl { false };
	bool benchmark { false };
	
};
extern nbody_state_struct nbody_state;

#endif