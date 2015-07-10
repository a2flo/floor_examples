
#ifndef __FLOOR_PATH_TRACER_HPP__
#define __FLOOR_PATH_TRACER_HPP__

#include <floor/core/essentials.hpp>

#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

// prototypes
kernel void path_trace(buffer<float4> img,
					   param<uint32_t> iteration,
					   param<uint32_t> seed,
					   param<uint2> img_size);

#endif
