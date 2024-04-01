/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_context.hpp>

struct unified_renderer {
	static bool init(const compute_context& ctx, const compute_queue& dev_queue, const bool enable_msaa,
					 const compute_kernel& vs, const compute_kernel& fs,
					 const compute_kernel* blit_vs, const compute_kernel* blit_fs, const compute_kernel* blit_fs_layered);
	static void destroy(const compute_context& ctx);
	static void render(const compute_context& ctx, const compute_queue& dev_queue, const compute_buffer& position_buffer);
};

#endif
