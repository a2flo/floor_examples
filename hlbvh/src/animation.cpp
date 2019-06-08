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

#include "animation.hpp"
#include <floor/threading/task.hpp>

animation::animation(const string& file_prefix,
					 const string& file_suffix,
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
	if(hlbvh_state.triangle_vis) {
		frames_indices.resize(frame_count);
	}
	atomic<uint32_t> done { frame_count };
	atomic<uint32_t> load_valid { 1u };
	atomic<uint32_t> max_vertex_count { 0 };
	for(uint32_t i = 0; i < frame_count; ++i) {
#define THREADED_OBJ_LOAD 0 // TODO: still need to make this thread-safe for opengl
#if !THREADED_OBJ_LOAD // non-threaded
		const auto frame_id = i;
#else
		task::spawn([this, frame_id = i, &done, &load_valid, &digits_width,
					 &file_prefix, &file_suffix]() {
#endif
			// start with frame id suffix (width is always the same, so insert 0s where necessary, e.g. 00042)
			string file_name = to_string(frame_id + 1);
			file_name.insert(0, digits_width - uint32_t(file_name.size()), '0');
			
			// load
			file_name.insert(0, file_prefix);
			file_name += file_suffix;
			//log_debug("file name: %s", file_name);
			bool success = false;
			auto model = obj_loader::load(floor::data_path(file_name), success, *hlbvh_state.ctx, *hlbvh_state.dev_queue,
										  // don't scale anything
										  1.0f,
										  // keep cpu data, b/c we still need it
										  // TODO: delete at the end
										  false,
										  // don't load textures, we don't need them
										  false,
										  // create gpu/graphics buffers?
										  !hlbvh_state.benchmark);
			frames[frame_id] = model;
			if(!success) {
				// signal that something went wrong
				load_valid = 0;
			}
			else {
				// linearize triangles + compute centroids for them
				// -> transforms the "face/vertex-idxs -> vertices" model into a linear array of triangles
				// TODO: reorder triangles based on morton code?
				auto mdl_triangles = make_shared<vector<float3>>();
				auto mdl_centroids = make_shared<vector<float3>>();
				auto mdl_indices = make_shared<vector<uint3>>();
				
				uint32_t triangle_count = 0;
				for(const auto& sub_obj : model->objects) {
					triangle_count += sub_obj->indices.size();
				}
				mdl_triangles->reserve(triangle_count * 3);
				mdl_centroids->reserve(triangle_count);
				if(hlbvh_state.triangle_vis) {
					mdl_indices->reserve(triangle_count);
				}
				
				size_t idx_count = 0;
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
						if(hlbvh_state.triangle_vis) {
							mdl_indices->emplace_back(idx);
						}
					}
					idx_count += sub_obj->indices.size();
				}
				frames_triangles[frame_id] = mdl_triangles;
				frames_centroids[frame_id] = mdl_centroids;
				
				// upload key-frame data for this animation
				frames_triangles_buffer[frame_id] = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, *mdl_triangles);
				frames_centroids_buffer[frame_id] = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, *mdl_centroids);
				if(hlbvh_state.triangle_vis) {
					frames_indices[frame_id] = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, *mdl_indices);
				}
				
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
		this_thread::yield();
	}
	valid = (load_valid != 0);
	if(!valid) return;
	
	// check if triangle count per frame is the same
	// (differences in triangle count could be handled, but only at the cost of performance)
	for(uint32_t i = 0; i < frame_count; ++i) {
		const auto frame_tri_count = (uint32_t)(frames_triangles[i]->size() / 3);
		if(i == 0) {
			tri_count = frame_tri_count;
		}
		else if(tri_count != frame_tri_count) {
			log_error("variable triangle count for \"%s\" frame #%u (first frame: %u, this frame: %u)",
					  file_prefix + file_suffix, i, tri_count, frame_tri_count);
			return;
		}
	}
	log_debug("%s #triangles: %u", file_prefix, tri_count);
	
	// now that we have the max triangle count, allocate the morton codes + ping buffer with this max size.
	// note that radix sort requires a multiple of 8192 (32 * 256) elements to function.
	static const size_t rs_alignment = 32u * COMPACTION_GROUP_SIZE * sizeof(uint2);
	auto morton_codes_size = tri_count * sizeof(uint2);
	if(morton_codes_size % rs_alignment != 0) {
		morton_codes_size = ((morton_codes_size / rs_alignment) + 1) * rs_alignment;
	}
	
	morton_codes = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, morton_codes_size);
	morton_codes_ping = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, morton_codes_size);
	triangles = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, tri_count * sizeof(float3) * 3u);
	
	// N leaves + (N-1) internal nodes, allocating enough for max triangle count
	bvh_leaves = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, tri_count * sizeof(uint32_t));
	bvh_internal = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, (tri_count - 1u) * sizeof(uint3));
	bvh_aabbs = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, (tri_count - 1u) * sizeof(float3) * 2u);
	bvh_aabbs_leaves = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, tri_count * sizeof(float3) * 2u);
	bvh_aabbs_counters = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, (tri_count - 1u) * sizeof(uint32_t));
	
	// for visualization purposes
	if(hlbvh_state.triangle_vis) {
		log_debug("max vertex count: %u", max_vertex_count.load());
		colliding_vertices = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, max_vertex_count * sizeof(uint32_t),
															COMPUTE_MEMORY_FLAG::READ_WRITE |
															COMPUTE_MEMORY_FLAG::HOST_READ_WRITE |
															COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
															GL_ARRAY_BUFFER);
		colliding_triangles[0] = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, tri_count * sizeof(uint32_t));
		colliding_triangles[1] = hlbvh_state.ctx->create_buffer(*hlbvh_state.dev_queue, tri_count * sizeof(uint32_t));
		log_debug("check tri col buffer: %u, %u", colliding_vertices->get_size(), colliding_vertices->get_opengl_object());
	}
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
