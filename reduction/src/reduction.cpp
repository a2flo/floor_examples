/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include "reduction.hpp"
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_coop.hpp>
#endif

#if defined(FLOOR_COMPUTE)

// POT sizes from 32 - 1024
#define POT_TILE_SIZES(F) \
F(32) \
F(64) \
F(128) \
F(256) \
F(512) \
F(1024)

///////////////////////////////////////////////////////////////////////////////
/// reduction

template <uint32_t tile_size, typename data_type>
floor_inline_always void reduce_add(buffer<const data_type> data, buffer<data_type> sum, const uint32_t count) {
	static_assert(sizeof(data_type) == 4 || sizeof(data_type) == 8, "invalid data type size (must be 4 or 8 bytes)");
	
	if constexpr(device_info::has_cooperative_kernel_support() && device_info::has_sub_group_shuffle()) {
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0 && defined(FLOOR_COMPUTE_INFO_CUDA_SM) && FLOOR_COMPUTE_INFO_CUDA_SM >= 60 /* TODO: proper define */
		// can use shuffle/swizzle + coop kernel
		static constexpr const uint32_t sub_group_width { max(1u, device_info::simd_width_min()) };
		static constexpr const uint32_t tile_count { max(1u, tile_size / sub_group_width) };
		static_assert(tile_count * sub_group_width == tile_size, "tile size must be a multiple of sub-group width");
		
		coop::global_group gg;
		const uint32_t item_count = gg.size();
		const uint32_t block_count = (count + (item_count - 1)) / item_count;
		uint32_t idx = group_id.x * tile_size + local_id.x;
		
		// accumulate locally
		data_type value = data_type(0);
		// TODO: different/linear stride?
		for (uint32_t i = 0; i < block_count; ++i, idx += item_count) {
			if(idx < count) {
				value += data[idx];
			}
		}
		
		// reduce in sub-group
		local_buffer<data_type, tile_count> lmem;
		auto red_val = compute_algorithm::sub_group_reduce_add(value);
		
		// reduce in work-group if necessary
		if constexpr(tile_count > 1) {
			if(sub_group_local_id == 0) {
				// sub-group item #0 writes to reduce value to local memory
				lmem[sub_group_id_1d] = red_val;
			}
			local_barrier();
			
			if (sub_group_id_1d == 0) {
				// sub-group #0 does the final reduction
				const auto sg_val = (uint32_t(sub_group_local_id) < tile_count ? lmem[sub_group_local_id] : data_type(0));
				red_val = compute_algorithm::sub_group_reduce_add(sg_val);
			}
		}
		
		// add to global sum (item #0 == sub-group item #0 in sub-group #0)
		if (local_id.x == 0) {
			atomic_add(&sum[0], red_val);
		}
#endif
	}
	else {
		// sum items in large blocks
		const auto per_item_count = (count + (global_size.x - 1u)) / global_size.x;
		const auto offset = per_item_count * tile_size * group_id.x;
		
#if defined(FLOOR_COMPUTE_INFO_TYPE_CPU)
		// CPU: sequential iteration over items per work-item, b/c it's executed this way
		uint32_t idx = offset + local_id.x * per_item_count;
		static constexpr const uint32_t idx_inc { 1u };
#else
		// GPU: work-group-size iteration over items per work-item
		uint32_t idx = offset + local_id.x;
		static constexpr const uint32_t idx_inc { tile_size };
#endif
		
		auto item_sum = data_type(0);
		for (uint32_t i = 0; i < per_item_count; ++i, idx += idx_inc) {
			if (idx < count) {
				item_sum += data[idx];
			}
		}
		
		if constexpr(device_info::has_sub_group_shuffle() && max(1u, device_info::simd_width_min()) <= tile_size) {
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
			// can use shuffle/swizzle
			static constexpr const uint32_t sub_group_width { max(1u, device_info::simd_width_min()) };
			static constexpr const uint32_t tile_count { max(1u, tile_size / sub_group_width) };
			static_assert(tile_count * sub_group_width == tile_size, "tile size must be a multiple of sub-group width");
			
			// reduce in sub-group first (via shuffle)
			const auto sg_red_val = compute_algorithm::sub_group_reduce_add(item_sum);
			
			if constexpr(tile_count == 1) {
				// special case where we only have one sub-group
				if(local_id.x == 0) {
					atomic_add(&sum[0], sg_red_val);
				}
			}
			else {
				local_buffer<data_type, tile_count> lmem;
				
				// write reduced sub-group value to local memory for the next step
				if(sub_group_local_id == 0) {
					lmem[sub_group_id_1d] = sg_red_val;
				}
				local_barrier();
				
				// final reduction of sub-group values
				if constexpr(tile_count <= sub_group_width) {
					// can do this directly in one sub-group
					if(sub_group_id_1d == 0) {
						const auto final_red_val = compute_algorithm::sub_group_reduce_add(tile_count == sub_group_width ?
																						   lmem[sub_group_local_id] :
																						   (sub_group_local_id < tile_count ?
																							lmem[sub_group_local_id] : data_type(0)));
						if(sub_group_local_id == 0) {
							atomic_add(&sum[0], final_red_val);
						}
					}
				}
				else if constexpr(tile_count <= sub_group_width * 2) {
					// tile count is at most 2x sub-group width
					if(sub_group_id_1d == 0) {
						const auto final_red_val = compute_algorithm::sub_group_reduce_add(sub_group_local_id * 2u < tile_count ?
																						   lmem[sub_group_local_id * 2u] +
																						   lmem[sub_group_local_id * 2u + 1u] :
																						   data_type(0));
						if(sub_group_local_id == 0) {
							atomic_add(&sum[0], final_red_val);
						}
					}
				}
				else {
					// do another round
					static constexpr const uint32_t tile_count_2nd { max(1u, tile_count / sub_group_width) };
					static_assert(tile_count_2nd <= sub_group_width, "invalid or too small sub-group width");
					if(sub_group_id_1d < tile_count_2nd) {
						const auto sg_red_val_2nd = compute_algorithm::sub_group_reduce_add(lmem[sub_group_id_1d * sub_group_width +
																								 sub_group_local_id]);
						local_barrier();
						if(sub_group_local_id == 0) {
							lmem[sub_group_id_1d] = sg_red_val_2nd;
						}
						local_barrier();
						
						// final reduction
						if(sub_group_id_1d == 0) {
							const auto final_red_val = compute_algorithm::sub_group_reduce_add(sub_group_local_id < tile_count_2nd ?
																							   lmem[sub_group_local_id] : data_type(0));
							if(sub_group_local_id == 0) {
								atomic_add(&sum[0], final_red_val);
							}
						}
					}
				}
			}
#endif
		}
		else {
			// fallback to local memory reduction
			
			// local reduction to work-item #0
			local_buffer<data_type, compute_algorithm::reduce_local_memory_elements<tile_size>()> lmem;
			const auto red_sum = compute_algorithm::reduce_add<tile_size>(item_sum, lmem);
			
			// only work-item #0 knows the reduced sum and will thus write to the total sum
			if(local_id.x == 0) {
				atomic_add(&sum[0], red_sum);
			}
		}
	}
}

// float/uint 32-bit and 64-bit reduction add kernels
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_0)
#define REDUCTION_KERNELS(tile_size) \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_f32_##tile_size(buffer<const float> data, buffer<float> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_u32_##tile_size(buffer<const uint32_t> data, buffer<uint32_t> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_f64_##tile_size() { \
	/* nop */ \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_u64_##tile_size() { \
	/* nop */ \
}

#elif defined(FLOOR_NO_DOUBLE)
#define REDUCTION_KERNELS(tile_size) \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_f32_##tile_size(buffer<const float> data, buffer<float> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_u32_##tile_size(buffer<const uint32_t> data, buffer<uint32_t> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_f64_##tile_size() { \
	/* nop */ \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_u64_##tile_size(buffer<const uint64_t> data, buffer<uint64_t> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
}

#else
#define REDUCTION_KERNELS(tile_size) \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_f32_##tile_size(buffer<const float> data, buffer<float> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_u32_##tile_size(buffer<const uint32_t> data, buffer<uint32_t> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_f64_##tile_size(buffer<const double> data, buffer<double> sum, param<uint32_t> count) { \
reduce_add<tile_size>(data, sum, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void reduce_add_u64_##tile_size(buffer<const uint64_t> data, buffer<uint64_t> sum, param<uint32_t> count) { \
	reduce_add<tile_size>(data, sum, count); \
}
#endif

// instantiate kernels
POT_TILE_SIZES(REDUCTION_KERNELS)


///////////////////////////////////////////////////////////////////////////////
/// inclusive/exclusive scan:
///  * first pass: perform scan in each work-group
///  * second pass: adjust values of each group based on the last value of the previous group
///
/// in : [4 1 5 3 4 2 7 9 1 2 3 4]
///
/// inclusive:
/// 1st: [4 5 10 13] [4 6 13 22] [1 3 6 10] (group size 4)
/// 2nd: [4 5 10 13 17 19 26 35 36 38 41 45]
///
/// exclusive:
/// 1st: [0 4 5 10 13] [4 6 13 22] [1 3 6] (group size 4)
/// 2nd: [0 4 5 10 13 17 19 26 35 36 38 41]
///
/// NOTE: this could be done more efficiently, but the point of this is to test the inclusive_scan/exclusive_scan themselves

template <uint32_t tile_size, bool is_inclusive>
floor_inline_always void scan_local(buffer<const uint32_t>& in, buffer<uint32_t>& out, const uint32_t count) {
	local_buffer<uint32_t, compute_algorithm::scan_local_memory_elements<tile_size>()> lmem;
	
	auto value = (global_id.x < count ? in[global_id.x] : 0u);
	if constexpr (is_inclusive) {
		value = compute_algorithm::inclusive_scan_add<tile_size>(value, lmem);
		
		// can directly write to output
		if (global_id.x < count) {
			out[global_id.x] = value;
		}
	} else {
		const auto scan_value = compute_algorithm::exclusive_scan_add<tile_size>(value, lmem);
		
		// first group: write full range
		// second+ group: never write item #0, but write item #N into the next group block
		if (group_id.x == 0 || (local_id.x > 0 && global_id.x < count)) {
			out[global_id.x] = scan_value;
		}
		if (local_id.x == tile_size - 1u && (global_id.x + 1u) < count) {
			out[global_id.x + 1u] = scan_value + value;
		}
	}
}

template <uint32_t tile_size, bool is_inclusive>
floor_inline_always void scan_global(buffer<const uint32_t>& in, buffer<uint32_t>& out, const uint32_t count) {
	if (group_id.x == 0) {
		out[global_id.x] = in[global_id.x];
		if constexpr (!is_inclusive) {
			if (local_id.x == tile_size - 1u) {
				out[global_id.x + 1u] = in[global_id.x + 1u];
			}
		}
		return;
	}
	
	// need to offset each value in each group (past group #0) with:
	//  * inclusive: the sum of the last values in each previous group block
	//  * exclusive: the sum of the first values in each previous group block (excluding group #0)
	const auto per_item_count = max((group_id.x + (tile_size - 1u)) / tile_size, 1u);
	const auto offset = per_item_count * tile_size * local_id.x;
	uint32_t idx = offset;
	if constexpr (is_inclusive) {
		idx += tile_size - 1u; // -> last value in each group
	} else {
		idx += tile_size; // -> first value in each group (past group #0)
	}
	
	auto item_sum = 0u;
	const auto first_idx_in_group = min(tile_size * group_id.x + (is_inclusive ? 0u : 1u), count);
	for (uint32_t i = 0; i < per_item_count; ++i, idx += tile_size) {
		if (idx < first_idx_in_group) {
			item_sum += in[idx];
		}
	}

	local_buffer<uint32_t, 1u> scan_value_offset;
	local_buffer<uint32_t, compute_algorithm::reduce_local_memory_elements<tile_size>()> lmem;
	const auto red_sum = compute_algorithm::reduce_add<tile_size>(item_sum, lmem);
	if (local_id.x == 0u) {
		scan_value_offset[0] = red_sum;
	}
	local_barrier();
	
	if constexpr (is_inclusive) {
		out[global_id.x] = in[global_id.x] + scan_value_offset[0];
	} else {
		if ((global_id.x + 1u) < count) {
			out[global_id.x + 1u] = in[global_id.x + 1u] + scan_value_offset[0];
		}
	}
}

#define SCAN_KERNELS(tile_size) \
kernel kernel_local_size(tile_size, 1, 1) void incl_scan_local_u32_##tile_size(buffer<const uint32_t> in, buffer<uint32_t> out, param<uint32_t> count) { \
	scan_local<tile_size, true>(in, out, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void excl_scan_local_u32_##tile_size(buffer<const uint32_t> in, buffer<uint32_t> out, param<uint32_t> count) { \
	scan_local<tile_size, false>(in, out, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void incl_scan_global_u32_##tile_size(buffer<const uint32_t> in, buffer<uint32_t> out, param<uint32_t> count) { \
	scan_global<tile_size, true>(in, out, count); \
} \
kernel kernel_local_size(tile_size, 1, 1) void excl_scan_global_u32_##tile_size(buffer<const uint32_t> in, buffer<uint32_t> out, param<uint32_t> count) { \
	scan_global<tile_size, false>(in, out, count); \
}

// instantiate kernels
POT_TILE_SIZES(SCAN_KERNELS)

#endif
