/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_HLBVH_COLLIDER_HPP__
#define __FLOOR_HLBVH_COLLIDER_HPP__

#include "hlbvh_state.hpp"
#include "animation.hpp"
#include <floor/compute/indirect_command.hpp>

class collider {
public:
	const vector<uint32_t>& collide(const vector<unique_ptr<animation>>& models);
	
protected:
	size_t allocated_model_count { 0 };
	shared_ptr<compute_buffer> collision_flags;
	shared_ptr<compute_buffer> aabb_collision_flags;
	shared_ptr<compute_buffer> aabbs;
	shared_ptr<compute_buffer> valid_counts_buffer;
	vector<shared_ptr<compute_buffer>> bit_buffers;
	shared_ptr<compute_buffer> rs_params_buffer;
	vector<uint32_t> collision_flags_host;
	
	unique_ptr<indirect_command_pipeline> radix_sort_pipeline;
	uint32_t radix_sort_pipeline_max_bit = 0u;
	
	void radix_sort(shared_ptr<compute_buffer> buffer,
					shared_ptr<compute_buffer> ping_buffer,
					const uint32_t size,
					const uint32_t max_bit = 32u);
	
};

#endif
