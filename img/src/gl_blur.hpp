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

#ifndef __FLOOR_IMG_GL_BLUR_HPP__
#define __FLOOR_IMG_GL_BLUR_HPP__

#if !defined(FLOOR_IOS)

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/core/gl_shader.hpp>

struct gl_blur {
	static bool init(const uint2& dim, const uint32_t& tap_count);
	
	static void blur(const GLuint& tex_src, const GLuint& tex_dst, const GLuint& tex_tmp,
					 const GLuint& vbo_fullscreen_triangle);
	
};

#endif

#endif
