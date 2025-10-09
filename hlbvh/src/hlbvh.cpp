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

#include "hlbvh_state.hpp"

#if defined(FLOOR_DEVICE)

#if defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/common.hpp>
#endif

#include "triangle_intersection.hpp"

// bvh stuff
#define LEAF_MASK 0x80000000u
#define LEAF_INV_MASK 0x7FFFFFFFu
#define LEAF_FLAG(index) (index | LEAF_MASK)

static uint32_t morton(uint32_t x, uint32_t y, uint32_t z) {
	// x, y and z are 10-bit numbers in the form
	// 00000000 00000000 000000A9 87654321
	// these need to be transformed into an interleaved format like
	// 00AAA999 88877766 65554443 33222111
	// order doesn't matter, but must be consistent -> will use XYZ order here
	//
	// input       : 00000000 00000000 000000A9 87654321
	// transform #0: 000000A9 00000000 00000000 87654321 (8-bit packs, 16 interleave)
	// transform #1: 000000A9 00000000 87650000 00004321 (4-bit packs, 8 interleave)
	// transform #2: 000000A9 00008700 00650000 43000021 (2-bit packs, 4 interleave)
	// transform #3: 0000A009 00800700 60050040 03002001 (1-bit packs, 2 interleave)
	// combination : 00AAA999 88877766 65554443 33222111
	//
	// for reference:
	// * http://graphics.stanford.edu/~seander/bithacks.html#InterleaveTableObvious
	// * http://stackoverflow.com/a/1024889
	
	x = (x | (x << 16u)) & 0x030000FFu;
	y = (y | (y << 16u)) & 0x030000FFu;
	z = (z | (z << 16u)) & 0x030000FFu;
	
	x = (x | (x <<  8u)) & 0x0300F00Fu;
	y = (y | (y <<  8u)) & 0x0300F00Fu;
	z = (z | (z <<  8u)) & 0x0300F00Fu;
	
	x = (x | (x <<  4u)) & 0x030C30C3u;
	y = (y | (y <<  4u)) & 0x030C30C3u;
	z = (z | (z <<  4u)) & 0x030C30C3u;
	
	x = (x | (x <<  2u)) & 0x09249249u;
	y = (y | (y <<  2u)) & 0x09249249u;
	z = (z | (z <<  2u)) & 0x09249249u;
	
	// total: 12 ANDS, 12 ORs, 12 SHLs (val double use, need 1 temporary, thus 4 registers)
	// sm_50: 12 SHLs, 12 LOP3.LUTs
	
	// note: relative cost == relative to the max throughput (aka in relation to flops)
	// e.g. 192 flops possible, only 32 *int ops possible -> cost factor is 6
	// -> 5.4.1 cuda programming guide
	
	// sm_20: 12 * 1 + 12 * 1 + 12 * 2 = 48 -> 1.5x
	// sm_21: 12 * 1 + 12 * 1 + 12 * 3 = 60 -> 1.66x
	// sm_30: 12 * 1.2 + 12 * 1.2 + 12 * 6 = 100.8 -> 2.8x
	// sm_35 (geforce): 12 * 1.2 + 12 * 1.2 + 12 * 6 = 100.8 -> 2.8x
	// sm_35 (tesla/quadro): 12 * 1.2 + 12 * 1.2 + 12 * 3 = 64.8 -> 1.8x
	// sm_50: 12 * 1 + 12 * 1 = 24 -> 1x
	
	return x | (y << 1u) | (z << 2u);
}

// computes all interpolated triangles for this frame, stores them in a global buffer,
// and continues to build the root aabb of the mesh (which will be needed later on)
kernel_1d(ROOT_AABB_GROUP_SIZE) void build_aabbs_and_init_bvh(buffer<const float3> triangles_cur,
															  buffer<const float3> triangles_next,
															  param<uint32_t> triangle_count,
															  param<uint32_t> mesh_idx,
															  param<float> interp,
															  buffer<float> aabbs,
															  buffer<float3> triangles,
															  buffer<uint3> bvh_internal) {
	const auto idx = global_id.x;
	bboxf aabb; // defaults to invalid extent
	if (idx < triangle_count) {
		const auto v0 = triangles_cur[idx * 3].interpolated(triangles_next[idx * 3], interp);
		const auto v1 = triangles_cur[idx * 3 + 1].interpolated(triangles_next[idx * 3 + 1], interp);
		const auto v2 = triangles_cur[idx * 3 + 2].interpolated(triangles_next[idx * 3 + 2], interp);
		triangles[idx * 3] = v0;
		triangles[idx * 3 + 1] = v1;
		triangles[idx * 3 + 2] = v2;
		aabb.min = v0.minned(v1).minned(v2);
		aabb.max = v0.maxed(v1).maxed(v2);
		if (idx == 0) {
			bvh_internal[0].z = 0u;
		}
	}
	
	// min/max reduce
	local_buffer<float3, algorithm::reduce_local_memory_elements<ROOT_AABB_GROUP_SIZE, float3, algorithm::group::OP::MIN>()> lmem_aabbs;
	aabb.min = algorithm::reduce_min<ROOT_AABB_GROUP_SIZE>(aabb.min, lmem_aabbs);
	local_barrier();
	aabb.max = algorithm::reduce_max<ROOT_AABB_GROUP_SIZE>(aabb.max, lmem_aabbs);
	if (local_id.x == 0) {
		atomic_min(&aabbs[mesh_idx * 6 + 0], aabb.min.x);
		atomic_min(&aabbs[mesh_idx * 6 + 1], aabb.min.y);
		atomic_min(&aabbs[mesh_idx * 6 + 2], aabb.min.z);
		atomic_max(&aabbs[mesh_idx * 6 + 3], aabb.max.x);
		atomic_max(&aabbs[mesh_idx * 6 + 4], aabb.max.y);
		atomic_max(&aabbs[mesh_idx * 6 + 5], aabb.max.z);
	}
}

kernel_1d() void compute_morton_codes(buffer<const bboxf> aabbs,
									  buffer<const float3> centroids_cur,
									  buffer<const float3> centroids_next,
									  param<uint32_t> triangle_count,
									  param<uint32_t> mesh_idx,
									  param<float> interp,
									  buffer<uint32_t> morton_codes_keys,
									  buffer<uint16_t> morton_codes_values) {
	const auto idx = global_id.x;
	if (idx >= triangle_count) {
		return;
	}
	
	// get the bounding box of the resp. mesh
	const auto mesh_bbox = aabbs[mesh_idx];
	
	// compute the centroid for this id
	auto coord = centroids_cur[idx].interpolated(centroids_next[idx], interp);
	
	// scale to [0, 1]
	coord = (coord - mesh_bbox.min).abs() / (mesh_bbox.max - mesh_bbox.min);
	// scale to [0, 1024[ or [0, 1023] as integer (so it fits into 10-bit)
	const auto scaled_coord = uint3(coord * 1024.0f).min(1023u);
	// compute the morton code for this (x, y, z)
	const auto morton_code = morton(scaled_coord.x, scaled_coord.y, scaled_coord.z);
	
	// store the morton code
	morton_codes_keys[idx] = morton_code;
	morton_codes_values[idx] = uint16_t(idx);
}

// NOTE: prefix = clz(morton code ^ morton code)
// this is getting to ugly ...
#define prefix_checked(a, b, c) prefix_checked_int_(a, b, c, morton_codes_keys, internal_node_count)
static int32_t prefix_checked_int_(const uint32_t mc_i,
								   const uint32_t i,
								   const int32_t j,
								   global const uint32_t* morton_codes_keys,
								   const uint32_t internal_node_count) {
	// out of range check (i) and (j)
	if (mc_i > 0x3FFF'FFFFu || j < 0 || uint32_t(j) > internal_node_count) {
		return -1;
	}
	// get morton code for j
	const uint32_t mc_j = morton_codes_keys[j];
	// check for identical morton codes
	return (mc_i == mc_j ?
			// "simply use i and j as a fallback if k_i = k_j when evaluating delta(i, j)"
			// also: since both morton codes match, add 32 to the matched prefix length
			32 + math::clz(i ^ uint32_t(j)) :
			// else: use the actual morton codes (the general case)
			math::clz(mc_i ^ mc_j));
}
static int32_t prefix_unchecked(const uint32_t mc_i, const uint32_t mc_j) {
	return math::clz(mc_i ^ mc_j);
}

kernel_1d() void build_bvh(buffer<const uint32_t> morton_codes_keys,
						   buffer<uint3> bvh_internal,
						   buffer<uint32_t> bvh_leaves,
						   param<uint32_t> internal_node_count) {
	const auto idx = global_id.x;
	if (idx >= internal_node_count) {
		return;
	}
	
	// credits: https://research.nvidia.com/sites/default/files/publications/karras2012hpg_paper.pdf
	
	// -> determine_range
	// determine direction of the range (+1 or -1)
	const auto mc_idx = morton_codes_keys[idx];
	const int prefix_prev = (idx > 0 ? prefix_checked(mc_idx, idx, int(idx) - 1) : -1);
	const int prefix_next = prefix_checked(mc_idx, idx, int(idx) + 1);
	const int d = (prefix_next - prefix_prev < 0 ? -1 : 1);
	
	// compute upper bound for the length of the range
	const int delta_min = (d < 0 ? prefix_next : prefix_prev);
	int l_max = 2;
	while (prefix_checked(mc_idx, idx, int(idx) + l_max * d) > delta_min) {
		l_max <<= 1;
	}
	// TODO: use paper hint with l_max <<= 2u and starting at 128? --good enough for now
	
	// find the other end using binary search
	int l = 0;
	for (int t = l_max >> 1; t > 0; t >>= 1) {
		if (prefix_checked(mc_idx, idx, int(idx) + (l + t) * d) > delta_min) {
			l += t;
		}
	}
	
	// note: not in the paper, but we have to actually make sure that: range begin < range end
	const auto j = uint32_t(int(idx) + l * d);
	const uint2 range {
		d >= 0 ? idx : uint32_t(j),
		d >= 0 ? uint32_t(j) : idx
	};
	
	// -> find_split
	// credits: http://devblogs.nvidia.com/parallelforall/thinking-parallel-part-iii-tree-construction-gpu
	const auto mc_begin = morton_codes_keys[range.x];
	const auto mc_end = morton_codes_keys[range.y];
	uint32_t split = 0u;
	if (mc_begin != mc_end) {
		const auto common_prefix = prefix_unchecked(mc_begin, mc_end);
		split = range.x;
		auto step = range.y - range.x;
		
		do {
			step = (step + 1u) >> 1u;
			const auto new_split = split + step;
			
			if (new_split < range.y) {
				const auto split_code = morton_codes_keys[new_split];
				const auto split_prefix = prefix_unchecked(mc_begin, split_code);
				split = (split_prefix > common_prefix ? new_split : split);
			}
		} while(step > 1u);
	} else {
		// Karras says that the split is in the middle of the range if the morton codes are identical,
		// and simply uses "(range.x + range.y) >> 1", but this is wrong under certain conditions
		// -> use the same code as above, but with checked parameters
		// note that this shouldn't influence general performance, because it happens only very rarely
		const auto common_prefix = prefix_checked(mc_begin, range.x, int(range.y));
		split = range.x;
		auto step = range.y - range.x;
		
		do {
			step = (step + 1u) >> 1u;
			const auto new_split = split + step;
			
			if (new_split < range.y) {
				const auto split_prefix = prefix_checked(mc_begin, range.x, int(new_split));
				split = (split_prefix > common_prefix ? new_split : split);
			}
		} while(step > 1u);
	}
	
	// output child pointers
	// note: to differentiate between internal and leaf nodes, leaf nodes have their highest bit set to 1
	const auto left_idx = split;
	const auto right_idx = split + 1u;
	
	bvh_internal[idx].x = (range.x == left_idx ? LEAF_FLAG(left_idx) : left_idx);
	bvh_internal[idx].y = (range.y == right_idx ? LEAF_FLAG(right_idx) : right_idx);
	
	if (range.x == left_idx) {
		bvh_leaves[left_idx] = idx;
	} else {
		bvh_internal[left_idx].z = uint32_t(idx);
	}
	
	if (range.y == right_idx) {
		bvh_leaves[right_idx] = idx;
	} else {
		bvh_internal[right_idx].z = uint32_t(idx);
	}
}

kernel_1d() void build_bvh_aabbs_leaves(buffer<const uint16_t> morton_codes_values,
										param<uint32_t> leaf_count,
										buffer<const float3> triangles,
										buffer<bboxf> bvh_aabbs_leaves) {
	const auto idx = global_id.x;
	if (idx >= leaf_count) {
		return;
	}
	
	// load triangle
	const auto tri_id = morton_codes_values[idx];
	const auto v0 = triangles[tri_id * 3];
	const auto v1 = triangles[tri_id * 3 + 1];
	const auto v2 = triangles[tri_id * 3 + 2];
	
	// compute aabb for this leaf
	bvh_aabbs_leaves[idx] = { v0.minned(v1).minned(v2), v0.maxed(v1).maxed(v2) };
}

kernel_1d() void build_bvh_aabbs(buffer<const uint3> bvh_internal,
								 buffer<const uint32_t> bvh_leaves,
								 param<uint32_t> leaf_count,
								 buffer<bboxf> bvh_aabbs,
								 buffer<const bboxf> bvh_aabbs_leaves,
								 buffer<uint32_t> counters) {
	const auto idx = global_id.x;
	if (idx >= leaf_count) {
		return;
	}
	
	auto parent = bvh_leaves[idx];
	for (;;) {
		// "the first thread terminates immediately while the second one gets to process the node"
		// counter old/return value is 0 for the first thread, 1 for the second -> process if 1
		if (atomic_inc(&counters[parent]) != 1u) {
			break;
		}
		
		// straightforward: grab left and right aabb, compute their min/max, store it in the current node
		const auto node = bvh_internal[parent];
		const auto masked_left_idx = (node.x & LEAF_INV_MASK); // leaf node if highest bit set
		const auto masked_right_idx = (node.y & LEAF_INV_MASK);
		
		const auto b_left = (masked_left_idx != node.x ? bvh_aabbs_leaves[masked_left_idx] : bvh_aabbs[masked_left_idx]);
		const auto b_right = (masked_right_idx != node.y ? bvh_aabbs_leaves[masked_right_idx] : bvh_aabbs[masked_right_idx]);
		
		bvh_aabbs[parent] = b_left.extended(b_right);
		
		// unless we're at the root, onto the next parent node
		if (parent == 0) [[unlikely]] {
			break;
		}
		parent = node.z;
	}
}

static inline bool check_overlap(const bboxf lhs, const bboxf rhs) {
#if 1
	if (lhs.min.x > rhs.max.x ||
		rhs.min.x > lhs.max.x ||
		lhs.min.y > rhs.max.y ||
		rhs.min.y > lhs.max.y ||
		lhs.min.z > rhs.max.z ||
		rhs.min.z > lhs.max.z) {
		return false;
	}
	return true;
#else
	if (min_a.x <= max_b.x &&
		min_a.y <= max_b.y &&
		min_a.z <= max_b.z &&
		max_a.x >= min_b.x &&
		max_a.y >= min_b.y &&
		max_a.z >= min_b.z) {
		return true;
	}
	return false;
#endif
}

static const_array<float3, 3> read_triangle(buffer<const float3>& triangles, const uint32_t triangle_idx) {
	return {{
		triangles[triangle_idx * 3u],
		triangles[triangle_idx * 3u + 1u],
		triangles[triangle_idx * 3u + 2u]
	}};
}

//! max stack size (element count) of the traversal stack used in collide_bvhs()
static constexpr const uint32_t collision_stack_size_per_item { 64u };
//! data type used in the stack in collide_bvhs(), i.e. index type
//! NOTE: this can be 16-bit, because we can guarantee that we have < 65536 triangles
using collsion_stack_data_type = uint16_t;

template <bool triangle_vis, uint32_t tile_size>
floor_inline_always static void collide_bvhs(// the leaves of bvh A that we want to collide with bvh B
											 const uint32_t leaf_count_a,
											 buffer<const bboxf>& bvh_aabbs_leaves_a,
											 buffer<const float3>& triangles_a,
											 buffer<const uint16_t>& morton_codes_values_a,
											 // the complete bvh B
											 const uint32_t internal_node_count_b floor_unused,
											 buffer<const uint3>& bvh_internal_b,
											 buffer<const bboxf>& bvh_aabbs_b,
											 buffer<const bboxf>& bvh_aabbs_leaves_b,
											 buffer<const float3>& triangles_b,
											 buffer<const uint16_t>& morton_codes_values_b,
											 // mesh indices of A and B
											 const uint32_t mesh_idx_a,
											 const uint32_t mesh_idx_b,
											 // flags if resp. mesh A/B collides with anything
											 // also (ab)used as an abort condition here
											 buffer<uint32_t>& collision_flags,
											 std::conditional_t<triangle_vis, buffer<uint32_t>&, int> colliding_triangles_a,
											 std::conditional_t<triangle_vis, buffer<uint32_t>&, int> colliding_triangles_b) {
	const auto idx = global_id.x;
	if (idx >= leaf_count_a) {
		return;
	}
	
	// leaf aabb
	const auto leaf_bbox = bvh_aabbs_leaves_a[idx];
	
	//
	local_buffer<collsion_stack_data_type, collision_stack_size_per_item * tile_size> stack;
	auto stack_ptr = &stack[local_id.x * collision_stack_size_per_item];
	*stack_ptr++ = 0; // push
	
	// traverse nodes starting from the root
	uint16_t node = 0;
	do {
		if constexpr(!triangle_vis) {
			// check abort condition (no need to do further checking when a collision has been found already)
			// NOTE: need to check both, because either could have been set previously
			// NOTE: if both have been set to true previously, this will also immediately return (as it should)
			if (collision_flags[mesh_idx_a] > 0 && collision_flags[mesh_idx_b] > 0) {
				break;
			}
		}
		
		bool traverse = false;
#pragma unroll
		for (uint32_t i = 0; i < 2; ++i) {
			// check child node for overlap
			const auto child = (i == 0 ? bvh_internal_b[node].x : bvh_internal_b[node].y);
			const auto masked_idx = (child & LEAF_INV_MASK); // leaf node if highest bit set
			const bool is_leaf = (child != masked_idx);
			
			// get aabb for the left and right child and check for overlap
			const auto child_bbox = (is_leaf ? bvh_aabbs_leaves_b[masked_idx] : bvh_aabbs_b[masked_idx]);
			
			// query overlaps a leaf node
			if (check_overlap(leaf_bbox, child_bbox)) {
				if (is_leaf) {
					// read triangle for this leaf node
					const auto overlap_triangle_idx = morton_codes_values_b[masked_idx];
					const auto ov = read_triangle(triangles_b, overlap_triangle_idx);
					
					// read triangle for query node (leaf triangle)
					// NOTE: this is faster / uses less registers than reading the triangle outside/before the traversal loop!
					const auto triangle_idx = morton_codes_values_a[idx];
					const auto v = read_triangle(triangles_a, triangle_idx);
					
					if (check_triangle_intersection(v[0], v[1], v[2], ov[0], ov[1], ov[2])) {
						atomic_inc(&collision_flags[mesh_idx_a]);
						atomic_inc(&collision_flags[mesh_idx_b]);
						if constexpr (triangle_vis) {
							atomic_inc(&colliding_triangles_a[triangle_idx]);
							atomic_inc(&colliding_triangles_b[overlap_triangle_idx]);
						}
					}
				} else {
					// query overlaps an internal node => traverse
					// -> set next node to left child (i == 0) or right child (if not traversing left child)
					node = (i == 0 || !traverse ? (uint16_t)child : node);
					// -> at right child and traversing left child: push right child onto the stack
					if (i == 1 && traverse) {
						*stack_ptr++ = (uint16_t)child; // push
					}
					traverse = true;
				}
			}
		}
		// not traversing either child, pop next node from stack
		if (!traverse) {
			node = *--stack_ptr;
		}
	} while (node != 0);
}

//! computes the max possible local size that can be used for collide_bvhs() based on available local memory size
static constexpr uint32_t compute_collide_max_local_size() {
	const auto local_mem_size = device_info::dedicated_local_memory();
	const auto stack_size_per_item = collision_stack_size_per_item * sizeof(collsion_stack_data_type);
	if (local_mem_size >= 16384u) {
		// max possible local size
		auto possible_local_size = uint32_t(local_mem_size / stack_size_per_item);
		// we can't use this directly, because we need to ensure that specific local sizes are specified
		// -> must be a multiple of 32
		if ((possible_local_size % 32) != 0u) {
			possible_local_size = (possible_local_size / 32u) * 32u;
		}
		// if the device requires that the work-group size X is a power-of-two, round down to the next one
		if constexpr (device_info::requires_work_group_size_x_is_pot()) {
			possible_local_size = const_math::prev_pot(possible_local_size);
		}
		return possible_local_size;
	}
	// otherwise: assume at least 16KiB are available
	return 128u;
}

// NOTE: this also demonstrates that we can use a constexpr function to specify the required local size
kernel_1d(compute_collide_max_local_size()) void collide_bvhs_no_tri_vis(buffer<const bboxf> bvh_aabbs_leaves_a,
																		 buffer<const float3> triangles_a,
																		 buffer<const uint16_t> morton_codes_values_a,
																		 buffer<const uint3> bvh_internal_b,
																		 buffer<const bboxf> bvh_aabbs_b,
																		 buffer<const bboxf> bvh_aabbs_leaves_b,
																		 buffer<const float3> triangles_b,
																		 buffer<const uint16_t> morton_codes_values_b,
																		 buffer<uint32_t> collision_flags,
																		 param<collide_params_t> params) {
	collide_bvhs<false, compute_collide_max_local_size()>(params.leaf_count_a, bvh_aabbs_leaves_a, triangles_a, morton_codes_values_a,
														  params.internal_node_count_b, bvh_internal_b, bvh_aabbs_b, bvh_aabbs_leaves_b,
														  triangles_b, morton_codes_values_b,
														  params.mesh_idx_a, params.mesh_idx_b, collision_flags, 0, 0);
}

kernel_1d(compute_collide_max_local_size()) void collide_bvhs_tri_vis(buffer<const bboxf> bvh_aabbs_leaves_a,
																	  buffer<const float3> triangles_a,
																	  buffer<const uint16_t> morton_codes_values_a,
																	  buffer<const uint3> bvh_internal_b,
																	  buffer<const bboxf> bvh_aabbs_b,
																	  buffer<const bboxf> bvh_aabbs_leaves_b,
																	  buffer<const float3> triangles_b,
																	  buffer<const uint16_t> morton_codes_values_b,
																	  buffer<uint32_t> collision_flags,
																	  buffer<uint32_t> colliding_triangles_a,
																	  buffer<uint32_t> colliding_triangles_b,
																	  param<collide_params_t> params) {
	collide_bvhs<true, compute_collide_max_local_size()>(params.leaf_count_a, bvh_aabbs_leaves_a, triangles_a, morton_codes_values_a,
														 params.internal_node_count_b, bvh_internal_b, bvh_aabbs_b, bvh_aabbs_leaves_b,
														 triangles_b, morton_codes_values_b,
														 params.mesh_idx_a, params.mesh_idx_b, collision_flags, colliding_triangles_a, colliding_triangles_b);
}

kernel_1d() void collide_root_aabbs(buffer<const bboxf> aabbs,
									param<uint32_t> total_aabb_checks,
									param<uint32_t> mesh_count,
									buffer<uint32_t> aabb_collision_flags) {
	const auto id = global_id.x;
	if(id >= total_aabb_checks) return;
	
	// reverse cantor (map 1D linear index onto "half triangle" of a square -> all unique combinations of (i, j), with i != j)
	const auto q = (uint32_t)math::fma(const_math::EPSILON<float> + math::sqrt(1.0f + 8.0f * float(id)), 0.5f, -0.5f);
	const auto i = id - (q * (q + 1u)) / 2u;
	const auto j = mesh_count - q + i - 1u;
	
	// get i and j aabb and check for overlap
	const auto bbox_lhs = aabbs[i];
	const auto bbox_rhs = aabbs[j];
	
	if (check_overlap(bbox_lhs, bbox_rhs)) {
		atomic_inc(&aabb_collision_flags[id]);
	}
}

kernel_1d() void map_collided_triangles(buffer<const uint32_t> colliding_triangles,
										buffer<const uint3> indices,
										buffer<uint32_t> colliding_vertices,
										param<uint32_t> triangle_count) {
	const auto idx = global_id.x;
	if(idx >= triangle_count) return;
	
	const bool is_collision = (colliding_triangles[idx] > 0u);
	if(!is_collision) return;
	
	const auto index = indices[idx];
	atomic_inc(&colliding_vertices[index.x]);
	atomic_inc(&colliding_vertices[index.y]);
	atomic_inc(&colliding_vertices[index.z]);
}

//////////////////////////////////////////
// radix sort with indirect compute support

kernel_1d(COMPACTION_GROUP_SIZE) void indirect_radix_sort_count(buffer<const uint32_t> keys,
																buffer<const indirect_radix_sort_params_t> params,
																buffer<const uint32_t> mask_op_bit,
																buffer<uint32_t> valid_counts) {
	const auto lid = local_id.x;
	const auto gid = group_id.x;
	
	uint32_t counter = 0;
	for (uint32_t pair_id = lid + gid * params->count_per_group;
		 pair_id < ((gid + 1u) * params->count_per_group) && pair_id < params->count;
		 pair_id += COMPACTION_GROUP_SIZE) {
		if ((keys[pair_id] & *mask_op_bit) == 0u) {
			++counter;
		}
	}
	
	// reduce + write final result (group sum)
	local_buffer<uint32_t, algorithm::reduce_local_memory_elements<COMPACTION_GROUP_SIZE, uint32_t>()> lmem;
	const auto reduced_value = algorithm::reduce_add<COMPACTION_GROUP_SIZE>(uint32_t(counter), lmem);
	if (lid == 0) {
		valid_counts[gid] = reduced_value;
	}
}

kernel_1d(PREFIX_SUM_GROUP_SIZE) void radix_sort_prefix_sum(buffer<uint32_t> in_out) {
	const auto idx = global_id.x;
	uint32_t val = (idx < COMPACTION_GROUP_COUNT ? in_out[idx] : 0);
	
	// work-group scan
	local_buffer<uint32_t, algorithm::scan_local_memory_elements<PREFIX_SUM_GROUP_SIZE, uint32_t>()> lmem;
	const auto result = algorithm::inclusive_scan_add<PREFIX_SUM_GROUP_SIZE>(val, lmem);
	
	if (idx < COMPACTION_GROUP_COUNT) {
		in_out[idx] = result;
	}
}

kernel_1d(COMPACTION_GROUP_SIZE) void indirect_radix_sort_stream_split(buffer<const uint32_t> src,
																	   buffer<uint32_t> dst,
																	   buffer<const uint16_t> src_values,
																	   buffer<uint16_t> dst_values,
																	   buffer<const indirect_radix_sort_params_t> params,
																	   buffer<const uint32_t> mask_op_bit,
																	   buffer<const uint32_t> valid_counts) {
	const auto lid = local_id.x;
	const auto gid = group_id.x;
	auto valid_elem_offset = (gid == 0 ? 0 : valid_counts[gid - 1]);
	auto invalid_elem_offset = valid_counts[COMPACTION_GROUP_COUNT - 1] + gid * params->count_per_group - valid_elem_offset;
	
	// since we're using barriers in here, all work-items must always execute this
	// -> only abort once the base id is out of range
	local_buffer<uint32_t, algorithm::scan_local_memory_elements<COMPACTION_GROUP_SIZE, uint32_t>()> lmem;
	for (uint32_t base_id = gid * params->count_per_group;
		 base_id < ((gid + 1u) * params->count_per_group) && base_id < params->count;
		 base_id += COMPACTION_GROUP_SIZE) {
		// pair id/offset for this work-item + check if it is actually active
		const auto pair_id = base_id + lid;
		const auto is_active = (pair_id < ((gid + 1u) * params->count_per_group) && pair_id < params->count);
		
		const auto in_idx = (is_active ? pair_id : base_id /* base is always valid */);
		const auto cur_key = src[in_idx];
		const auto cur_value = src_values[in_idx];
		const auto valid = (is_active && (cur_key & *mask_op_bit) == 0u ? 1u : 0u);
		
		local_barrier();
		const auto result = algorithm::inclusive_scan_add<COMPACTION_GROUP_SIZE>(valid, lmem);
		
		if (is_active) {
			const auto out_idx = (valid ? valid_elem_offset + result - 1 :
								  invalid_elem_offset + lid + 1 - result - 1);
			dst[out_idx] = cur_key;
			dst_values[out_idx] = cur_value;
		}
		
		// NOTE: scan already does a local_barrier() at the end, so another one is unnecessary here
		if (lid == COMPACTION_GROUP_SIZE - 1) {
			lmem[0] = result + valid_elem_offset;
			lmem[1] = COMPACTION_GROUP_SIZE - result + invalid_elem_offset;
		}
		local_barrier();
		
		valid_elem_offset = lmem[0];
		invalid_elem_offset = lmem[1];
	}
}

#endif
