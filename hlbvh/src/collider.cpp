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

#include "collider.hpp"

const vector<uint32_t>& collider::collide(const vector<unique_ptr<animation>>& models) {
	if(models.empty()) return collision_flags_host;
	if(hlbvh_state.stop) return collision_flags_host;
	
	// general idea:
	// * construct root aabbs of all models (+compute and store interpolated triangles for this frame)
	// * collide root aabbs with each other and store these aabb collision flags (then, read back on host)
	// * only need to construct bvhs and do further collision detection for models which aabbs have collided with something
	// * compute bvh for each valid model (compute morton codes, compute actual bvh structure, compute aabbs)
	// * intersect bvhs (and triangles) with each other for all potential model pairs (from step #2)
	const auto model_count = models.size();
	const auto total_aabb_checks = (uint32_t)(model_count * model_count - model_count) / 2u;
	
	// alloc all data (once every time model count changes)
	if(model_count != allocated_model_count) {
		allocated_model_count = model_count;
		
		collision_flags = hlbvh_state.ctx->create_buffer(hlbvh_state.dev, model_count * sizeof(uint32_t),
														 COMPUTE_MEMORY_FLAG::WRITE | COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
		collision_flags_host.resize(model_count);
	
		aabb_collision_flags = hlbvh_state.ctx->create_buffer(hlbvh_state.dev, total_aabb_checks * sizeof(uint32_t),
															  COMPUTE_MEMORY_FLAG::WRITE | COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
		
		aabbs = hlbvh_state.ctx->create_buffer(hlbvh_state.dev, model_count * sizeof(float3) * 2,
											   COMPUTE_MEMORY_FLAG::READ_WRITE | COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
	}
	
	// init all data (every time this is called)
	collision_flags->zero(hlbvh_state.dev_queue);
	aabb_collision_flags->zero(hlbvh_state.dev_queue);
	
#if COLLIDING_TRIANGLES_VIS
	for(const auto& mdl : models) {
		mdl->colliding_triangles->zero(hlbvh_state.dev_queue);
	}
#endif
	
	vector<float3> init_aabbs(model_count * 2);
	for(size_t i = 0; i < model_count; ++i) {
		init_aabbs[i * 2] = float3(numeric_limits<float>::max());
		init_aabbs[i * 2 + 1] = float3(numeric_limits<float>::lowest());
	}
	aabbs->write(hlbvh_state.dev_queue, init_aabbs);
	
	// compute root aabbs
	uint32_t mdl_idx = 0;
	for(const auto& mdl : models) {
		const auto cur_frame = mdl->cur_frame, next_frame = mdl->next_frame;
		const auto triangle_count = mdl->tri_count;
		
		//log_undecorated("build_aabbs: %u (%u, %u)", mdl_idx, triangle_count_cur, triangle_count_next); logger::flush();
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["build_aabbs"],
									   uint1 { triangle_count },
									   uint1 { ROOT_AABB_GROUP_SIZE },
									   mdl->frames_triangles_buffer[cur_frame],
									   mdl->frames_triangles_buffer[next_frame],
									   triangle_count,
									   mdl_idx,
									   mdl->step,
									   aabbs,
									   mdl->triangles);
		++mdl_idx;
	}
	
	//
	unordered_set<uint32_t> valid_meshes;
	vector<uint2> potential_pairs;
	{
		//log_undecorated("collide_root_aabbs"); logger::flush();
		auto aabb_collision_flags_host = make_unique<uint32_t[]>(total_aabb_checks);
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["collide_root_aabbs"],
									   uint1 { total_aabb_checks },
									   uint1 { HW_MAX_GROUP_SIZE },
									   aabbs,
									   total_aabb_checks,
									   uint32_t(model_count),
									   aabb_collision_flags);
		
		// read back aabb collision flags
		aabb_collision_flags->read(hlbvh_state.dev_queue, aabb_collision_flags_host.get());
		
		// we're only interessted in further constructing+colliding bvhs for meshes whose root aabbs collide with something
		// and also only the resp. collision pairs
		for(uint32_t lin_idx = 0; lin_idx < total_aabb_checks; ++lin_idx) {
			if(aabb_collision_flags_host[lin_idx] != 0) {
				// reverse cantor (map 1D linear index onto "half triangle" of a square -> all unique combinations of (i, j), with i != j)
				const auto q = (uint32_t)((const_math::EPSILON<float> + sqrt(1.0f + 8.0f * float(lin_idx))) * 0.5f - 0.5f);
				const auto i = lin_idx - (q * (q + 1u)) / 2u;
				const auto j = uint32_t(model_count) - q + i - 1u;
				valid_meshes.insert(i);
				valid_meshes.insert(j);
				potential_pairs.emplace_back(i, j);
				//log_undecorated("possible collision: %u <-> %u", i, j);
			}
		}
	}
	
	
	// compute bvh
	for(const auto& i : valid_meshes) {
		const auto& mdl = models[i];
		const auto cur_frame = mdl->cur_frame, next_frame = mdl->next_frame;
		const auto triangle_count = mdl->tri_count;
		const auto morton_codes_count = uint32_t(mdl->morton_codes->get_size() / sizeof(uint2));
		
		//log_undecorated("compute_morton_codes: %u", i); logger::flush();
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["compute_morton_codes"],
									   uint1 { morton_codes_count },
									   uint1 { HW_MAX_GROUP_SIZE },
									   aabbs,
									   mdl->frames_centroids_buffer[cur_frame],
									   mdl->frames_centroids_buffer[next_frame],
									   triangle_count,
									   i,
									   mdl->step,
									   mdl->morton_codes);
		
		//log_undecorated("radix: %u", i); logger::flush();
		radix_sort(mdl->morton_codes, mdl->morton_codes_ping, morton_codes_count, 30);
		
		//
		const auto leaf_count = triangle_count;
		const auto internal_node_count = leaf_count - 1u;
		//log_undecorated("build_bvh: %u (node count: %u/%u)", i, leaf_count, internal_node_count); logger::flush();
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["build_bvh"],
									   uint1 { internal_node_count },
									   uint1 { HW_MAX_GROUP_SIZE },
									   mdl->morton_codes,
									   mdl->bvh_internal,
									   mdl->bvh_leaves,
									   internal_node_count);
		
		//log_undecorated("build_bvh_aabbs_leaves: %u", i); logger::flush();
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["build_bvh_aabbs_leaves"],
									   uint1 { leaf_count },
									   uint1 { HW_MAX_GROUP_SIZE },
									   mdl->morton_codes,
									   leaf_count,
									   mdl->triangles,
									   mdl->bvh_aabbs_leaves);
		
		//log_undecorated("build_bvh_aabbs: %u", i); logger::flush();
		mdl->bvh_aabbs_counters->zero(hlbvh_state.dev_queue);
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["build_bvh_aabbs"],
									   uint1 { leaf_count },
									   uint1 { HW_MAX_GROUP_SIZE },
									   mdl->morton_codes,
									   mdl->bvh_internal,
									   mdl->bvh_leaves,
									   internal_node_count,
									   leaf_count,
									   mdl->triangles,
									   mdl->bvh_aabbs,
									   mdl->bvh_aabbs_leaves,
									   mdl->bvh_aabbs_counters);
	}
	// collide all potential mesh collision pairs with each other
	for(const auto& col_pair : potential_pairs) {
		const auto& i = col_pair.x;
		const auto& j = col_pair.y;
		const auto& mdl_i = models[i];
		const auto& mdl_j = models[j];
		//log_debug("collide: %u %u", i, j); logger::flush();
		
		const auto leaf_count_i = mdl_i->tri_count;
		const auto leaf_count_j = mdl_j->tri_count;
		
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["collide_bvhs"],
									   uint1 { leaf_count_i },
									   uint1 { HW_MAX_GROUP_SIZE },
									   // the leaves of bvh A that we want to collide with bvh B
									   leaf_count_i,
									   mdl_i->bvh_aabbs_leaves,
									   mdl_i->triangles,
									   mdl_i->morton_codes,
									   // the complete bvh B
									   leaf_count_j - 1u,
									   mdl_j->bvh_internal,
									   mdl_j->bvh_aabbs,
									   mdl_j->bvh_aabbs_leaves,
									   mdl_j->triangles,
									   mdl_j->morton_codes,
									   // mesh indices of A and B
									   i, j,
									   // flags if resp. mesh A/B collides with anything
									   // also (ab)used as an abort condition here
									   collision_flags
#if COLLIDING_TRIANGLES_VIS
									   , mdl_i->colliding_triangles
									   , mdl_j->colliding_triangles
#endif
									   );
	}
	
	// copy to host + return
	// TODO: share buffer with gl/metal instead? (unless doing cpu side checking)
	collision_flags->read(hlbvh_state.dev_queue, &collision_flags_host[0]);
#if 0 // log collided models
	string collision_str = "collisions: ";
	for(const auto& col_flag : collision_flags_host) {
		collision_str += (col_flag > 0 ? "1 " : "0 ");
	}
	log_msg("%s", collision_str);
#endif
	
#if COLLIDING_TRIANGLES_VIS
	for(uint32_t i = 0; i < uint32_t(model_count); ++i) {
		const auto& mdl = models[i];
		const auto cur_frame = mdl->cur_frame;
		const auto triangle_count = mdl->tri_count;
		mdl->colliding_vertices->zero(hlbvh_state.dev_queue);
		if(collision_flags_host[i] > 0) {
			hlbvh_state.dev_queue->execute(hlbvh_state.kernels["map_collided_triangles"],
										   uint1 { triangle_count },
										   uint1 { HW_MAX_GROUP_SIZE },
										   mdl->colliding_triangles,
										   mdl->frames_indices[cur_frame],
										   mdl->colliding_vertices,
										   triangle_count);
		}
	}
#endif
	
	return collision_flags_host;
}

void collider::radix_sort(shared_ptr<compute_buffer> buffer,
						  shared_ptr<compute_buffer> ping_buffer,
						  const size_t size,
						  const uint32_t max_bit) {
	if(!valid_counts_buffer) {
		valid_counts_buffer = hlbvh_state.ctx->create_buffer(hlbvh_state.dev, COMPACTION_GROUP_COUNT * sizeof(int32_t),
															 COMPUTE_MEMORY_FLAG::READ_WRITE);
	}
	
	//log_debug("radix sort: size: %u", size); logger::flush();
	for(uint32_t bit = 0u; bit < max_bit; ++bit) {
		const auto mask_op_bit = uint32_t(1u << bit);
		//log_debug("radix sort: %u, %X, size: %u", bit, mask_op_bit, size); logger::flush();
		
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["radix_sort_count"],
									   uint1 { COMPACTION_GROUP_COUNT * COMPACTION_GROUP_SIZE },
									   uint1 { COMPACTION_GROUP_SIZE },
									   buffer,
									   uint32_t(size),
									   uint32_t(size / COMPACTION_GROUP_SIZE),
									   mask_op_bit,
									   valid_counts_buffer);
		
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["radix_sort_prefix_sum"],
									   uint1 { PREFIX_SUM_GROUP_SIZE },
									   uint1 { PREFIX_SUM_GROUP_SIZE },
									   valid_counts_buffer,
									   uint32_t(COMPACTION_GROUP_COUNT));
		
		hlbvh_state.dev_queue->execute(hlbvh_state.kernels["radix_sort_stream_split"],
									   uint1 { COMPACTION_GROUP_COUNT * COMPACTION_GROUP_SIZE },
									   uint1 { COMPACTION_GROUP_SIZE },
									   buffer,
									   ping_buffer,
									   uint32_t(size),
									   uint32_t(size / COMPACTION_GROUP_SIZE),
									   mask_op_bit,
									   valid_counts_buffer);
		
		buffer.swap(ping_buffer);
	}
}
