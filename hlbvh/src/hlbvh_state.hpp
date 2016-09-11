/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_HLBVH_HLBVH_STATE_HPP__
#define __FLOOR_HLBVH_HLBVH_STATE_HPP__

// fixed work-group count and work-group sizes for all kernels
#define COMPACTION_GROUP_COUNT 256u
#define COMPACTION_GROUP_SIZE 256u
#define PREFIX_SUM_GROUP_SIZE 256u
#define ROOT_AABB_GROUP_SIZE 256u

#include <floor/math/quaternion.hpp>
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/compute_context.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_queue.hpp>
#endif

struct hlbvh_state_struct {
	bool cam_mode { true }; // false: rotate around origin, true: free cam
	quaternionf cam_rotation;
	bool enable_cam_rotate { false }, enable_cam_move { false };
	float distance { 10.0f };
	const float max_distance { 100.0f };
	
	//
	bool done { false };
	bool stop { false };
	
	//
	bool no_opengl { false };
	bool no_metal { false };
	bool no_vulkan { false };
	bool benchmark { false };
	
	
	// if true:  draw collided triangles red (note that this is slower than
	//           "just doing collision detection" due the necessity to copy/transform
	//           data so that it can be used by opengl/metal), also still some inaccuracies
	// if false: draw collided models red (fast-ish, not as fast as console/benchmark-only mode)
	bool triangle_vis { true };
	
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	// main compute context
	shared_ptr<compute_context> ctx;
	// active compute device
	shared_ptr<compute_device> dev;
	// device compute/command queue
	shared_ptr<compute_queue> dev_queue;
	// collision/hlbvh kernels
	unordered_map<string, shared_ptr<compute_kernel>> kernels;
	unordered_map<string, uint32_t> kernel_max_local_size;
#endif
	
};
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
extern hlbvh_state_struct hlbvh_state;
#endif

#endif
