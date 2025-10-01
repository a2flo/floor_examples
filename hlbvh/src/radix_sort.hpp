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

#pragma once

#include <floor/core/essentials.hpp>
#if defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/common.hpp>
#endif

namespace radix_sort {

//! assuming SIMD32 here
static constexpr const uint32_t lane_count { 32u };
static constexpr const uint32_t lane_mask { lane_count - 1u };
//! 8-bit radix
static constexpr const uint32_t radix_log { 8u };
static constexpr const uint32_t radix { 1u << radix_log };
static constexpr const uint32_t radix_mask { radix - 1u };
//! 4 passes, sorting 8-bit each
static constexpr const uint32_t sort_passes { 4u };
//! we use a local size of 256 for every kernel
static constexpr const uint32_t upsweep_dim { 256u };
static constexpr const uint32_t scan_dim { 256u };
static constexpr const uint32_t downsweep_warps { 8u };
static constexpr const uint32_t downsweep_dim { downsweep_warps * lane_count /* 256 */ };
//! number of keys each thread/work-item handles in the upsweep and downsweep passes
//! NOTE: with dim == 256, this results in exactly 16KiB of local memory usage in downsweep
//!       -> 2 active work-groups per CU/SM for 32KiB devices, 4 for 64KiB, ...
static constexpr const uint32_t keys_per_thread { 15u };
//! overall partition size (#keys) in the downsweep pass
static constexpr const uint32_t partition_size { keys_per_thread * downsweep_dim /* 3840 */ };

static_assert(upsweep_dim == radix);
static_assert(scan_dim == radix);
static_assert(downsweep_dim == radix);

struct indirect_params_t {
	uint32_t count;
	uint32_t group_count;
	uint32_t _unused_0;
	uint32_t _unused_1;
};
static_assert(sizeof(indirect_params_t) == 16u);

struct indirect_radix_shift_t {
	uint32_t radix_shift;
	uint32_t _unused_0;
	uint32_t _unused_1;
	uint32_t _unused_2;
};
static_assert(sizeof(indirect_radix_shift_t) == 16u);

} // radix_sort
