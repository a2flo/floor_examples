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

#ifndef __FLOOR_WARP_UNIFIED_RENDERER_HPP__
#define __FLOOR_WARP_UNIFIED_RENDERER_HPP__

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/compute_kernel.hpp>
#include "obj_loader.hpp"
#include "camera.hpp"
#include "warp_state.hpp"

// unified Metal/Vulkan renderer
class unified_renderer {
public:
	unified_renderer();
	~unified_renderer();
	
	bool init();
	void destroy();
	void render(const floor_obj_model& model, const camera& cam);
	
	bool rebuild_renderer();
	
	// pipelines
	enum WARP_PIPELINE : uint32_t {
		SCENE_SCATTER = 0,
		SCENE_GATHER,
		SCENE_GATHER_FWD,
		SKYBOX_SCATTER,
		SKYBOX_GATHER,
		SKYBOX_GATHER_FWD,
		SHADOW,
		BLIT,
		BLIT_SWIZZLE,
		__MAX_WARP_PIPELINE
	};
	static constexpr size_t warp_pipeline_count() {
		return (size_t)WARP_PIPELINE::__MAX_WARP_PIPELINE;
	}
	
	// shaders
	enum WARP_SHADER : uint32_t {
		SCENE_SCATTER_VS = 0,
		SCENE_SCATTER_TES,
		SCENE_SCATTER_FS,
		SCENE_GATHER_VS,
		SCENE_GATHER_TES,
		SCENE_GATHER_FS,
		SCENE_GATHER_FWD_VS,
		SCENE_GATHER_FWD_TES,
		SCENE_GATHER_FWD_FS,
		SKYBOX_SCATTER_VS,
		SKYBOX_SCATTER_FS,
		SKYBOX_GATHER_VS,
		SKYBOX_GATHER_FS,
		SKYBOX_GATHER_FWD_VS,
		SKYBOX_GATHER_FWD_FS,
		SHADOW_VS,
		BLIT_VS,
		BLIT_FS,
		BLIT_SWIZZLE_VS,
		BLIT_SWIZZLE_FS,
		TESS_UPDATE_FACTORS,
		__MAX_WARP_SHADER
	};
	static constexpr size_t warp_shader_count() {
		return (size_t)WARP_SHADER::__MAX_WARP_SHADER;
	}
	
	//! initializes the materials argument buffer in the specified model
	void init_model_materials_arg_buffer(const compute_queue& dev_queue, floor_obj_model& model);
	
protected:
	void create_textures(const COMPUTE_IMAGE_TYPE color_format);
	void destroy_textures(bool is_resize);
	
	void create_skybox();
	void destroy_skybox();
	
	event::handler resize_handler_fnctr;
	bool resize_handler(EVENT_TYPE type, shared_ptr<event_object>);
	
	void render_full_scene(const floor_obj_model& model, const camera& cam);
	void render_kernels(const float& delta, const float& render_delta, const size_t& warp_frame_num);
	
	//
	unique_ptr<graphics_pass> scatter_passes[2];
	unique_ptr<graphics_pass> gather_passes[2];
	unique_ptr<graphics_pass> gather_fwd_passes[2];
	unique_ptr<graphics_pass> shadow_pass;
	unique_ptr<graphics_pass> blit_pass;
	void create_passes();
	
	//
	unique_ptr<graphics_pipeline> scatter_pipeline;
	unique_ptr<graphics_pipeline> skybox_scatter_pipeline;
	unique_ptr<graphics_pipeline> gather_pipeline;
	unique_ptr<graphics_pipeline> skybox_gather_pipeline;
	unique_ptr<graphics_pipeline> gather_fwd_pipeline;
	unique_ptr<graphics_pipeline> skybox_gather_fwd_pipeline;
	unique_ptr<graphics_pipeline> shadow_pipeline;
	unique_ptr<graphics_pipeline> blit_pipeline;
	void create_pipelines();
	
	shared_ptr<compute_buffer> tess_factors_buffer;
	
	//
	struct {
		shared_ptr<compute_image> color[2];
		shared_ptr<compute_image> depth[2];
		shared_ptr<compute_image> motion[4];
		shared_ptr<compute_image> motion_depth[2];
		shared_ptr<compute_image> compute_color;
		
		uint2 dim;
		uint2 dim_multiple;
	} scene_fbo;
	struct {
		shared_ptr<compute_image> shadow_image;
		uint2 dim {
#if !defined(FLOOR_IOS)
			16384u
#else
			4096u
#endif
		};
	} shadow_map;
	shared_ptr<compute_image> skybox_tex;
	static constexpr const double4 clear_color { 0.215, 0.412, 0.6, 0.0 };
	bool first_frame { true };
	bool blit_frame { false };
	
	// rendering + warp uniforms
	float3 light_pos;
	matrix4f pm, mvm, rmvm;
	matrix4f prev_mvm, prev_prev_mvm;
	matrix4f prev_rmvm, prev_prev_rmvm;
	matrix4f light_bias_mvpm, light_mvpm;
	
	struct __attribute__((packed)) {
		matrix4f mvpm;
		matrix4f light_bias_mvpm;
		float3 cam_pos;
		float3 light_pos;
		matrix4f m0; // mvm (scatter), next_mvpm (gather)
		matrix4f m1; // prev_mvm (scatter), prev_mvpm (gather)
	} scene_uniforms;
	
	struct {
		matrix4f imvpm;
		matrix4f m0; // prev_imvpm (scatter), next_mvpm (gather)
		matrix4f m1; // unused (scatter), prev_mvpm (gather)
	} skybox_uniforms;
	
	// shader
	shared_ptr<compute_program> shader_prog;
	array<compute_kernel*, size_t(WARP_SHADER::__MAX_WARP_SHADER)> shaders;
	array<const compute_kernel::kernel_entry*, size_t(WARP_SHADER::__MAX_WARP_SHADER)> shader_entries;
	
	bool compile_shaders(const string add_cli_options = "");
	
	// current metal (id <MTLCommandBuffer>) or vulkan command buffer used for rendering
	//void* render_cmd_buffer;
	
};

#endif
