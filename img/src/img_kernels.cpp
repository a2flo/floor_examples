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

#include "img_kernels.hpp"

#if defined(FLOOR_COMPUTE)

// defined during compilation:
// TAP_COUNT: kernel width, -> N*N filter (effective N computed below)

// sample pattern must be: <even number> <middle> <even number>
static_assert(TAP_COUNT % 2 == 1, "tap count must be an odd number!");
// a tap count of 21 results in an effective tap count of 63 (31px radius), due to the fact that 8-bit values
// multiplied with the resp. binomial coefficient have no contribution (or almost none) to the final image
// 3 -> 3, 5 -> 5, 7 -> 7, 9 -> 11, 11 -> 15, 13 -> 21, 15 -> 29, 17 -> 39, 19 -> 51, 21 -> 63
// also: math at that range gets very wonky, so 21 is a good cutoff choice
static_assert(TAP_COUNT >= 3u && TAP_COUNT <= 21, "tap count must be a value between 3 and 21!");

template <uint32_t tap_count>
static constexpr uint32_t find_effective_n() {
	// minimal contribution a fully white pixel must have to affect the blur result
	// (ignoring the fact that 0.5 gets rounded up to 1 and that multiple outer pixels combined can produce values > 1)
	constexpr const auto min_contribution = 1.0L / 255.0L;
	
	// start at the wanted tap count and go up by 2 "taps" if the row is unusable (and no point going beyond 64)
	for(uint32_t count = tap_count; count < 64u; count += 2) {
		// / 2^N for this row
		const long double sum_div = 1.0L / (long double)const_math::pow(2ull, int(count - 1));
		for(uint32_t i = 0u; i <= count; ++i) {
			const auto coeff = const_math::binomial(count - 1u, i);
			// is the coefficient large enough to produce a visible result?
			if((sum_div * (long double)coeff) > min_contribution) {
				// if so, check how many usable values this row has now (should be >= desired tap count)
				if((count - i * 2) < tap_count) {
					break;
				}
				return count;
			}
		}
	}
	return 0;
}

// computes the blur coefficients for the specified tap count (at compile-time)
template <uint32_t tap_count>
static constexpr auto compute_coefficients() {
	const_array<float, tap_count> ret {};
	
	// compute binomial coefficients and divide them by 2^(effective tap count - 1)
	// this is basically computing a row in pascal's triangle, using all values (or the middle part) as coefficients
	const auto effective_n = find_effective_n<tap_count>();
	const long double sum_div = 1.0L / (long double)const_math::pow(2ull, int(effective_n - 1));
	for(uint32_t i = 0u, k = (effective_n - tap_count) / 2u; i < tap_count; ++i, ++k) {
		// coefficient_i = (n choose k) / 2^n
		ret[i] = float(sum_div * (long double)const_math::binomial(effective_n - 1, k));
	}
	
	return ret;
}

template <uint32_t tile_size, uint32_t lateral_dim, typename storage_type>
static void image_blur_single_stage(const_image_2d<storage_type> in_img, image_2d<vector_n<storage_type, 4>, true> out_img) {
	static_assert(tile_size == lateral_dim * lateral_dim);
	static_assert(lateral_dim <= 32u);
	
	// map 1D to 2D
	// NOTE: we could also run this as a 2D kernel and avoid this mapping, but this gives us more control
	const uint2 lid {
		local_id.x % lateral_dim,
		local_id.x / lateral_dim,
	};
	
	// awesome constexpr magic
	static constexpr const auto coeffs = compute_coefficients<TAP_COUNT>();
	// aka half kernel width or radius, but also the part that "protrudes" out of the inner tile
	static constexpr const int overlap = TAP_COUNT / 2;
	
	// this uses local memory as a sample + compute cache
	// note that using a float4/half4 instead of a uchar4 requires more storage, but computations using floating point
	// values are _a_lot_ faster than integer math or doing int->float conversions + float math
	static constexpr const auto sample_count_x = lateral_dim + 2u * uint32_t(overlap);
	static constexpr const auto sample_count_y = sample_count_x;
	static constexpr const auto sample_count = sample_count_x * sample_count_y;
	local_buffer<vector_n<storage_type, 4>, sample_count> samples;
	
	// get image dim and compute offsets
	const auto img_dim = in_img.dim().xy;
	// NOTE: we have ensured that the image size is a multiple of 32px and we know that lateral_dim is <= 32 here -> can just divide
	const auto tile_count_x = img_dim.x / lateral_dim;
	const uint2 sample_offset_inner {
		(group_id.x % tile_count_x) * lateral_dim,
		(group_id.x / tile_count_x) * lateral_dim,
	};
	const auto sample_offset_outer = sample_offset_inner.cast<int>() - overlap;
	
	// read the input pixels and store them in the local buffer/"cache"
	// -> since we have more samples than work-items, we loop over the sample count and let the work-items
	//    read data multiple times to keep everything occupied as best as possible
	for (uint32_t sample_idx = local_id.x; sample_idx < sample_count; sample_idx += local_size.x) {
		// compute the image coordinate for this sample index
		// NOTE: out-of-bounds accesses are clamped-to-edge (only need min() here, because sample_offset_outer is already >= 0)
		const int2 img_coord {
			math::clamp(int(sample_idx % sample_count_x) + sample_offset_outer.x, 0, int(img_dim.x) - 1),
			math::clamp(int(sample_idx / sample_count_x) + sample_offset_outer.y, 0, int(img_dim.y) - 1)
		};
		samples[sample_idx] = in_img.read(img_coord);
	}
	// make sure the complete tile has been read and stored
	local_barrier();
	
	// the blur is now computed using a separable filter, i.e. one vertical pass and one horizontal pass.
	// the vertical pass is done before the horizontal pass to increase occupancy:
	//  -> if the horizontal pass would be computed first, the middle part of a warp/sub-group/SIMD-unit
	//     would do the blur computation, but the outer parts would simply idle
	//  -> if the vertical pass is done first, all horizontal lines either have to be computed completely
	//     or don't have to be computed at all
	//
	//      horizontal first
	//       (active items)
	//  ------------------------
	//  |       |xxxxxx|       | // all horizontal lines have active and inactive work-items
	//  |       |xxxxxx|       |
	//  |       |xxxxxx|       |
	//  |       |xxxxxx|       |
	//  |       |-x-x-x|       |
	//  |       |xxxxxx|       |
	//  |       |xxxxxx|       |
	//  |       |-x-x-x|       |
	//  |       |xxxxxx|       |
	//  |       |xxxxxx|       |
	//  |       |xxxxxx|       |
	//  |       |xxxxxx|       |
	//  ------------------------
	//
	//       vertical first
	//       (active items)
	//  ------------------------
	//  |                      | // can skip "empty" horizontal lines w/o any work-items
	//  |                      |
	//  |                      |
	//  |                      |
	//  |xxxxxxx|x-x-x-|xxxxxxx|
	//  |xxxxxxx|xxxxxx|xxxxxxx|
	//  |xxxxxxx|xxxxxx|xxxxxxx|
	//  |xxxxxxx|x-x-x-|xxxxxxx|
	//  |                      |
	//  |                      |
	//  |                      |
	//  |                      |
	//  ------------------------
	//
	// NOTE: of course, this is simply a matter of mapping ids to execution order, so if "lid" was reversed,
	//       the other way around would be more efficient. also, there is no guarantee that ids/work-items
	//       are properly mapped to warps/sub-groups/SIMD-units (0-31 warp #0, 32-63 warp #1, ...), but in
	//       practice, this is usually the case.
	
	// the actual in/out pixel block + overlap
	static constexpr const auto vertical_pass_sample_count = sample_count_x * lateral_dim;
	// offset to first item we need to handle
	static constexpr const auto vertical_pass_sample_offset = sample_count_x * overlap;
	// the results of the vertical pass are written into a separate block of local memory,
	// since this prevents additional synchronization and lowers registers usage
	// (we would need to store 2 or 3 color values for each item on the "stack", i.e. registers)
	local_buffer<vector_n<storage_type, 4>, vertical_pass_sample_count> vertical_pass_samples;
	for (uint32_t idx = local_id.x; idx < vertical_pass_sample_count; idx += local_size.x) {
		float4 v_color;
		auto sample_idx = (vertical_pass_sample_offset + idx) - (overlap * sample_count_x /* Y stride */);
#pragma clang loop unroll_count(TAP_COUNT)
		for (int i = -overlap; i <= overlap; ++i, sample_idx += sample_count_x) {
			// note that this will be optimized to an fma instruction if possible
			v_color += coeffs[size_t(overlap + i)] * samples[sample_idx].template cast<float>();
		}
		vertical_pass_samples[idx] = v_color;
	}
	
	// make sure all write accesses have completed in the loop
	local_barrier();
	
	// horizontal blur: we now only need to run this with the actual in/out block
	//  ------------------------
	//  |                      |
	//  |                      |
	//  |                      |
	//  |                      |
	//  |       |x-x-x-|       |
	//  |       |x-x-x-|       |
	//  |       |x-x-x-|       |
	//  |       |x-x-x-|       |
	//  |                      |
	//  |                      |
	//  |                      |
	//  |                      |
	//  ------------------------
	
	float4 h_color;
	auto sample_idx = lid.x + lid.y * sample_count_x;
#pragma clang loop unroll_count(TAP_COUNT)
	for (uint32_t i = 0; i < TAP_COUNT; ++i, ++sample_idx) {
		h_color += coeffs[i] * vertical_pass_samples[sample_idx].template cast<float>();
	}
	
	// write out
	const uint2 img_coord {
		lid.x + sample_offset_inner.x,
		lid.y + sample_offset_inner.y
	};
	out_img.write(img_coord, h_color);
}

kernel_1d(1024) void image_blur_single_stage_32x32_f32(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	image_blur_single_stage<1024, 32, float>(in_img, out_img);
}
kernel_1d(256) void image_blur_single_stage_16x16_f32(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	image_blur_single_stage<256, 16, float>(in_img, out_img);
}
kernel_1d(64) void image_blur_single_stage_8x8_f32(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	image_blur_single_stage<64, 8, float>(in_img, out_img);
}

kernel_1d(1024) void image_blur_single_stage_32x32_f16(const_image_2d<half> in_img, image_2d<half4, true> out_img) {
	image_blur_single_stage<1024, 32, half>(in_img, out_img);
}
kernel_1d(256) void image_blur_single_stage_16x16_f16(const_image_2d<half> in_img, image_2d<half4, true> out_img) {
	image_blur_single_stage<256, 16, half>(in_img, out_img);
}
kernel_1d(64) void image_blur_single_stage_8x8_f16(const_image_2d<half> in_img, image_2d<half4, true> out_img) {
	image_blur_single_stage<64, 8, half>(in_img, out_img);
}

// this is the dumb version of the blur, processing a horizontal or vertical line w/o manual caching
// NOTE: this is practically the same as the opengl/glsl shader
template <uint32_t direction /* 0 == horizontal, 1 == vertical */, typename storage_type>
floor_inline_always static void image_blur_dumb(const_image_2d<storage_type> in_img, image_2d<vector_n<storage_type, 4>, true> out_img) {
	const int2 img_coord { global_id.xy };
	
	constexpr const auto coeffs = compute_coefficients<TAP_COUNT>();
	constexpr const int overlap = TAP_COUNT / 2;
	
	float4 color;
#pragma clang loop unroll_count(TAP_COUNT)
	for (int i = -overlap; i <= overlap; ++i) {
		color += (coeffs[size_t(overlap + i)] *
#if TAP_COUNT <= 15 // can use texel offset here, TAP_COUNT == 15 has an offset range of [-7, 7]
				  in_img.read(img_coord, int2 { direction == 0 ? i : 0, direction == 0 ? 0 : i }).template cast<float>()
#else // else: need to resort to integer math
				  in_img.read(img_coord + int2 { direction == 0 ? i : 0, direction == 0 ? 0 : i }).template cast<float>()
#endif
				  );
	}
	
	out_img.write(img_coord, color);
}

kernel_2d() void image_blur_dumb_horizontal_f32(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	image_blur_dumb<0, float>(in_img, out_img);
}
kernel_2d() void image_blur_dumb_horizontal_f16(const_image_2d<half> in_img, image_2d<half4, true> out_img) {
	image_blur_dumb<0, half>(in_img, out_img);
}

kernel_2d() void image_blur_dumb_vertical_f32(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	image_blur_dumb<1, float>(in_img, out_img);
}
kernel_2d() void image_blur_dumb_vertical_f16(const_image_2d<half> in_img, image_2d<half4, true> out_img) {
	image_blur_dumb<1, half>(in_img, out_img);
}

#endif
