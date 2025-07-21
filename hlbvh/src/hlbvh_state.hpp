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

#pragma once

#include <floor/core/essentials.hpp>
#include <floor/math/quaternion.hpp>
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
#include <floor/device/device_context.hpp>
#include <floor/device/device.hpp>
#include <floor/device/device_queue.hpp>
#endif
using namespace fl;

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
	bool no_metal { false };
	bool no_vulkan { false };
	bool benchmark { false };
	bool no_fubar { false };
	
	bool uni_renderer { true };
	
	// if true:  draw collided triangles red (note that this is slower than
	//           "just doing collision detection" due the necessity to copy/transform
	//           data so that it can be used by the renderer), also still some inaccuracies
	// if false: draw collided models red (fast-ish, not as fast as console/benchmark-only mode)
	bool triangle_vis { true };
	
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
	// main compute context
	std::shared_ptr<device_context> cctx;
	// device compute/command queue
	std::shared_ptr<device_queue> cqueue;
	// active compute device
	const device* cdev { nullptr };
	
	//! render device context
	std::shared_ptr<device_context> rctx;
	//! render device main queue
	std::shared_ptr<device_queue> rqueue;
	//! render device
	const device* rdev { nullptr };
	
	// collision/hlbvh kernels
	std::unordered_map<std::string, std::shared_ptr<device_function>> kernels;
	std::unordered_map<std::string, uint32_t> kernel_max_local_size;
#endif
	
};
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
extern hlbvh_state_struct hlbvh_state;
#endif

// fixed work-group count and work-group sizes for all kernels
#define COMPACTION_GROUP_COUNT 256u
#define COMPACTION_GROUP_SIZE 256u
#define PREFIX_SUM_GROUP_SIZE 256u
#define ROOT_AABB_GROUP_SIZE 256u

struct indirect_radix_sort_params_t {
	uint32_t count;
	uint32_t count_per_group; // == count / COMPACTION_GROUP_COUNT
};
