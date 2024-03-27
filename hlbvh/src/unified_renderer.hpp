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

#ifndef __FLOOR_HLBVH_UNIFIED_RENDERER_HPP__
#define __FLOOR_HLBVH_UNIFIED_RENDERER_HPP__

#include <floor/core/essentials.hpp>
#include <floor/floor/floor.hpp>
#include <floor/compute/compute_context.hpp>
#include "animation.hpp"
#include "camera.hpp"

struct unified_renderer {
	static bool init(shared_ptr<compute_kernel> vs,
					 shared_ptr<compute_kernel> fs);
	static void destroy();
	static void render(const vector<unique_ptr<animation>>& models,
					   const bool cam_mode,
					   const camera& cam);
};

#endif
