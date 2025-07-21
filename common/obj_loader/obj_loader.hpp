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

#pragma once

#include <floor/floor.hpp>
#include <floor/device/device_buffer.hpp>
#include <floor/device/device_image.hpp>
using namespace fl;

struct obj_model {
	std::vector<float3> vertices { float3 { 0.0f } };
	std::vector<float2> tex_coords { float2 { 0.0f } };
	std::vector<float3> normals { float3 { 0.0f, 1.0f, 0.0f } };
	std::vector<float3> binormals { float3 { 1.0f, 0.0f, 0.0f } };
	std::vector<float3> tangents { float3 { 0.0f, 0.0f, -1.0f } };
	std::vector<uint32_t> materials_data { 0 }; // { index : 16, should-displace : 16 }
	
	struct material_info {
		std::string name;
		std::string diffuse_file_name;
		std::string specular_file_name;
		std::string normal_file_name;
		std::string mask_file_name;
		std::string displacement_file_name;
		bool has_proper_displacement { false };
	};
	std::vector<material_info> material_infos;
	
	struct sub_object {
		const std::string name;
		std::vector<uint3> indices;
		uint32_t index_count { 0u };
		uint32_t mat_idx { 0u };
		std::shared_ptr<device_buffer> indices_floor_vbo;
	};
	std::vector<std::unique_ptr<sub_object>> objects;
};

struct floor_obj_model : obj_model {
	std::shared_ptr<device_buffer> vertices_buffer;
	std::shared_ptr<device_buffer> tex_coords_buffer;
	std::shared_ptr<device_buffer> normals_buffer;
	std::shared_ptr<device_buffer> binormals_buffer;
	std::shared_ptr<device_buffer> tangents_buffer;
	std::shared_ptr<device_buffer> materials_data_buffer;
	
	// contains *all* indices
	std::shared_ptr<device_buffer> indices_buffer;
	uint32_t index_count { 0u };
	
	// all materials
	std::vector<device_image*> diffuse_textures;
	std::vector<device_image*> specular_textures;
	std::vector<device_image*> normal_textures;
	std::vector<device_image*> mask_textures;
	std::vector<device_image*> displacement_textures;
	std::shared_ptr<argument_buffer> materials_arg_buffer;
	
	std::vector<std::shared_ptr<device_image>> textures;
	
	std::shared_ptr<argument_buffer> model_data_arg_buffer;
};

class obj_loader {
public:
	static std::shared_ptr<obj_model> load(const std::string& file_name, bool& success,
										   const device_context& ctx,
										   const device_queue& cqueue,
										   const float scale = 0.1f,
										   const bool cleanup_cpu_data = true,
										   const bool is_load_textures = true,
										   const bool create_gpu_buffers = true,
										   const MEMORY_FLAG add_mem_flags = MEMORY_FLAG::NONE);
	
	struct pvrtc_texture {
		uint2 dim;
		uint32_t bpp;
		bool is_mipmapped;
		bool is_alpha;
		std::unique_ptr<uint8_t[]> pixels;
		size_t image_size;
	};
	
	struct texture {
		SDL_Surface* surface { nullptr };
		std::shared_ptr<pvrtc_texture> pvrtc {};
	};
	static std::pair<bool, texture> load_texture(const std::string& filename);
	
	static IMAGE_TYPE floor_image_type_format(const SDL_Surface* surface);
	
protected:
	// static class
	obj_loader(const obj_loader&) = delete;
	~obj_loader() = delete;
	obj_loader& operator=(const obj_loader&) = delete;
	
};
