/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#ifndef __FLOOR_NBODY_UNIFIED_RENDERER_HPP__
#define __FLOOR_NBODY_UNIFIED_RENDERER_HPP__

#include <floor/floor.hpp>
#include <floor/device/device_context.hpp>
using namespace fl;

struct unified_renderer {
	static bool init(const device_context& ctx, const device_queue& dev_queue, const bool enable_msaa,
					 const device_function& vs, const device_function& fs,
					 const device_function* blit_vs, const device_function* blit_fs, const device_function* blit_fs_layered);
	static void destroy(const device_context& ctx);
	static void render(const device_context& ctx, const device_queue& dev_queue, const device_buffer& position_buffer);
	static bool resize_handler(EVENT_TYPE type, std::shared_ptr<event_object> evt);
};

#endif
