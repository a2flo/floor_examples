
#ifndef __NBODY_HPP__
#define __NBODY_HPP__

#include <floor/math/vector_lib.hpp>

#if defined(FLOOR_COMPUTE)

// prototypes
kernel void nbody_compute(buffer<const float4> in_positions,
						  buffer<float4> out_positions,
						  buffer<float3> velocities,
						  param<float> delta);

kernel void nbody_raster(buffer<float4> positions,
						 buffer<float3> img,
						 buffer<float3> img_old,
						 param<uint2> img_size,
						 param<uint32_t> body_count,
						 param<matrix4f> mview);

#endif

#endif
