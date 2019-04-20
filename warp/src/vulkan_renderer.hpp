/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_WARP_VULKAN_RENDERER_HPP__
#define __FLOOR_WARP_VULKAN_RENDERER_HPP__

#include "common_renderer.hpp"
#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#endif

class vulkan_renderer final : public common_renderer {
public:
	bool init() override;
	void destroy() override;
	void render(const floor_obj_model& model, const camera& cam) override;
	
protected:
	void create_textures(const COMPUTE_IMAGE_TYPE color_format = COMPUTE_IMAGE_TYPE::BGRA8UI_NORM) override;
	bool compile_shaders(const string add_cli_options) override;
	
	void render_full_scene(const floor_obj_model& model, const camera& cam) override;
	
#if !defined(FLOOR_NO_VULKAN)
	bool make_pipeline(const vulkan_device& vk_dev,
					   VkRenderPass render_pass,
					   const WARP_PIPELINE pipeline_id,
					   const WARP_SHADER vertex_shader,
					   const WARP_SHADER fragment_shader,
					   const uint2& render_size,
					   const uint32_t color_attachment_count,
					   const bool has_depth_attachment,
					   const VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS);
	
	const vulkan_kernel::vulkan_kernel_entry* get_shader_entry(const WARP_SHADER& shader) const;
#endif
	
};

#endif
