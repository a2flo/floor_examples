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

#ifndef __FLOOR_WARP_UNIFIED_RENDERER_HPP__
#define __FLOOR_WARP_UNIFIED_RENDERER_HPP__

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/indirect_command.hpp>
#include "obj_loader.hpp"
#include "camera.hpp"
#include "warp_state.hpp"
#include "warp_shaders.hpp"
#include <libwarp/libwarp.h>

class render_thread;

// unified Metal/Vulkan renderer
class unified_renderer {
public:
	unified_renderer();
	~unified_renderer();
	
	bool init();
	void render();
	
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
		BLIT_DEBUG_DEPTH_FS,
		BLIT_DEBUG_MOTION_2D_FS,
		BLIT_DEBUG_MOTION_3D_FS,
		BLIT_DEBUG_MOTION_DEPTH_FS,
		TESS_UPDATE_FACTORS,
		__MAX_WARP_SHADER
	};
	static constexpr size_t warp_shader_count() {
		return (size_t)WARP_SHADER::__MAX_WARP_SHADER;
	}
	
	//! various post-initialization:
	//!  * initializes the model/materials argument buffers in the specified model
	//!  * creates tessellation factors buffer
	//!  * encode indirect pipelines
	void post_init(const compute_queue& dev_queue, floor_obj_model& model, const camera& cam);
	
	// FPS functions
	uint32_t get_fps();
	bool is_new_fps_count();
	
	//! RAII object for automatically starting and ending a renderer flush within a scope
	struct flush_renderer_raii_t {
		unified_renderer& renderer;
		flush_renderer_raii_t(unified_renderer& renderer_) : renderer(renderer_) {
			renderer.flush_renderer_begin();
		}
		~flush_renderer_raii_t() {
			renderer.flush_renderer_end();
		}
	};
	
	//! for debugging purposes: blits different computed/rendered framebuffers
	void set_debug_blit_mode(const uint32_t mode) {
		debug_blit_mode = mode;
	}
	
protected:
	friend render_thread;
	
	// global model and camera state (available in/after post_init())
	floor_obj_model* model { nullptr };
	const camera* cam { nullptr };
	
	void create_frame_buffer_images(const COMPUTE_IMAGE_TYPE color_format);
	void destroy_frame_buffer_images(bool is_resize);
	
	void create_skybox();
	void destroy_skybox();
	
	event::handler resize_handler_fnctr;
	bool resize_handler(EVENT_TYPE type, shared_ptr<event_object>);
	
	//
	unique_ptr<graphics_pass> scatter_passes[2];
	unique_ptr<graphics_pass> gather_passes[2];
	unique_ptr<graphics_pass> gather_fwd_passes[2];
	unique_ptr<graphics_pass> shadow_pass;
	unique_ptr<graphics_pass> blit_pass;
	void create_passes();
	
	// direct rendering + pipeline state
	unique_ptr<graphics_pipeline> scatter_pipeline;
	unique_ptr<graphics_pipeline> skybox_scatter_pipeline;
	unique_ptr<graphics_pipeline> gather_pipeline;
	unique_ptr<graphics_pipeline> skybox_gather_pipeline;
	unique_ptr<graphics_pipeline> gather_fwd_pipeline;
	unique_ptr<graphics_pipeline> skybox_gather_fwd_pipeline;
	unique_ptr<graphics_pipeline> shadow_pipeline;
	unique_ptr<graphics_pipeline> blit_pipeline;
	unique_ptr<graphics_pipeline> blit_debug_depth_pipeline;
	unique_ptr<graphics_pipeline> blit_debug_motion_2d_pipeline;
	unique_ptr<graphics_pipeline> blit_debug_motion_3d_pipeline;
	unique_ptr<graphics_pipeline> blit_debug_motion_depth_pipeline;
	bool create_pipelines();
	
	//! current amount of in flight frames
	atomic<uint32_t> in_flight_frames { 0u };
	//! the index of the next frame that will be rendered
	uint32_t next_frame_idx { 0u };
	//! set to true to signal renderer halt
	atomic<bool> halt_renderer { false };
	//! 0: queued new frame, 1: ready for next frame
	//! NOTE: this is used for throttling
	atomic<uint32_t> frame_render_state { 1 };
	//! is warping active in any frame right now?
	atomic<bool> warp_active { false };
	
	// we'll try to render multiple frames at once -> need separate objects to make that work
	static constexpr const uint32_t max_frames_in_flight { 2u };
	struct frame_object_t {
		// per-frame queue
		shared_ptr<compute_queue> dev_queue;
		
		// render thread for this frame
		unique_ptr<render_thread> rthread;
		
		//! current camera state for this frame
		camera::camera_state_t cam_state;
		
		// manual sync
		unique_ptr<compute_fence> render_post_tess_update_fence;
		unique_ptr<compute_fence> render_post_shadow_fence;
		unique_ptr<compute_fence> render_post_scene_fence;
		unique_ptr<compute_fence> render_post_skybox_fence;
		
		// indirect rendering (if supported)
		unique_ptr<indirect_command_pipeline> indirect_shadow_pipeline;
		unique_ptr<indirect_command_pipeline> indirect_scene_scatter_pipeline;
		unique_ptr<indirect_command_pipeline> indirect_scene_gather_pipeline;
		unique_ptr<indirect_command_pipeline> indirect_skybox_scatter_pipeline;
		unique_ptr<indirect_command_pipeline> indirect_skybox_gather_pipeline;
		unique_ptr<argument_buffer> indirect_scene_fs_data;
		unique_ptr<argument_buffer> indirect_sky_fs_data;
		
		// other per-frame data
		shared_ptr<compute_buffer> frame_uniforms_buffer;
		shared_ptr<compute_buffer> tess_factors_buffer;
		
		//! signals that the frame has finished rendering and can now be blitted to the screen
		atomic_bool render_done { false };
		//! signals that the frame has been presented
		atomic_bool present_done { true };
		
		//! current frame delta (including warped frames)
		float frame_delta { 0.0f };
		//! current render delta (only rendered frames)
		float render_delta { 0.0f };
		
		//! warping data
		struct {
			//! is warping active for the current frame?
			atomic_bool is_active { false };
			//! current warp frame number/index
			uint32_t frame_num { 0u };
		} warp;
		
		// frame buffer images
		struct {
			shared_ptr<compute_image> color;
			shared_ptr<compute_image> depth;
			shared_ptr<compute_image> motion[2];
			shared_ptr<compute_image> motion_depth;
			shared_ptr<compute_image> compute_color;
			
			uint2 dim;
			uint2 dim_multiple;
		} scene_fbo;
		
		struct {
			shared_ptr<compute_image> shadow_image;
			uint2 dim {
#if !defined(FLOOR_IOS)
				8192u
#else
				2048u
#endif
			};
		} shadow_map;
		
		//! signal that all indirect pipelines need to be rebuild (referenced buffers became invalid)
		atomic_bool rebuild_pipelines { false };
		
		//! storage of the current epoch
		atomic<uint64_t> current_epoch { 0u };
		
		//! advance to the next epoch
		uint64_t next_epoch() {
			return ++current_epoch;
		}
		
		//! returns the current epoch
		uint64_t get_epoch() const {
			return current_epoch.load();
		}
	};
	frame_object_t frame_objects[max_frames_in_flight];
	
	//! need to ensure that only one renderer flush is active at a time
	safe_mutex renderer_flush_lock;
	//! flushes the renderer pipeline:
	//! wait until all currently in-flight frames have been rendered+presented and stop scheduling new frames
	void flush_renderer_begin() REQUIRES(!renderer_flush_lock) ACQUIRE(renderer_flush_lock);
	//! end the renderer pipeline flush:
	//! restart scheduling of new frames
	void flush_renderer_end() REQUIRES(renderer_flush_lock) RELEASE(renderer_flush_lock);
	
	void encode_indirect_pipelines(const uint32_t frame_idx);
	
	void render_full_scene(const uint32_t frame_idx, const float time_delta);
	void render_kernels(const float& delta, const float& render_delta, const uint32_t& warp_frame_num,
						const frame_object_t& cur_frame, const frame_object_t& prev_frame);
	void update_uniforms(const uint32_t frame_idx, const float time_delta);
	
	//
	shared_ptr<compute_image> skybox_tex;
	static constexpr const double4 clear_color { 0.215, 0.412, 0.6, 0.0 };
	// 0 = off, 1 = depth, 2 = motion 2D, 3 = motion 3D #0, 4 = motion 3D #1, 5 = motion depth
	atomic<uint32_t> debug_blit_mode { 0u };
	
	// rendering + warp uniforms
	float3 light_pos;
	matrix4f pm, mvm, rmvm;
	matrix4f prev_mvm, prev_prev_mvm;
	matrix4f prev_rmvm, prev_prev_rmvm;
	
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
	
	warp_shaders::frame_uniforms_t frame_uniforms;
	
	libwarp_camera_setup cam_setup {};
	
	// shader
	shared_ptr<compute_program> shader_prog;
	array<compute_kernel*, size_t(WARP_SHADER::__MAX_WARP_SHADER)> shaders;
	array<const compute_kernel::kernel_entry*, size_t(WARP_SHADER::__MAX_WARP_SHADER)> shader_entries;
	
	bool compile_shaders(const string add_cli_options = "");
	
	// FPS counting
	safe_mutex fps_state_lock;
	atomic<uint32_t> cur_fps_count { 0u };
	atomic_bool new_fps_count { false };
	struct fps_state_t {
		//! start point of the current "second"/epoch
		chrono::steady_clock::time_point sec_start_point { chrono::steady_clock::now() };
		//! number of rendered frames since "sec_start_point"
		uint32_t fps_count { 0u };
	} fps_state GUARDED_BY(fps_state_lock);
	void finish_frame(const uint32_t rendered_frame_idx) REQUIRES(!fps_state_lock);
	
};

#endif
