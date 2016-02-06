
#ifndef __FLOOR_PATH_TRACER_HPP__
#define __FLOOR_PATH_TRACER_HPP__

#include <floor/core/essentials.hpp>

#if defined(FLOOR_COMPUTE)

#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

#if !defined(SCREEN_HEIGHT)
#define SCREEN_HEIGHT 512
#endif

#if !defined(SCREEN_WIDTH)
#define SCREEN_WIDTH 512
#endif

#endif

#endif
