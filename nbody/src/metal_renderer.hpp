/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_NBODY_METAL_RENDERER_HPP__
#define __FLOOR_NBODY_METAL_RENDERER_HPP__

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_context.hpp>

struct metal_renderer {
	static bool init(shared_ptr<compute_device> dev,
					 shared_ptr<compute_kernel> vs,
					 shared_ptr<compute_kernel> fs);
	static void render(shared_ptr<compute_queue> dev_queue,
					   shared_ptr<compute_buffer> position_buffer);
};

#endif
