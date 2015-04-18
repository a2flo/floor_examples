
#if defined(FLOOR_COMPUTE)

// computes the blur coefficients for the specified tap count (at compile-time)
template <uint32_t tap_count>
constexpr auto compute_coefficients() {
	array<float, tap_count> ret {};
	
	// compute binomial coefficients and divide them by 2^(tap count - 1)
	const float sum_div = const_math::pow(2u, tap_count - 1u);
	for(uint32_t i = 0u; i < tap_count; ++i) {
		// n! / (k! * (n - k)!)
		const auto coeff = (const_math::factorial(tap_count - 1u) / (const_math::factorial(i) * const_math::factorial(tap_count - 1u - i)));
		// / 2^N
		ret[i] = float(coeff) / sum_div;
	}
	
	return ret;
}

// defined during compilation:
// TAP_COUNT: kernel width, -> N*N filter
// TILE_SIZE: (local) work-group size, this is INNER_TILE_SIZE + (TAPCOUNT / 2) * 2, thus includes the overlap
// INNER_TILE_SIZE: the image portion that will actually be computed + output in here
// SECOND_CACHE: flag that determines if a second local/shared memory "cache" is used, so that one barrier can be skipped
static_assert(TAP_COUNT % 2 == 1, "tap count must be an odd number!");
kernel void image_blur(ro_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> in_img,
					   wo_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI> out_img) {
	const int2 gid { get_global_id(0), get_global_id(1) };
	const int2 lid { get_local_id(0), get_local_id(1) };
	const int lin_lid { lid.y * TILE_SIZE + lid.x };
	
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
	samples[lin_lid] = read(in_img, img_coord);
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
			v_color += coeffs[overlap + i] * samples[idx];
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
			h_color += coeffs[overlap + i] * samples_2[idx];
		}
		
		// write out
		write(out_img, img_coord, h_color);
	}
}

#endif
