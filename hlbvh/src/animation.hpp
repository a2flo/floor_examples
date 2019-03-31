/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_HLBVH_ANIMATION_HPP__
#define __FLOOR_HLBVH_ANIMATION_HPP__

#include "hlbvh_state.hpp"
#include "obj_loader.hpp"

struct animation {
	animation(const string& file_prefix,
			  const string& file_suffix,
			  const uint32_t frame_count,
			  const bool loop_or_reset = false,
			  const float step_size = 0.025f);
	
	bool is_valid() const { return valid; }
	
	void do_step();
	
	bool valid { false };
	bool stopped { true };
	bool loop_or_reset { false }; // false: loop, true: reset
	
	uint32_t frame_count { 0 };
	uint32_t cur_frame { 0 };
	uint32_t next_frame { 1 };
	float step { 0.0f };
	
	uint32_t tri_count { 0 };
	vector<shared_ptr<obj_model>> frames;
	vector<shared_ptr<vector<float3>>> frames_triangles;
	vector<shared_ptr<vector<float3>>> frames_centroids;
	vector<shared_ptr<compute_buffer>> frames_triangles_buffer;
	vector<shared_ptr<compute_buffer>> frames_centroids_buffer;
	
	float step_size;
	
	// interpolated triangles of the current+next frame
	shared_ptr<compute_buffer> triangles;
	
	// morton code buffer (+ping buffer for radix sort) used by all frames
	shared_ptr<compute_buffer> morton_codes;
	shared_ptr<compute_buffer> morton_codes_ping;
	
	// bvh buffers (leaf nodes + internal nodes)
	shared_ptr<compute_buffer> bvh_leaves;
	shared_ptr<compute_buffer> bvh_internal;
	shared_ptr<compute_buffer> bvh_aabbs;
	shared_ptr<compute_buffer> bvh_aabbs_leaves;
	shared_ptr<compute_buffer> bvh_aabbs_counters;
	
	uint32_t colliding_triangles_idx { 0 };
	shared_ptr<compute_buffer> colliding_triangles[2];
	shared_ptr<compute_buffer> colliding_vertices;
	vector<shared_ptr<compute_buffer>> frames_indices;
	
};

#endif
