/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_WARP_GL_RENDERER_HPP__
#define __FLOOR_WARP_GL_RENDERER_HPP__

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_context.hpp>
#include "obj_loader.hpp"
#include "camera.hpp"
#include "warp_state.hpp"

struct gl_renderer {
	static bool init();
	static void destroy();
	static bool render(const gl_obj_model& model, const camera& cam);
	
	// internal
	static bool compile_shaders();
	static void blit(const bool full_scene);
	static void render_kernels(const float& delta, const float& render_delta,
							   const size_t& warp_frame_num);
	static void render_full_scene(const gl_obj_model& model, const camera& cam);
};

#endif
