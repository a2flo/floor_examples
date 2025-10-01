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

#include "hlbvh_state.hpp"
#include "animation.hpp"
#include <floor/device/indirect_command.hpp>

class collider {
public:
	void collide(const std::vector<std::unique_ptr<animation>>& models);
	
protected:
	size_t allocated_model_count { 0 };
	std::shared_ptr<device_buffer> collision_flags;
	std::shared_ptr<device_buffer> aabb_collision_flags;
	std::shared_ptr<device_buffer> aabbs;
	std::shared_ptr<device_buffer> valid_counts_buffer;
	std::vector<std::shared_ptr<device_buffer>> bit_buffers;
	std::shared_ptr<device_buffer> rs_params_buffer;
	std::vector<uint32_t> collision_flags_host;
	
	std::unique_ptr<indirect_command_pipeline> radix_sort_pipeline;
	uint32_t radix_sort_pipeline_max_bit { 0u };
	
	void radix_sort(device_buffer* inout_buffer,
					device_buffer* ping_buffer,
					device_buffer* values_inout_buffer,
					device_buffer* values_ping_buffer,
					const uint32_t count);
	
	void radix_sort_legacy(device_buffer* inout_buffer,
						   device_buffer* ping_buffer,
						   device_buffer* values_inout_buffer,
						   device_buffer* values_ping_buffer,
						   const uint32_t count);
	
	void radix_sort_improved(device_buffer* inout_buffer,
							 device_buffer* ping_buffer,
							 device_buffer* values_inout_buffer,
							 device_buffer* values_ping_buffer,
							 const uint32_t count);
	
};
