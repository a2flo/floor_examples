/*
 *  Copyright (C) 2024 Thomas Smith (via https://github.com/b0nes164/GPUSorting)
 *  Copyright (C) 2025 Florian Ziesche (libfloor adaptation + improvements)
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include "radix_sort.hpp"
using namespace fl;

#if defined(FLOOR_DEVICE) && !defined(FLOOR_DEVICE_OPENCL)

#if defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
FLOOR_IGNORE_WARNING(pass-failed) // ignore unroll warnings in non-device Host-Compute
#endif

// memory zero'ing in an indirect pipeline is faster than a manual zero call on the host side (especially for small sizes)
kernel_1d_simd(radix_sort::lane_count, radix_sort::upsweep_dim) void indirect_radix_zero(buffer<uint32_t> global_histogram) {
	global_histogram[global_id.x] = 0u;
}

// first pass of the upsweep computes the overall global histogram (used in all 4 passes) and the first pass histogram
kernel_1d_simd(radix_sort::lane_count, radix_sort::upsweep_dim) void indirect_radix_upsweep_init(buffer<const uint32_t> src,
																								 buffer<uint32_t> global_histogram,
																								 buffer<uint32_t> pass_histogram,
																								 buffer<const radix_sort::indirect_params_t> params) {
	const auto count = params->count;
	const auto group_count = params->group_count;
	
	static constexpr const uint32_t lmem_elem_count {
		std::max(algorithm::scan_local_memory_elements<radix_sort::upsweep_dim, uint4, algorithm::group::OP::ADD>(), radix_sort::radix)
	};
	local_buffer<uint4, lmem_elem_count> bins;
	
	// TODO: automatic local memory zero init
	for (uint32_t i = local_id.x; i < lmem_elem_count; i += radix_sort::upsweep_dim) {
		bins[i] = {};
	}
	local_barrier();
	
	for (uint32_t block_idx = group_id.x * radix_sort::keys_per_thread,
		 block_end = math::min(block_idx + radix_sort::keys_per_thread, group_count * radix_sort::keys_per_thread);
		 block_idx < block_end; ++block_idx) {
		uint32_t item_index = block_idx * radix_sort::upsweep_dim + local_id.x;
		if (item_index < count) {
			auto item = src[item_index];
			atomic_inc(&bins[item & radix_sort::radix_mask].x);
			atomic_inc(&bins[(item >> radix_sort::radix_log) & radix_sort::radix_mask].y);
			atomic_inc(&bins[(item >> (radix_sort::radix_log * 2u)) & radix_sort::radix_mask].z);
			atomic_inc(&bins[(item >> (radix_sort::radix_log * 3u)) & radix_sort::radix_mask].w);
		}
	}
	local_barrier();
	
	const uint4 scan_value = bins[local_id.x];
	// NOTE: only want the result of the first binning here
	pass_histogram[group_id.x + local_id.x * group_count] = scan_value.x;
	local_barrier();
	
	const auto scan_result = algorithm::exclusive_scan_add<radix_sort::upsweep_dim>(scan_value, bins);
	atomic_add(&global_histogram[local_id.x], scan_result.x);
	atomic_add(&global_histogram[radix_sort::radix + local_id.x], scan_result.y);
	atomic_add(&global_histogram[radix_sort::radix * 2u + local_id.x], scan_result.z);
	atomic_add(&global_histogram[radix_sort::radix * 3u + local_id.x], scan_result.w);
}

// 2nd+ upsweep pass only computes the pass histogram
kernel_1d_simd(radix_sort::lane_count, radix_sort::upsweep_dim) void indirect_radix_upsweep_pass_only(buffer<const uint32_t> src,
																									  buffer<uint32_t> pass_histogram,
																									  buffer<const radix_sort::indirect_params_t> params,
																									  buffer<const radix_sort::indirect_radix_shift_t> radix_shift_params) {
	const auto count = params->count;
	const auto group_count = params->group_count;
	const auto radix_shift = radix_shift_params->radix_shift;
	
	static constexpr const uint32_t lmem_elem_count {
		std::max(algorithm::scan_local_memory_elements<radix_sort::upsweep_dim, uint32_t, algorithm::group::OP::ADD>(), radix_sort::radix)
	};
	local_buffer<uint32_t, lmem_elem_count> bins;
	
	// TODO: automatic local memory zero init
	for (uint32_t i = local_id.x; i < lmem_elem_count; i += radix_sort::upsweep_dim) {
		bins[i] = {};
	}
	local_barrier();
	
	for (uint32_t block_idx = group_id.x * radix_sort::keys_per_thread,
		 block_end = math::min(block_idx + radix_sort::keys_per_thread, group_count * radix_sort::keys_per_thread);
		 block_idx < block_end; ++block_idx) {
		uint32_t item_index = block_idx * radix_sort::upsweep_dim + local_id.x;
		if (item_index < count) {
			auto item = src[item_index];
			atomic_inc(&bins[(item >> radix_shift) & radix_sort::radix_mask]);
		}
	}
	local_barrier();
	
	const auto scan_value = bins[local_id.x];
	pass_histogram[group_id.x + local_id.x * group_count] = scan_value;
}

// combined upsweep pass computing the pass histogram and global histogram necessary for the current pass
// NOTE: only used when local memory atomics are not wanted or otherwise broken
kernel_1d_simd(radix_sort::lane_count, radix_sort::upsweep_dim) void indirect_radix_upsweep(buffer<const uint32_t> src,
																							buffer<uint32_t> pass_histogram,
																							buffer<uint32_t> global_histogram,
																							buffer<const radix_sort::indirect_params_t> params,
																							buffer<const radix_sort::indirect_radix_shift_t> radix_shift_params) {
	const auto count = params->count;
	const auto group_count = params->group_count;
	const auto radix_shift = radix_shift_params->radix_shift;
	
	static_assert(radix_sort::upsweep_dim == radix_sort::radix);
	static_assert(radix_sort::keys_per_thread <= 15u); // 4 bits per value
	static constexpr const uint32_t bin_bitness { 4u };
	static constexpr const uint32_t bins_per_value { (sizeof(uint32_t) * 8u) / bin_bitness }; // 8 bins
	static constexpr const uint32_t bin_value_mask { (1u << bin_bitness) - 1u };
	static constexpr const uint32_t bins_per_item { radix_sort::radix / bins_per_value }; // 8 bins per uint32_t * 32 for 256 in total
	static constexpr const uint32_t lmem_elem_count { bins_per_item * radix_sort::upsweep_dim };
	static_assert(lmem_elem_count == 8192u);
	local_buffer<uint32_t, lmem_elem_count> bins;
#pragma unroll
	for (uint32_t i = 0u; i < bins_per_item; ++i) {
		bins[local_id.x + i * radix_sort::upsweep_dim] = 0u;
	}
	
	for (uint32_t block_idx = group_id.x * radix_sort::keys_per_thread,
		 block_end = math::min(block_idx + radix_sort::keys_per_thread, group_count * radix_sort::keys_per_thread);
		 block_idx < block_end; ++block_idx) {
		const auto item_index = block_idx * radix_sort::upsweep_dim + local_id.x;
		if (item_index < count) {
			const auto item = src[item_index];
			const auto digit = (item >> radix_shift) & radix_sort::radix_mask;
			const auto item_bin = digit / bins_per_value;
			const auto item_intra_bin = digit % bins_per_value;
			const auto item_intra_bin_one = 1u << (4u * item_intra_bin);
			bins[local_id.x + item_bin * radix_sort::upsweep_dim] += item_intra_bin_one;
		}
	}
	local_barrier();
	
	uint32_t digit_total = 0u;
	{
		const auto digit = local_id.x;
		const auto item_bin = digit / bins_per_value;
		const auto item_intra_bin = digit % bins_per_value;
		const auto item_intra_bin_shift = (4u * item_intra_bin);
		for (uint32_t i = 0u; i < radix_sort::upsweep_dim; ++i) {
			digit_total += (bins[i + item_bin * radix_sort::upsweep_dim] >> item_intra_bin_shift) & bin_value_mask;
		}
		pass_histogram[group_id.x + digit * group_count] = digit_total;
	}
	local_barrier();
	
	const auto scan_result = algorithm::exclusive_scan_add<radix_sort::upsweep_dim>(digit_total, bins);
	atomic_add(&global_histogram[radix_shift * (radix_sort::radix / 8u) + local_id.x], scan_result);
}

// this handles up to 256 groups
kernel_1d_simd(radix_sort::lane_count, radix_sort::scan_dim) void indirect_radix_scan_small(buffer<uint32_t> pass_histogram,
																							buffer<const radix_sort::indirect_params_t> params) {
	local_buffer<uint32_t, algorithm::scan_local_memory_elements<radix_sort::scan_dim, uint32_t, algorithm::group::OP::ADD>()> scan_mem;
	const auto group_count = params->group_count;
	const auto input_value = (local_id.x < group_count ? pass_histogram[local_id.x + group_id.x * group_count] : 0u);
	const auto scan_result = algorithm::exclusive_scan_add<radix_sort::scan_dim>(input_value, scan_mem);
	if (local_id.x < group_count) {
		pass_histogram[local_id.x + group_id.x * group_count] = scan_result;
	}
}

// for more than 256 groups
kernel_1d_simd(radix_sort::lane_count, radix_sort::scan_dim) void indirect_radix_scan(buffer<uint32_t> pass_histogram,
																					  buffer<const radix_sort::indirect_params_t> params) {
	local_buffer<uint32_t, algorithm::scan_local_memory_elements<radix_sort::scan_dim, uint32_t, algorithm::group::OP::ADD>()> scan_mem;
	local_buffer<uint32_t, 1u> prev_iteration_sum_lmem;
	const auto group_count = params->group_count;
	const auto group_iterations = (group_count + radix_sort::scan_dim - 1u) / radix_sort::scan_dim;
	const auto radix_idx = group_id.x;
	uint32_t prev_iteration_sum = 0u;
	uint32_t i = 0;
	uint32_t group_idx = local_id.x;
	for (;;) {
		const auto input_value = (group_idx < group_count ? pass_histogram[group_idx + radix_idx * group_count] : 0u);
		const auto scan_result = algorithm::inclusive_scan_add<radix_sort::scan_dim>(input_value + prev_iteration_sum, scan_mem);
		if (group_idx < group_count) {
			// -> to exclusive sum result
			pass_histogram[group_idx + radix_idx * group_count] = scan_result - input_value;
		}
		
		// iter end here, so that we can skip the final barrier + exchange
		++i;
		group_idx += radix_sort::scan_dim;
		if (i == group_iterations) {
			break;
		}
		
		if (local_id.x == radix_sort::scan_dim - 1u) {
			prev_iteration_sum_lmem[0] = scan_result;
		}
		
		local_barrier();
		prev_iteration_sum = (local_id.x == 0 ? prev_iteration_sum_lmem[0] : 0);
	}
}

template <bool is_key_only, typename value_type = uint32_t>
requires (sizeof(value_type) <= sizeof(uint32_t))
static inline void radix_downsweep(buffer<const uint32_t>& src,
								   buffer<uint32_t>& dst,
								   std::conditional_t<is_key_only, int, buffer<const value_type>&> src_values,
								   std::conditional_t<is_key_only, int, buffer<value_type>&> dst_values,
								   buffer<const uint32_t>& global_histogram,
								   buffer<const uint32_t>& pass_histogram,
								   const uint32_t count,
								   const uint32_t radix_shift) {
	static constexpr const uint32_t histogram_size { radix_sort::downsweep_warps * radix_sort::radix }; // 8 * 256 == 2048
	local_buffer<uint32_t, radix_sort::partition_size> warp_histograms;
	local_buffer<uint32_t, radix_sort::radix> local_histogram;
	
	// clear local histogram memory
	for (uint32_t i = local_id.x; i < histogram_size; i += radix_sort::downsweep_dim) {
		warp_histograms[i] = 0;
	}
	local_barrier();
	
	// load keys
	uint32_t keys[radix_sort::keys_per_thread];
	const auto warp_offset = sub_group_id * radix_sort::lane_count * radix_sort::keys_per_thread;
	const auto dev_offset = group_id.x /* partition index */ * radix_sort::partition_size;
#pragma unroll
	for (uint32_t i = sub_group_local_id + warp_offset + dev_offset, k = 0; k < radix_sort::keys_per_thread; i += radix_sort::lane_count, ++k) {
		keys[k] = (i < count ? src[i] : 0xFFFF'FFFFu);
	}
	
	// warp level multi-split
	uint16_t offsets[radix_sort::keys_per_thread];
	const auto lane_mask_lt = (1u << sub_group_local_id) - 1u;
	const auto warp_hist_offset = sub_group_id * radix_sort::radix;
#pragma unroll
	for (uint32_t k = 0; k < radix_sort::keys_per_thread; ++k) {
		uint32_t eq_mask = 0xFFFF'FFFFu;
#pragma unroll
		for (uint32_t bit = 0; bit < radix_sort::radix_log; ++bit) {
			const auto current_bit = 1u << (bit + radix_shift);
			const auto pred = ((keys[k] & current_bit) != 0u);
			const auto bal = simd_ballot(pred);
			eq_mask &= (pred ? bal : ~bal);
		}
		offsets[k] = uint16_t(math::popcount(eq_mask & lane_mask_lt));
		const auto digit = ((keys[k] >> radix_shift) & radix_sort::radix_mask);
		const auto pre_increment_val = warp_histograms[warp_hist_offset + digit];
		simd_barrier(); // NOTE: never replace this with any other kind of barrier, otherwise this will get stuck!
		if (!offsets[k]) {
			warp_histograms[warp_hist_offset + digit] += uint32_t(math::popcount(eq_mask));
		}
		simd_barrier();
		offsets[k] += pre_increment_val;
	}
	local_barrier();
	
	// exclusive prefix sum up the warp histograms
	auto reduction = warp_histograms[local_id.x];
#pragma unroll
	for (uint32_t i = local_id.x + radix_sort::radix, k = 0; k < radix_sort::downsweep_warps - 1; i += radix_sort::radix, ++k) {
		reduction += warp_histograms[i];
		warp_histograms[i] = reduction - warp_histograms[i];
	}
	// take advantage of barrier to begin exclusive prefix sum across the reductions
	local_histogram[local_id.x] = simd_shuffle(algorithm::group::sub_group_inclusive_scan<algorithm::group::OP::ADD>(reduction),
											   (sub_group_local_id + radix_sort::lane_mask) & radix_sort::lane_mask);
	local_barrier();
	
	// warp histogram reduction + exclusive-scan
	if (sub_group_id == 0) {
		const auto index = sub_group_local_id * radix_sort::lane_count;
		const auto pred = (index < radix_sort::radix);
		const auto t = algorithm::group::sub_group_exclusive_scan<algorithm::group::OP::ADD>(pred ? local_histogram[index] : 0);
		if (pred) {
			local_histogram[index] = t;
		}
	}
	local_barrier();
	
	const auto sum_add_val = (sub_group_local_id > 0 ? local_histogram[local_id.x - sub_group_local_id] : 0u);
	local_barrier();
	if (sub_group_local_id > 0) {
		local_histogram[local_id.x] += sum_add_val;
	}
	local_barrier();
	
	// update offsets
#pragma unroll
	for (uint32_t k = 0; k < radix_sort::keys_per_thread; ++k) {
		const auto t2 = (keys[k] >> radix_shift) & radix_sort::radix_mask;
		const auto warp_hist_add = (sub_group_id > 0 ? warp_histograms[warp_hist_offset + t2] : 0u);
		offsets[k] += warp_hist_add + local_histogram[t2];
	}
	local_barrier();
	
	// scatter keys
	// NOTE: this overwrites the histograms with all keys in this partition
#pragma unroll
	for (uint32_t k = 0; k < radix_sort::keys_per_thread; ++k) {
		warp_histograms[offsets[k]] = keys[k];
	}
	
	// compute global output indices
	local_histogram[local_id.x] = ((global_histogram[local_id.x + radix_shift * radix_sort::lane_count] +
									pass_histogram[group_id.x + local_id.x * group_size.x]) -
								   local_histogram[local_id.x]);
	local_barrier();
	
	// scatter runs of keys and/or values / write output
	const auto final_partition_size = count - group_id.x * radix_sort::partition_size;
	if constexpr (is_key_only) { // -> key only
		if (group_id.x < group_size.x - 1) {
#pragma unroll
			for (uint32_t i = local_id.x, k = 0; k < radix_sort::keys_per_thread; i += radix_sort::downsweep_dim, ++k) {
				dst[local_histogram[(warp_histograms[i] >> radix_shift) & radix_sort::radix_mask] + i] = warp_histograms[i];
			}
		} else {
			for (uint32_t i = local_id.x; i < final_partition_size; i += radix_sort::downsweep_dim) {
				dst[local_histogram[(warp_histograms[i] >> radix_shift) & radix_sort::radix_mask] + i] = warp_histograms[i];
			}
		}
	} else { // -> key/value
		uint8_t digits[radix_sort::keys_per_thread];
		value_type values[radix_sort::keys_per_thread];
		
		if (group_id.x < group_size.x - 1) {
#pragma unroll
			for (uint32_t i = local_id.x, k = 0; k < radix_sort::keys_per_thread; i += radix_sort::downsweep_dim, ++k) {
				digits[k] = (warp_histograms[i] >> radix_shift) & radix_sort::radix_mask;
				dst[local_histogram[digits[k]] + i] = warp_histograms[i];
			}
			local_barrier();
			
#pragma unroll
			for (uint32_t i = sub_group_local_id + warp_offset + dev_offset, k = 0; k < radix_sort::keys_per_thread;
				 i += radix_sort::lane_count, ++k) {
				values[k] = src_values[i];
			}
			
			// scatter values
#pragma unroll
			for (uint32_t k = 0; k < radix_sort::keys_per_thread; ++k) {
				warp_histograms[offsets[k]] = values[k];
			}
			local_barrier();
			
#pragma unroll
			for (uint32_t i = local_id.x, k = 0; k < radix_sort::keys_per_thread; i += radix_sort::downsweep_dim, ++k) {
				dst_values[local_histogram[digits[k]] + i] = value_type(warp_histograms[i]);
			}
		} else {
#pragma unroll
			for (uint32_t i = local_id.x, k = 0; k < radix_sort::keys_per_thread; i += radix_sort::downsweep_dim, ++k) {
				if (i < final_partition_size) {
					digits[k] = (warp_histograms[i] >> radix_shift) & radix_sort::radix_mask;
					dst[local_histogram[digits[k]] + i] = warp_histograms[i];
				}
			}
			local_barrier();
			
#pragma unroll
			for (uint32_t i = sub_group_local_id + warp_offset, k = 0; k < radix_sort::keys_per_thread; i += radix_sort::lane_count, ++k) {
				if (i < final_partition_size) {
					values[k] = src_values[i + dev_offset];
				}
			}
			
			// scatter values
#pragma unroll
			for (uint32_t k = 0; k < radix_sort::keys_per_thread; ++k) {
				warp_histograms[offsets[k]] = values[k];
			}
			local_barrier();
			
#pragma unroll
			for (uint32_t i = local_id.x, k = 0; k < radix_sort::keys_per_thread; i += radix_sort::downsweep_dim, ++k) {
				if (i < final_partition_size) {
					dst_values[local_histogram[digits[k]] + i] = value_type(warp_histograms[i]);
				}
			}
		}
	}
}

kernel_1d_simd(radix_sort::lane_count, radix_sort::downsweep_dim)
void indirect_radix_downsweep_keys(buffer<const uint32_t> src,
								   buffer<uint32_t> dst,
								   buffer<const uint32_t> global_histogram,
								   buffer<const uint32_t> pass_histogram,
								   buffer<const radix_sort::indirect_params_t> params,
								   buffer<const radix_sort::indirect_radix_shift_t> radix_shift_params) {
	const auto count = params->count;
	const auto radix_shift = radix_shift_params->radix_shift;
	radix_downsweep<true>(src, dst, 0, 0, global_histogram, pass_histogram, count, radix_shift);
}

kernel_1d_simd(radix_sort::lane_count, radix_sort::downsweep_dim)
void indirect_radix_downsweep_kv16(buffer<const uint32_t> src,
								   buffer<uint32_t> dst,
								   buffer<const uint16_t> src_values,
								   buffer<uint16_t> dst_values,
								   buffer<const uint32_t> global_histogram,
								   buffer<const uint32_t> pass_histogram,
								   buffer<const radix_sort::indirect_params_t> params,
								   buffer<const radix_sort::indirect_radix_shift_t> radix_shift_params) {
	const auto count = params->count;
	const auto radix_shift = radix_shift_params->radix_shift;
	radix_downsweep<false, uint16_t>(src, dst, src_values, dst_values, global_histogram, pass_histogram, count, radix_shift);
}

kernel_1d_simd(radix_sort::lane_count, radix_sort::downsweep_dim)
void indirect_radix_downsweep_kv32(buffer<const uint32_t> src,
								   buffer<uint32_t> dst,
								   buffer<const uint32_t> src_values,
								   buffer<uint32_t> dst_values,
								   buffer<const uint32_t> global_histogram,
								   buffer<const uint32_t> pass_histogram,
								   buffer<const radix_sort::indirect_params_t> params,
								   buffer<const radix_sort::indirect_radix_shift_t> radix_shift_params) {
	const auto count = params->count;
	const auto radix_shift = radix_shift_params->radix_shift;
	radix_downsweep<false, uint32_t>(src, dst, src_values, dst_values, global_histogram, pass_histogram, count, radix_shift);
}

#endif
