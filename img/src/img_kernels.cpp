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

#include "img_kernels.hpp"

#if defined(FLOOR_COMPUTE)

// defined during compilation:
// TAP_COUNT: kernel width, -> N*N filter (effective N computed below)
// TILE_SIZE: (local) work-group size, this is INNER_TILE_SIZE + (TAPCOUNT / 2) * 2, thus includes the overlap
// INNER_TILE_SIZE: the image portion that will actually be computed + output in here
// SECOND_CACHE: flag that determines if a second local/shared memory "cache" is used, so that one barrier can be skipped

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
		const long double sum_div = 1.0L / (long double)(2ull << (count - 1));
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
	const long double sum_div = 1.0L / (long double)(2ull << (effective_n - 1));
	for(uint32_t i = 0u, k = (effective_n - tap_count) / 2u; i < tap_count; ++i, ++k) {
		// coefficient_i = (n choose k) / 2^n
		ret[i] = float(sum_div * (long double)const_math::binomial(effective_n - 1, k));
	}
	
	return ret;
}

kernel void image_blur_single_stage(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	const int2 gid { global_id.xy };
	const int2 lid { local_id.xy };
	
	const uint32_t lin_lid { local_id.y * TILE_SIZE + local_id.x };
	
	// awesome constexpr magic
	constexpr const auto coeffs = compute_coefficients<TAP_COUNT>();
	// aka half kernel width or radius, but also the part that "protrudes" out of the inner tile
	constexpr const int overlap = TAP_COUNT / 2;
	
	// this uses local memory as a sample + compute cache
	// note that using a float4 instead of a uchar4 requires more storage, but computation using float4s
	// is _a_lot_ faster than integer math or doing int->float conversions + float math
	constexpr const auto sample_count = TILE_SIZE * TILE_SIZE;
	local_buffer<float4, sample_count> samples;
	
	// map from the global work size to the actual image size
	const int2 img_coord = (gid / TILE_SIZE) * INNER_TILE_SIZE + (lid - overlap);
	// read the input pixel and store it in the local buffer/"cache" (note: out-of-bound access is clamped)
	samples[lin_lid] = in_img.read(img_coord);
	// make sure the complete tile has been read and stored
	local_barrier();
	
	// the blur is now computed using a separable filter, i.e. one vertical pass and one horizontal pass.
	// the vertical pass is done before the horizontal pass to increase occupancy
	//  -> if the horizontal pass would be computed first, the middle part of a warp/sub-group/SIMD-unit
	//     would do the blur computation, but the outer parts would simply idle
	//  -> if the vertical pass is done first, all horizontal lines either have to be computed completely
	//     or don't have to be computed at all
	//
	//      horizontal first
	//  ------------------------
	//  |       |xxxxxx|       | // all lines have active work-items
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
	//  ------------------------
	//  |                      | // can skip "empty" lines w/o work-items
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
	// NOTE: of course this is simply a matter of mapping ids to execution order, so if "lid" was reversed,
	//       the other way around would be more efficient. also, there is no guarantee that ids/work-items
	//       are properly mapped to warps/sub-groups/SIMD-units (0-31 warp #0, 32-63 warp #1, ...), but in
	//       practice, this is usually the case (true for nvidia gpus and intel cpus).
	
	// note for later: when using an additional sample cache, only one sync point is necessary after the first pass
#if defined(SECOND_CACHE)
	local_buffer<float4, TILE_SIZE * INNER_TILE_SIZE> samples_2;
#endif
	
	// vertical blur:
	// this must be done for the inner tile and the horizontal overlapping part (necessary for the horizontal blur)
	// also note that doing this branch is faster than clamping idx to [0, TILE_SIZE - 1]
	float4 v_color;
	if(lid.y >= overlap && lid.y < (TILE_SIZE - overlap)) {
		int idx = (lid.y - overlap) * TILE_SIZE + lid.x;
#pragma clang loop unroll_count(TAP_COUNT)
		for(int i = -overlap; i <= overlap; ++i, idx += TILE_SIZE) {
			// note that this will be optimized to an fma instruction if possible
			v_color += coeffs[size_t(overlap + i)] * samples[uint32_t(idx)];
		}
		
#if defined(SECOND_CACHE)
		// write results to the second cache
		samples_2[(lid.y - overlap) * TILE_SIZE + lid.x] = v_color;
		
		// this is always executed by all threads in a warp (on nvidia h/w) if the tile size is <= 32.
		// uncertain about other h/w, so don't do it (TODO: need a get_simd_width() function or SIMD_WIDTH macro)
#if defined(FLOOR_COMPUTE_CUDA) && TILE_SIZE <= 32
		local_barrier();
#endif
#endif
	}
	
#if defined(SECOND_CACHE) && !defined(FLOOR_COMPUTE_CUDA)
	// else case from above (aka "the safe route"):
	// barriers/fences must always be executed by all work-items in a work-group (technically warp/sub-group)
	local_barrier();
#endif
	
#if !defined(SECOND_CACHE)
	// make sure all read accesses have completed (in the loop)
	local_barrier();
	// write the sample back to the local cache
	if(lid.y >= overlap && lid.y < (TILE_SIZE - overlap)) {
		samples[lin_lid] = v_color;
	}
	// make sure everything has been updated
	local_barrier();
#endif
	
	// horizontal blur:
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
	if(lid.x >= overlap && lid.x < (TILE_SIZE - overlap) &&
	   lid.y >= overlap && lid.y < (TILE_SIZE - overlap)) {
		float4 h_color;
		
#if defined(SECOND_CACHE)
		int idx = (lid.y - overlap) * TILE_SIZE + lid.x - overlap;
#else
		int idx = lid.y * TILE_SIZE + lid.x - overlap;
#define samples_2 samples
#endif
		
#pragma clang loop unroll_count(TAP_COUNT)
		for(int i = -overlap; i <= overlap; ++i, ++idx) {
			h_color += coeffs[size_t(overlap + i)] * samples_2[uint32_t(idx)];
		}
		
		// write out
		out_img.write(img_coord, h_color);
	}
}

// this is the dumb version of the blur, processing a horizontal or vertical line w/o manual caching
// NOTE: this is practically the same as the opengl/glsl shader
template <uint32_t direction /* 0 == horizontal, 1 == vertical */>
floor_inline_always static void image_blur_dumb(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	const int2 img_coord { global_id.xy };
	
	constexpr const auto coeffs = compute_coefficients<TAP_COUNT>();
	constexpr const int overlap = TAP_COUNT / 2;
	
	float4 color;
#pragma clang loop unroll_count(TAP_COUNT)
	for(int i = -overlap; i <= overlap; ++i) {
		color += (coeffs[size_t(overlap + i)] *
#if TAP_COUNT <= 15 // can use texel offset here, TAP_COUNT == 15 has an offset range of [-7, 7]
				  in_img.read(img_coord, int2 { direction == 0 ? i : 0, direction == 0 ? 0 : i })
#else // else: need to resort to integer math
				  in_img.read(img_coord + int2 { direction == 0 ? i : 0, direction == 0 ? 0 : i })
#endif
				  );
	}
	
	out_img.write(img_coord, color);
}

kernel void image_blur_dumb_horizontal(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	image_blur_dumb<0>(in_img, out_img);
}

kernel void image_blur_dumb_vertical(const_image_2d<float> in_img, image_2d<float4, true> out_img) {
	image_blur_dumb<1>(in_img, out_img);
}

#endif
