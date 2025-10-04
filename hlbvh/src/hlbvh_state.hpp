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
#include <floor/device/indirect_command.hpp>
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
	
	// if true:  draw collided triangles red (note that this is slower than
	//           "just doing collision detection" due the necessity to copy/transform
	//           data so that it can be used by the renderer), also still some inaccuracies
	// if false: draw collided models red (fast-ish, not as fast as console/benchmark-only mode)
	bool triangle_vis { true };
	
	// use improved radix sort?
	bool improved_radix_sort { true };
	// if enabled, uses kernels that don't use local memory atomics
	bool no_local_atomics { false };
	
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
	
	PLATFORM_TYPE default_platform { PLATFORM_TYPE::NONE };
	
	// collision/hlbvh kernels
	const device_function* kernel_build_aabbs { nullptr };
	const device_function* kernel_collide_root_aabbs { nullptr };
	const device_function* kernel_compute_morton_codes { nullptr };
	const device_function* kernel_build_bvh { nullptr };
	const device_function* kernel_build_bvh_aabbs_leaves { nullptr };
	const device_function* kernel_build_bvh_aabbs { nullptr };
	const device_function* kernel_collide_bvhs_no_tri_vis { nullptr };
	const device_function* kernel_collide_bvhs_tri_vis { nullptr };
	const device_function* kernel_map_collided_triangles { nullptr };
	
	const device_function* kernel_indirect_radix_sort_count { nullptr };
	const device_function* kernel_radix_sort_prefix_sum { nullptr };
	const device_function* kernel_indirect_radix_sort_stream_split { nullptr };
	
	const device_function* kernel_indirect_radix_zero { nullptr };
	const device_function* kernel_indirect_radix_upsweep_init { nullptr };
	const device_function* kernel_indirect_radix_upsweep_pass_only { nullptr };
	const device_function* kernel_indirect_radix_upsweep { nullptr };
	const device_function* kernel_indirect_radix_scan_small { nullptr };
	const device_function* kernel_indirect_radix_scan { nullptr };
	const device_function* kernel_indirect_radix_downsweep_keys { nullptr };
	const device_function* kernel_indirect_radix_downsweep_kv16 { nullptr };
	std::shared_ptr<indirect_command_pipeline> radix_sort_pipeline;
	std::array<std::shared_ptr<device_buffer>, 4u> radix_shift_param_buffers;
	std::shared_ptr<device_buffer> radix_sort_param_buffer;
	std::shared_ptr<device_buffer> radix_sort_global_histogram;
	std::shared_ptr<device_buffer> radix_sort_pass_histogram;
	
	uint32_t max_local_size_build_aabbs { 0u };
	uint32_t max_local_size_collide_root_aabbs { 0u };
	uint32_t max_local_size_compute_morton_codes { 0u };
	uint32_t max_local_size_build_bvh { 0u };
	uint32_t max_local_size_build_bvh_aabbs_leaves { 0u };
	uint32_t max_local_size_build_bvh_aabbs { 0u };
	uint32_t max_local_size_collide_bvhs_no_tri_vis { 0u };
	uint32_t max_local_size_collide_bvhs_tri_vis { 0u };
	uint32_t max_local_size_map_collided_triangles { 0u };
	
	uint32_t max_local_size_indirect_radix_sort_count { 0u };
	uint32_t max_local_size_radix_sort_prefix_sum { 0u };
	uint32_t max_local_size_indirect_radix_sort_stream_split { 0u };
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

struct collide_params_t {
	uint32_t leaf_count_a;
	uint32_t internal_node_count_b;
	uint32_t mesh_idx_a;
	uint32_t mesh_idx_b;
};
