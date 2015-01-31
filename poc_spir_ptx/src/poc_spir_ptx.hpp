
#ifndef __POC_SPIR_PTX_HPP__
#define __POC_SPIR_PTX_HPP__

#include <floor/math/vector_lib.hpp>

// opencl/cuda syntax differ slightly from std c++ syntax in that they require certain
// keywords in certain places, which the host compiler doesn't understand.
// -> add a cheap workaround for now
// (note that proper source compat is still wip)
#if !defined(FLOOR_LLVM_COMPUTE)
#define kernel
#define global
#define constant
#define opencl_constant
#define device_constant
static atomic<uint32_t> global_id_counter { 0 };
#define get_global_id(dim) (global_id_counter++)
void reset_counter();
#else
#define reset_counter()
#endif

// prototypes
kernel void path_trace(global float3* img,
					   const uint32_t iteration,
					   const uint32_t seed,
					   const uint2 img_size);

#endif
