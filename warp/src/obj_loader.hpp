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

#ifndef __FLOOR_OBJ_LOADER_HPP__
#define __FLOOR_OBJ_LOADER_HPP__

#include <floor/floor/floor.hpp>

struct obj_model {
	vector<float3> vertices { float3 { 0.0f } };
	vector<float2> tex_coords { float2 { 0.0f } };
	vector<float3> normals { float3 { 0.0f, 1.0f, 0.0f } };
	vector<float3> binormals { float3 { 1.0f, 0.0f, 0.0f } };
	vector<float3> tangents { float3 { 0.0f, 0.0f, -1.0f } };
	GLuint vertices_vbo { 0u };
	GLuint tex_coords_vbo { 0u };
	GLuint normals_vbo { 0u };
	GLuint binormals_vbo { 0u };
	GLuint tangents_vbo { 0u };
	
	struct material {
		GLuint diffuse { 0u };
		GLuint specular { 0u };
		GLuint normal { 0u };
		GLuint mask { 0u };
	};
	vector<material> materials;
	vector<GLuint> textures;
	
	struct material_info {
		string name;
		string diffuse_file_name;
		string specular_file_name;
		string normal_file_name;
		string mask_file_name;
	};
	vector<material_info> material_infos;
	
	struct sub_object {
		const string name;
		vector<uint3> indices;
		GLsizei triangle_count { 0u };
		GLuint indices_vbo { 0u };
		uint32_t mat_idx { 0u };
	};
	vector<unique_ptr<sub_object>> objects;
	
};

class obj_loader {
public:
	static obj_model load(const string& file_name, bool& success);
	
protected:
	// static class
	obj_loader(const obj_loader&) = delete;
	~obj_loader() = delete;
	obj_loader& operator=(const obj_loader&) = delete;
	
};

#endif
