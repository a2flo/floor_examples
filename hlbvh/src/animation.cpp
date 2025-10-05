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

#include "animation.hpp"
#include "radix_sort.hpp"
#include <floor/threading/task.hpp>

animation::animation(const std::string& file_prefix,
					 const std::string& file_suffix,
					 const uint32_t frame_count_,
					 const bool loop_or_reset_,
					 const float step_size_) :
loop_or_reset(loop_or_reset_), frame_count(frame_count_), step_size(step_size_) {
	if(frame_count < 2) return;
	
	// read files in parallel
	const auto digits_width = const_math::int_width(frame_count);
	frames.resize(frame_count);
	frames_triangles.resize(frame_count);
	frames_centroids.resize(frame_count);
	frames_triangles_buffer.resize(frame_count);
	frames_centroids_buffer.resize(frame_count);
	frames_indices.resize(frame_count);
	std::atomic<uint32_t> done { frame_count };
	std::atomic<uint32_t> load_valid { 1u };
	std::atomic<uint32_t> max_vertex_count { 0 };
	auto sharing_flags = MEMORY_FLAG::NONE;
	if (hlbvh_state.cctx != hlbvh_state.rctx && !hlbvh_state.benchmark) {
		if (hlbvh_state.rctx->get_platform_type() == PLATFORM_TYPE::VULKAN) {
			sharing_flags = MEMORY_FLAG::VULKAN_SHARING;
		} else if (hlbvh_state.rctx->get_platform_type() == PLATFORM_TYPE::METAL) {
			sharing_flags = MEMORY_FLAG::METAL_SHARING;
		}
	}
	for (uint32_t i = 0; i < frame_count; ++i) {
#define THREADED_OBJ_LOAD 0 // TODO: still need to make this thread-safe for opengl
#if !THREADED_OBJ_LOAD // non-threaded
		const auto frame_id = i;
#else
		task::spawn([this, frame_id = i, &done, &load_valid, &digits_width,
					 &file_prefix, &file_suffix]() {
#endif
			// start with frame id suffix (width is always the same, so insert 0s where necessary, e.g. 00042)
			std::string file_name = std::to_string(frame_id + 1);
			file_name.insert(0, digits_width - uint32_t(file_name.size()), '0');
			
			// load
			file_name.insert(0, file_prefix);
			file_name += file_suffix;
			//log_debug("file name: $", file_name);
			bool success = false;
			auto model = obj_loader::load(floor::data_path(file_name), success, *hlbvh_state.cctx, *hlbvh_state.cqueue,
										  // don't scale anything
										  1.0f,
										  // keep cpu data, b/c we still need it
										  // TODO: delete at the end
										  false,
										  // don't load textures, we don't need them
										  false,
										  // create gpu/graphics buffers?
										  !hlbvh_state.benchmark,
										  sharing_flags);
			frames[frame_id] = model;
			if(!success) {
				// signal that something went wrong
				load_valid = 0;
			}
			else {
				// linearize triangles + compute centroids for them
				// -> transforms the "face/vertex-idxs -> vertices" model into a linear array of triangles
				// TODO: reorder triangles based on morton code?
				auto mdl_triangles = std::make_shared<std::vector<float3>>();
				auto mdl_centroids = std::make_shared<std::vector<float3>>();
				auto mdl_indices = std::make_shared<std::vector<uint3>>();
				
				uint32_t triangle_count = 0;
				for(const auto& sub_obj : model->objects) {
					triangle_count += sub_obj->indices.size();
				}
				mdl_triangles->reserve(triangle_count * 3);
				mdl_centroids->reserve(triangle_count);
				mdl_indices->reserve(triangle_count);
				
				for(const auto& sub_obj : model->objects) {
					for(const auto& idx : sub_obj->indices) {
						const float3 tri[] {
							model->vertices[idx.x],
							model->vertices[idx.y],
							model->vertices[idx.z],
						};
						mdl_triangles->emplace_back(tri[0]);
						mdl_triangles->emplace_back(tri[1]);
						mdl_triangles->emplace_back(tri[2]);
						mdl_centroids->emplace_back((tri[0] + tri[1] + tri[2]) * (1.0f / 3.0f));
						mdl_indices->emplace_back(idx);
					}
				}
				frames_triangles[frame_id] = mdl_triangles;
				frames_centroids[frame_id] = mdl_centroids;
				
				// upload key-frame data for this animation
				frames_triangles_buffer[frame_id] = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, *mdl_triangles);
				frames_triangles_buffer[frame_id]->set_debug_label("frames_triangles");
				frames_centroids_buffer[frame_id] = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, *mdl_centroids);
				frames_centroids_buffer[frame_id]->set_debug_label("frames_centroids");
				frames_indices[frame_id] = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, *mdl_indices);
				frames_indices[frame_id]->set_debug_label("frames_indices");
				
				//
				const auto vertex_count = (uint32_t)model->vertices.size();
				for(;;) {
					uint32_t expected = max_vertex_count;
					if(expected >= vertex_count) break;
					if(max_vertex_count.compare_exchange_strong(expected, vertex_count)) break;
				}
			}
			--done;
#if THREADED_OBJ_LOAD
		}, "mdl loader");
#endif
	}
	// wait until all loaded
	while(done > 0) {
		std::this_thread::yield();
	}
	valid = (load_valid != 0);
	if(!valid) return;
	
	// check if triangle count per frame is the same
	// (differences in triangle count could be handled, but only at the cost of performance)
	for (uint32_t i = 0; i < frame_count; ++i) {
		const auto frame_tri_count = (uint32_t)(frames_triangles[i]->size() / 3);
		if (i == 0) {
			tri_count = frame_tri_count;
		} else if (tri_count != frame_tri_count) {
			log_error("variable triangle count for \"$\" frame #$ (first frame: $, this frame: $)",
					  file_prefix + file_suffix, i, tri_count, frame_tri_count);
			return;
		}
	}
	if (tri_count > max_triangle_count) {
		log_error("triangle count is too large: $' - only up to $' triangles are supported", tri_count, max_triangle_count);
		return;
	}
	log_debug("$ #triangles: $", file_prefix, tri_count);
	
	// now that we have the max triangle count, allocate the morton codes + ping buffer with this max size
	auto morton_codes_keys_size = tri_count * sizeof(uint32_t);
	auto morton_codes_values_size = tri_count * sizeof(uint16_t);
	if (!hlbvh_state.improved_radix_sort) {
		// NOTE: legacy radix sort requires a multiple of 8192 (32 * 256) elements to function.
		static constexpr const size_t rs_legacy_alignment_keys { 32u * COMPACTION_GROUP_SIZE * sizeof(uint32_t) };
		static constexpr const size_t rs_legacy_alignment_values { 32u * COMPACTION_GROUP_SIZE * sizeof(uint16_t) };
		if ((morton_codes_keys_size % rs_legacy_alignment_keys) != 0) {
			morton_codes_keys_size = ((morton_codes_keys_size / rs_legacy_alignment_keys) + 1) * rs_legacy_alignment_keys;
		}
		if ((morton_codes_values_size % rs_legacy_alignment_values) != 0) {
			morton_codes_values_size = ((morton_codes_values_size / rs_legacy_alignment_values) + 1) * rs_legacy_alignment_values;
		}
	}
	
	morton_codes_keys = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, morton_codes_keys_size);
	morton_codes_keys->set_debug_label("morton_codes_keys");
	morton_codes_keys_ping = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, morton_codes_keys_size);
	morton_codes_keys_ping->set_debug_label("morton_codes_keys_ping");
	morton_codes_values = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, morton_codes_values_size);
	morton_codes_values->set_debug_label("morton_codes_values");
	morton_codes_values_ping = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, morton_codes_values_size);
	morton_codes_values_ping->set_debug_label("morton_codes_values_ping");
	
	triangles = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, tri_count * sizeof(float3) * 3u);
	triangles->set_debug_label("triangles");
	
	// N leaves + (N-1) internal nodes, allocating enough for max triangle count
	bvh_leaves = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, tri_count * sizeof(uint32_t));
	bvh_leaves->set_debug_label("bvh_leaves");
	bvh_internal = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, (tri_count - 1u) * sizeof(uint3));
	bvh_internal->set_debug_label("bvh_internal");
	bvh_aabbs = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, (tri_count - 1u) * sizeof(bboxf));
	bvh_aabbs->set_debug_label("bvh_aabbs");
	bvh_aabbs_leaves = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, tri_count * sizeof(bboxf));
	bvh_aabbs_leaves->set_debug_label("bvh_aabbs_leaves");
	bvh_aabbs_counters = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, (tri_count - 1u) * sizeof(uint32_t));
	bvh_aabbs_counters->set_debug_label("bvh_aabbs_counters");
	
	// for visualization purposes
	log_debug("max vertex count: $", max_vertex_count.load());
	auto sharing_sync_flags = MEMORY_FLAG::NONE;
	if (hlbvh_state.cctx != hlbvh_state.rctx) {
		sharing_sync_flags |= (MEMORY_FLAG::SHARING_SYNC |
							   // render backend only reads data
							   MEMORY_FLAG::SHARING_RENDER_READ |
							   // compute backend only writes data
							   MEMORY_FLAG::SHARING_COMPUTE_WRITE);
	}
	colliding_vertices = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, max_vertex_count * sizeof(uint32_t),
														 MEMORY_FLAG::READ_WRITE |
														 MEMORY_FLAG::HOST_READ_WRITE |
														 sharing_flags | sharing_sync_flags);
	colliding_vertices->set_debug_label("colliding_vertices");
	colliding_triangles = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, tri_count * sizeof(uint32_t));
	colliding_triangles->set_debug_label("colliding_triangles");
	log_debug("check tri col buffer: $", colliding_vertices->get_size());
}

void animation::do_step() {
	step += step_size;
	if(step > 1.0f) {
		if(!loop_or_reset) {
			// advance by one, loop around
			cur_frame = next_frame;
			next_frame = (next_frame + 1) % frame_count;
		}
		else {
			// reset on wrap-around
			if(next_frame + 1 == frame_count) {
				cur_frame = 0;
				next_frame = 1;
			}
			// middle frame, just advance by one
			else cur_frame = next_frame++;
		}
		step -= 1.0f;
	}
}
