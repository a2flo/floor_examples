
#if defined(FLOOR_COMPUTE)

typedef uint64_t image;

floor_inline_always uchar4 read_surf_2d(image img_handle, uint2 tex_coord) {
	uchar4 ret;
	asm("suld.b.2d.v4.b8.clamp { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=r"(ret.x), "=r"(ret.y), "=r"(ret.z), "=r"(ret.w) :
		"l"(img_handle), "r"(tex_coord.x * 4), "r"(tex_coord.y));
	return ret;
}
floor_inline_always uchar4 read_surf_2d(image img_handle, int2 tex_coord) {
	uchar4 ret;
	asm("suld.b.2d.v4.b8.clamp { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=r"(ret.x), "=r"(ret.y), "=r"(ret.z), "=r"(ret.w) :
		"l"(img_handle), "r"(tex_coord.x * 4), "r"(tex_coord.y));
	return ret;
}
floor_inline_always float4 read_tex_2d(image img_handle, int2 tex_coord) {
	float4 ret;
	asm("tex.2d.v4.f32.s32 { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=f"(ret.x), "=f"(ret.y), "=f"(ret.z), "=f"(ret.w) :
		"l"(img_handle), "r"(tex_coord.x), "r"(tex_coord.y));
	return ret;
}

floor_inline_always void write_surf_2d(image img_handle, uint2 tex_coord, uchar4 data) {
	asm("sust.b.2d.v4.b8.clamp [%0, { %1, %2 }], { %3, %4, %5, %6 };" : :
		"l"(img_handle), "r"(tex_coord.x * 4), "r"(tex_coord.y),
		"r"(data.x), "r"(data.y), "r"(data.z), "r"(data.w));
}
floor_inline_always void write_surf_2d(image img_handle, int2 tex_coord, uchar4 data) {
	asm("sust.b.2d.v4.b8.clamp [%0, { %1, %2 }], { %3, %4, %5, %6 };" : :
		"l"(img_handle), "r"(tex_coord.x * 4), "r"(tex_coord.y),
		"r"(data.x), "r"(data.y), "r"(data.z), "r"(data.w));
}

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
static_assert(TAP_COUNT % 2 == 1, "tap count must be an odd number!");
kernel void image_blur(image in_img, image out_img) {
	const int2 gid { get_global_id(0), get_global_id(1) };
	const int2 lid { get_local_id(0), get_local_id(1) };
	const int lin_lid { lid.y * TILE_SIZE + lid.x };
	
	// awesome constexpr magic
	constexpr const auto coeffs = compute_coefficients<TAP_COUNT>();
	// aka half kernel width or radius
	constexpr const int overlap = TAP_COUNT / 2;
	
	// this uses local memory as a sample + compute cache
	// note that using a float4 instead of a uchar4 requires more storage, but computation using float4s
	// is _a lot_ faster than integer math or doing int->float conversions + float math
	constexpr const auto sample_count = TILE_SIZE * TILE_SIZE;
	local_buffer<float4, sample_count> samples;
	
	// map from the global work size to the actual image size
	const int2 img_coord = (gid / TILE_SIZE) * INNER_TILE_SIZE + (lid - overlap);
	// read the input pixel and store it in the local buffer/"cache" (note: out-of-bound access is clamped)
#if 0
	samples[lin_lid] = float4(read_surf_2d(in_img, img_coord));
#else
	samples[lin_lid] = read_tex_2d(in_img, img_coord);
#endif
	local_barrier();
	
	// horizontal blur:
	// this must be done for the inner tile and the vertical overlapping part (necessary for the vertical blur)
	// also note that doing this branch is faster than clamping idx to [0, TILE_SIZE - 1]
	if(lid.x >= overlap && lid.x < (TILE_SIZE - overlap)) {
		float4 h_color;
		int idx = lid.y * TILE_SIZE + lid.x - overlap;
#pragma clang loop unroll_count(TAP_COUNT)
		for(int i = -overlap; i <= overlap; ++i) {
			// note that this will be optimized to an fma instruction if possible
			h_color += samples[idx] * coeffs[overlap + i];
			++idx;
		}
		// and write the sample back to the local cache
		samples[lin_lid] = h_color;
	}
	local_barrier();
	
	// vertical blur:
	// this must only be done for the inner tile
	// again, branch so we don't have to clamp in here
	if(lid.x >= overlap && lid.x < (TILE_SIZE - overlap) &&
	   lid.y >= overlap && lid.y < (TILE_SIZE - overlap)) {
		float4 v_color;
		int idx = (lid.y - overlap) * TILE_SIZE + lid.x;
#pragma clang loop unroll_count(TAP_COUNT)
		for(int i = -overlap; i <= overlap; ++i) {
			v_color += samples[idx] * coeffs[overlap + i];
			idx += TILE_SIZE;
		}
		
		// write out
#if 0
		write_surf_2d(out_img, img_coord, uchar4(v_color));
#else
		write_surf_2d(out_img, img_coord, uchar4(v_color * 255.0f));
#endif
	}
}

#endif
