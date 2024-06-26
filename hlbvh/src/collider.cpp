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

#include "collider.hpp"
#include <floor/core/timer.hpp>

#if defined(FLOOR_DEBUG)
#define log_if_debug(...) do { log_debug(__VA_ARGS__); } while (false)
#else
#define log_if_debug(...)
#endif

void collider::collide(const vector<unique_ptr<animation>>& models) {
	if (models.empty() || hlbvh_state.stop) {
		return;
	}
	
	if (hlbvh_state.benchmark) {
		hlbvh_state.cqueue->finish();
	}
	const auto start_time = floor_timer::start();
	
	// general idea:
	// * construct root aabbs of all models (+compute and store interpolated triangles for this frame)
	// * collide root aabbs with each other and store these aabb collision flags (then, read back on host)
	// * only need to construct bvhs and do further collision detection for models which aabbs have collided with something
	// * compute bvh for each valid model (compute morton codes, compute actual bvh structure, compute aabbs)
	// * intersect bvhs (and triangles) with each other for all potential model pairs (from step #2)
	const auto model_count = models.size();
	const auto total_aabb_checks = (uint32_t)(model_count * model_count - model_count) / 2u;
	
	// alloc all data (once every time model count changes)
	if (model_count != allocated_model_count) {
		allocated_model_count = model_count;
		
		collision_flags = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, model_count * sizeof(uint32_t),
														  COMPUTE_MEMORY_FLAG::WRITE | COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
		collision_flags->set_debug_label("collision_flags");
		collision_flags_host.resize(model_count);
		
		aabb_collision_flags = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, total_aabb_checks * sizeof(uint32_t),
															   COMPUTE_MEMORY_FLAG::WRITE | COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
		aabb_collision_flags->set_debug_label("aabb_collision_flags");
		
		aabbs = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, model_count * sizeof(float3) * 2,
												COMPUTE_MEMORY_FLAG::READ_WRITE | COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
		aabbs->set_debug_label("aabbs");
	}
	
	// init all data (every time this is called)
	collision_flags->zero(*hlbvh_state.cqueue);
	aabb_collision_flags->zero(*hlbvh_state.cqueue);
	
	if (hlbvh_state.triangle_vis) {
		for (const auto& mdl : models) {
			mdl->colliding_triangles->zero(*hlbvh_state.cqueue);
			mdl->colliding_vertices->zero(*hlbvh_state.cqueue);
		}
	}
	for (const auto& mdl : models) {
		mdl->bvh_aabbs_counters->zero(*hlbvh_state.cqueue);
	}
	
	vector<float3> init_aabbs(model_count * 2);
	for (size_t i = 0; i < model_count; ++i) {
		init_aabbs[i * 2] = float3(__FLT_MAX__);
		init_aabbs[i * 2 + 1] = float3(-__FLT_MAX__);
	}
	aabbs->write(*hlbvh_state.cqueue, init_aabbs);
	hlbvh_state.cqueue->finish();
	
	// compute root aabbs
	uint32_t mdl_idx = 0;
	for (const auto& mdl : models) {
		const auto cur_frame = mdl->cur_frame, next_frame = mdl->next_frame;
		const auto triangle_count = mdl->tri_count;
		
		log_if_debug("build_aabbs: $ ($)", mdl_idx, triangle_count);
		hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["build_aabbs"],
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
		log_if_debug("collide_root_aabbs");
		auto aabb_collision_flags_host = make_unique<uint32_t[]>(total_aabb_checks);
		hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["collide_root_aabbs"],
										 uint1 { total_aabb_checks },
										 uint1 { hlbvh_state.kernel_max_local_size["collide_root_aabbs"] },
										 aabbs,
										 total_aabb_checks,
										 uint32_t(model_count),
										 aabb_collision_flags);
		
		// read back aabb collision flags
		aabb_collision_flags->read(*hlbvh_state.cqueue, aabb_collision_flags_host.get());
		
		// we're only interested in further constructing+colliding bvhs for meshes whose root aabbs collide with something
		// and also only the resp. collision pairs
		for (uint32_t lin_idx = 0; lin_idx < total_aabb_checks; ++lin_idx) {
			if (aabb_collision_flags_host[lin_idx] != 0) {
				// reverse cantor (map 1D linear index onto "half triangle" of a square -> all unique combinations of (i, j), with i != j)
				const auto q = (uint32_t)((const_math::EPSILON<float> + sqrt(1.0f + 8.0f * float(lin_idx))) * 0.5f - 0.5f);
				const auto i = lin_idx - (q * (q + 1u)) / 2u;
				const auto j = uint32_t(model_count) - q + i - 1u;
				valid_meshes.insert(i);
				valid_meshes.insert(j);
				potential_pairs.emplace_back(i, j);
				log_if_debug("possible collision: $ <-> $", i, j);
			}
		}
	}
	
	
	// compute bvh
	for (const auto& i : valid_meshes) {
		const auto& mdl = models[i];
		const auto cur_frame = mdl->cur_frame, next_frame = mdl->next_frame;
		const auto triangle_count = mdl->tri_count;
		const auto morton_codes_count = uint32_t(mdl->morton_codes->get_size() / sizeof(uint2));
		
		log_if_debug("compute_morton_codes: $", i);
		hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["compute_morton_codes"],
										 uint1 { morton_codes_count },
										 uint1 { hlbvh_state.kernel_max_local_size["compute_morton_codes"] },
										 aabbs,
										 mdl->frames_centroids_buffer[cur_frame],
										 mdl->frames_centroids_buffer[next_frame],
										 triangle_count,
										 i,
										 mdl->step,
										 mdl->morton_codes);
		
		log_if_debug("radix: $", i);
		radix_sort(mdl->morton_codes, mdl->morton_codes_ping, morton_codes_count, 30);
		
		//
		const auto leaf_count = triangle_count;
		const auto internal_node_count = leaf_count - 1u;
		log_if_debug("build_bvh: $ (node count: $/$)", i, leaf_count, internal_node_count);
		hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["build_bvh"],
										 uint1 { internal_node_count },
										 uint1 { hlbvh_state.kernel_max_local_size["build_bvh"] },
										 mdl->morton_codes,
										 mdl->bvh_internal,
										 mdl->bvh_leaves,
										 internal_node_count);
		
		log_if_debug("build_bvh_aabbs_leaves: $", i);
		hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["build_bvh_aabbs_leaves"],
										 uint1 { leaf_count },
										 uint1 { hlbvh_state.kernel_max_local_size["build_bvh_aabbs_leaves"] },
										 mdl->morton_codes,
										 leaf_count,
										 mdl->triangles,
										 mdl->bvh_aabbs_leaves);
		
		log_if_debug("build_bvh_aabbs: $", i);
		hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["build_bvh_aabbs"],
										 uint1 { leaf_count },
										 uint1 { hlbvh_state.kernel_max_local_size["build_bvh_aabbs"] },
										 mdl->bvh_internal,
										 mdl->bvh_leaves,
										 leaf_count,
										 mdl->bvh_aabbs,
										 mdl->bvh_aabbs_leaves,
										 mdl->bvh_aabbs_counters);
	}
	// collide all potential mesh collision pairs with each other
	for (const auto& col_pair : potential_pairs) {
		const auto& i = col_pair.x;
		const auto& j = col_pair.y;
		const auto& mdl_i = models[i];
		const auto& mdl_j = models[j];
		log_if_debug("collide: $ $", i, j);
		
		const auto leaf_count_i = mdl_i->tri_count;
		const auto leaf_count_j = mdl_j->tri_count;
		
		if (hlbvh_state.triangle_vis) {
			hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["collide_bvhs_tri_vis"],
											 uint1 { leaf_count_i },
											 uint1 { hlbvh_state.kernel_max_local_size["collide_bvhs_tri_vis"] },
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
											 collision_flags,
											 mdl_i->colliding_triangles,
											 mdl_j->colliding_triangles);
		} else {
			hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["collide_bvhs_no_tri_vis"],
											 uint1 { leaf_count_i },
											 uint1 { hlbvh_state.kernel_max_local_size["collide_bvhs_no_tri_vis"] },
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
											 collision_flags);
		}
	}
	
	//
	const auto stop_time = floor_timer::stop<chrono::microseconds>(start_time);
	if (hlbvh_state.benchmark) {
		log_debug("time: $ms", ((long double)stop_time) / 1000.0L);
	}
	
	// copy to host + return
	// TODO: share buffer with gl/metal instead? (unless doing cpu side checking)
	collision_flags->read(*hlbvh_state.cqueue, &collision_flags_host[0]);
#if 0 // log collided models
	string collision_str = "collisions: ";
	for(const auto& col_flag : collision_flags_host) {
		collision_str += (col_flag > 0 ? "1 " : "0 ");
	}
	log_msg("$", collision_str);
#endif
	
	if (hlbvh_state.triangle_vis) {
		for (uint32_t i = 0; i < uint32_t(model_count); ++i) {
			const auto& mdl = models[i];
			const auto cur_frame = mdl->cur_frame;
			const auto triangle_count = mdl->tri_count;
			if (collision_flags_host[i] > 0) {
				hlbvh_state.cqueue->execute_sync(*hlbvh_state.kernels["map_collided_triangles"],
												 uint1 { triangle_count },
												 uint1 { hlbvh_state.kernel_max_local_size["map_collided_triangles"] },
												 mdl->colliding_triangles,
												 mdl->frames_indices[cur_frame],
												 mdl->colliding_vertices,
												 triangle_count);
			}
		}
	}
}

void collider::radix_sort(shared_ptr<compute_buffer> buffer,
						  shared_ptr<compute_buffer> ping_buffer,
						  const uint32_t count,
						  const uint32_t max_bit) {
	if (!valid_counts_buffer) {
		valid_counts_buffer = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, COMPACTION_GROUP_COUNT * sizeof(uint32_t),
															  COMPUTE_MEMORY_FLAG::READ_WRITE);
		valid_counts_buffer->set_debug_label("valid_counts_buffer");
	}
	if (bit_buffers.empty()) {
		array<uint32_t, 4> data; // must have at least 16 bytes in the buffer, even if we only use 4 bytes
		for (uint32_t bit = 0u; bit < 32u; ++bit) {
			data[0] = 1u << bit; // mask op bit
			auto bit_buf = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, { (uint8_t*)data.data(), data.size() * sizeof(uint32_t) },
														   COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
			bit_buf->set_debug_label("bit_buffer:" + to_string(bit) + ":mask=" + to_string(data[0]));
			bit_buffers.emplace_back(bit_buf);
		}
	}
	if (!rs_params_buffer) {
		rs_params_buffer = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, sizeof(indirect_radix_sort_params_t),
														   COMPUTE_MEMORY_FLAG::READ |
														   COMPUTE_MEMORY_FLAG::HOST_WRITE |
														   COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT);
		rs_params_buffer->set_debug_label("rs_params_buffer");
	}
	
	log_if_debug("radix sort: count: $, max-bit: $", count, max_bit);
	if (hlbvh_state.cdev->indirect_compute_command_support) {
		const auto& indirect_radix_sort_count_kernel = *hlbvh_state.kernels["indirect_radix_sort_count"];
		const auto& indirect_radix_sort_prefix_sum_kernel = *hlbvh_state.kernels["radix_sort_prefix_sum"];
		const auto& indirect_radix_sort_stream_split_kernel = *hlbvh_state.kernels["indirect_radix_sort_stream_split"];
		
		if (!radix_sort_pipeline || radix_sort_pipeline_max_bit != max_bit) {
			radix_sort_pipeline_max_bit = max_bit;
			indirect_command_description desc {
				.command_type = indirect_command_description::COMMAND_TYPE::COMPUTE,
				.max_command_count = max_bit * 3u,
				.debug_label = "radix_sort_pipeline"
			};
			desc.compute_buffer_counts_from_functions(*hlbvh_state.cdev, {
				&indirect_radix_sort_count_kernel,
				&indirect_radix_sort_prefix_sum_kernel,
				&indirect_radix_sort_stream_split_kernel,
			});
			radix_sort_pipeline = hlbvh_state.cctx->create_indirect_command_pipeline(desc);
			if (!radix_sort_pipeline->is_valid()) {
				throw runtime_error("failed to create indirect radix sort pipeline");
			}
		} else {
			radix_sort_pipeline->reset();
		}
		
		const auto count_per_group = (count + COMPACTION_GROUP_COUNT - 1u) / COMPACTION_GROUP_COUNT;
		const indirect_radix_sort_params_t indirect_radix_sort_params {
			count,
			count_per_group
		};
		rs_params_buffer->write(*hlbvh_state.cqueue, &indirect_radix_sort_params);
		
		assert((max_bit % 2u) == 0u);
		auto cur_buffer = buffer.get();
		auto cur_ping_buffer = ping_buffer.get();
		for (uint32_t bit = 0u; bit < max_bit; ++bit) {
			radix_sort_pipeline->add_compute_command(*hlbvh_state.cdev, indirect_radix_sort_count_kernel)
				.set_arguments(cur_buffer,
							   rs_params_buffer,
							   bit_buffers[bit],
							   valid_counts_buffer)
				.execute(COMPACTION_GROUP_COUNT * COMPACTION_GROUP_SIZE, COMPACTION_GROUP_SIZE)
				.barrier();
			radix_sort_pipeline->add_compute_command(*hlbvh_state.cdev, indirect_radix_sort_prefix_sum_kernel)
				.set_arguments(valid_counts_buffer)
				.execute(PREFIX_SUM_GROUP_SIZE, PREFIX_SUM_GROUP_SIZE)
				.barrier();
			radix_sort_pipeline->add_compute_command(*hlbvh_state.cdev, indirect_radix_sort_stream_split_kernel)
				.set_arguments(cur_buffer,
							   cur_ping_buffer,
							   rs_params_buffer,
							   bit_buffers[bit],
							   valid_counts_buffer)
				.execute(COMPACTION_GROUP_COUNT * COMPACTION_GROUP_SIZE, COMPACTION_GROUP_SIZE)
				.barrier();
			swap(cur_buffer, cur_ping_buffer);
		}
		radix_sort_pipeline->complete();
		
		const compute_queue::indirect_execution_parameters_t exec_params {
			.wait_until_completion = true,
			.debug_label = "radix_sort",
		};
		hlbvh_state.cqueue->execute_indirect(*radix_sort_pipeline, exec_params);
	} else {
		const auto count_arg = uint32_t(count);
		const auto count_per_group_arg = uint32_t(count / COMPACTION_GROUP_COUNT);
		
		compute_queue::execution_parameters_t radix_sort_count_params {
			.execution_dim = 1u,
			.global_work_size = uint1 { COMPACTION_GROUP_COUNT * COMPACTION_GROUP_SIZE },
			.local_work_size = uint1 { COMPACTION_GROUP_SIZE },
			.wait_until_completion = true,
			.debug_label = "radix_sort_count",
		};
		compute_queue::execution_parameters_t radix_sort_prefix_sum_params {
			.execution_dim = 1u,
			.global_work_size = uint1 { PREFIX_SUM_GROUP_SIZE },
			.local_work_size = uint1 { PREFIX_SUM_GROUP_SIZE },
			.args = { valid_counts_buffer },
			.wait_until_completion = true,
			.debug_label = "radix_sort_prefix_sum",
		};
		compute_queue::execution_parameters_t radix_sort_stream_split_params {
			.execution_dim = 1u,
			.global_work_size = uint1 { COMPACTION_GROUP_COUNT * COMPACTION_GROUP_SIZE },
			.local_work_size = uint1 { COMPACTION_GROUP_SIZE },
			.wait_until_completion = true,
			.debug_label = "radix_sort_stream_split",
		};
		
		for (uint32_t bit = 0u; bit < max_bit; ++bit) {
			const auto mask_op_bit = uint32_t(1u << bit);
			//log_if_debug("radix sort: $, $X, count: $", bit, mask_op_bit, count);
			
			radix_sort_count_params.args = {
				buffer,
				count_arg,
				count_per_group_arg,
				mask_op_bit,
				valid_counts_buffer
			};
			radix_sort_stream_split_params.args = {
				buffer,
				ping_buffer,
				count_arg,
				count_per_group_arg,
				mask_op_bit,
				valid_counts_buffer
			};
			hlbvh_state.cqueue->execute_with_parameters(*hlbvh_state.kernels["radix_sort_count"], radix_sort_count_params);
			hlbvh_state.cqueue->execute_with_parameters(*hlbvh_state.kernels["radix_sort_prefix_sum"], radix_sort_prefix_sum_params);
			hlbvh_state.cqueue->execute_with_parameters(*hlbvh_state.kernels["radix_sort_stream_split"], radix_sort_stream_split_params);
			
			buffer.swap(ping_buffer);
		}
	}
	log_if_debug("radix done");
}
