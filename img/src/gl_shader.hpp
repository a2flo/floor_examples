/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_IMG_GL_SHADER_HPP__
#define __FLOOR_IMG_GL_SHADER_HPP__

#include <floor/floor/floor.hpp>

struct img_shader_object {
	struct internal_shader_object {
		unsigned int program { 0 };
		unsigned int vertex_shader { 0 };
		unsigned int fragment_shader { 0 };
		
		struct shader_variable {
			size_t location;
			size_t size;
			size_t type;
		};
		unordered_map<string, shader_variable> uniforms;
		unordered_map<string, shader_variable> attributes;
		unordered_map<string, size_t> samplers;
	};
	const string name;
	internal_shader_object program;
	
	img_shader_object(const string& shd_name) : name(shd_name) {}
	~img_shader_object() {}
};

bool compile_shader(img_shader_object& shd, const char* vs_text, const char* fs_text);

#endif
