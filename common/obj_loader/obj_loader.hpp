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

#ifndef __FLOOR_OBJ_LOADER_HPP__
#define __FLOOR_OBJ_LOADER_HPP__

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_image.hpp>

struct obj_model {
	vector<float3> vertices { float3 { 0.0f } };
	vector<float2> tex_coords { float2 { 0.0f } };
	vector<float3> normals { float3 { 0.0f, 1.0f, 0.0f } };
	vector<float3> binormals { float3 { 1.0f, 0.0f, 0.0f } };
	vector<float3> tangents { float3 { 0.0f, 0.0f, -1.0f } };
	vector<uint32_t> materials_data { 0 }; // { index : 16, should-displace : 16 }
	
	struct material_info {
		string name;
		string diffuse_file_name;
		string specular_file_name;
		string normal_file_name;
		string mask_file_name;
		string displacement_file_name;
		bool has_proper_displacement { false };
	};
	vector<material_info> material_infos;
	
	struct sub_object {
		const string name;
		vector<uint3> indices;
		uint32_t index_count { 0u };
		uint32_t mat_idx { 0u };
		shared_ptr<compute_buffer> indices_floor_vbo;
	};
	vector<unique_ptr<sub_object>> objects;
};

struct floor_obj_model : obj_model {
	shared_ptr<compute_buffer> vertices_buffer;
	shared_ptr<compute_buffer> tex_coords_buffer;
	shared_ptr<compute_buffer> normals_buffer;
	shared_ptr<compute_buffer> binormals_buffer;
	shared_ptr<compute_buffer> tangents_buffer;
	shared_ptr<compute_buffer> materials_data_buffer;
	
	// contains *all* indices
	shared_ptr<compute_buffer> indices_buffer;
	uint32_t index_count { 0u };
	
	// all materials
	vector<compute_image*> diffuse_textures;
	vector<compute_image*> specular_textures;
	vector<compute_image*> normal_textures;
	vector<compute_image*> mask_textures;
	vector<compute_image*> displacement_textures;
	shared_ptr<argument_buffer> materials_arg_buffer;
	
	vector<shared_ptr<compute_image>> textures;
	
	shared_ptr<argument_buffer> model_data_arg_buffer;
};

class obj_loader {
public:
	static shared_ptr<obj_model> load(const string& file_name, bool& success,
									  const compute_context& ctx,
									  const compute_queue& cqueue,
									  const float scale = 0.1f,
									  const bool cleanup_cpu_data = true,
									  const bool is_load_textures = true,
									  const bool create_gpu_buffers = true,
									  const COMPUTE_MEMORY_FLAG add_mem_flags = COMPUTE_MEMORY_FLAG::NONE);
	
	struct pvrtc_texture {
		uint2 dim;
		uint32_t bpp;
		bool is_mipmapped;
		bool is_alpha;
		unique_ptr<uint8_t[]> pixels;
		size_t image_size;
	};
	
	struct texture {
		SDL_Surface* surface { nullptr };
		shared_ptr<pvrtc_texture> pvrtc {};
	};
	static pair<bool, texture> load_texture(const string& filename);
	
	static COMPUTE_IMAGE_TYPE floor_image_type_format(const SDL_Surface* surface);
	
protected:
	// static class
	obj_loader(const obj_loader&) = delete;
	~obj_loader() = delete;
	obj_loader& operator=(const obj_loader&) = delete;
	
};

#endif
