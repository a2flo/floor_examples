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

#include "common_renderer.hpp"

#include <floor/core/timer.hpp>
#include <floor/core/file_io.hpp>
#include <libwarp/libwarp.h>

common_renderer::common_renderer() :
resize_handler_fnctr(bind(&common_renderer::resize_handler, this, placeholders::_1, placeholders::_2)) {
	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
}

common_renderer::~common_renderer() {
	floor::get_event()->remove_event_handler(resize_handler_fnctr);
}

void common_renderer::create_textures(const COMPUTE_IMAGE_TYPE color_format) {
	scene_fbo.dim = floor::get_physical_screen_size();
	
	// kernel work-group size is { 32, 16 } -> round global size to a multiple of it
	scene_fbo.dim_multiple = scene_fbo.dim.rounded_next_multiple(warp_state.tile_size);
	
	for(size_t i = 0, count = size(scene_fbo.color); i < count; ++i) {
		scene_fbo.color[i] = warp_state.ctx->create_image(*warp_state.dev_queue, scene_fbo.dim,
														  color_format |
														  COMPUTE_IMAGE_TYPE::IMAGE_2D |
														  COMPUTE_IMAGE_TYPE::READ_WRITE |
														  COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
														  COMPUTE_MEMORY_FLAG::READ_WRITE);
		
		scene_fbo.depth[i] = warp_state.ctx->create_image(*warp_state.dev_queue, scene_fbo.dim,
														  COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
														  COMPUTE_IMAGE_TYPE::D32F |
														  COMPUTE_IMAGE_TYPE::READ |
														  COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
														  COMPUTE_MEMORY_FLAG::READ);
		
		for(size_t j = 0; j < 2; ++j) {
			scene_fbo.motion[i * 2 + j] = warp_state.ctx->create_image(*warp_state.dev_queue, scene_fbo.dim,
																	   COMPUTE_IMAGE_TYPE::IMAGE_2D |
																	   COMPUTE_IMAGE_TYPE::R32UI |
																	   COMPUTE_IMAGE_TYPE::READ_WRITE |
																	   COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
																	   COMPUTE_MEMORY_FLAG::READ_WRITE);
		}
		
		scene_fbo.motion_depth[i] = warp_state.ctx->create_image(*warp_state.dev_queue, scene_fbo.dim,
																 COMPUTE_IMAGE_TYPE::IMAGE_2D |
																 COMPUTE_IMAGE_TYPE::RG16F |
																 COMPUTE_IMAGE_TYPE::READ_WRITE |
																 COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
																 COMPUTE_MEMORY_FLAG::READ_WRITE);
	}
	
	scene_fbo.compute_color = warp_state.ctx->create_image(*warp_state.dev_queue, scene_fbo.dim,
														   COMPUTE_IMAGE_TYPE::IMAGE_2D |
														   COMPUTE_IMAGE_TYPE::BGRA8UI_NORM |
														   COMPUTE_IMAGE_TYPE::READ_WRITE,
														   COMPUTE_MEMORY_FLAG::READ_WRITE);
	
	// create appropriately sized s/w depth buffer
	warp_state.scatter_depth_buffer = warp_state.ctx->create_buffer(*warp_state.dev_queue, sizeof(float) *
																	size_t(scene_fbo.dim.x) * size_t(scene_fbo.dim.y));
}

void common_renderer::destroy_textures(bool is_resize) {
	scene_fbo.compute_color = nullptr;
	for(auto& img : scene_fbo.color) {
		img = nullptr;
	}
	for(auto& img : scene_fbo.depth) {
		img = nullptr;
	}
	for(auto& img : scene_fbo.motion) {
		img = nullptr;
	}
	for(auto& img : scene_fbo.motion_depth) {
		img = nullptr;
	}
	
	// only need to destroy this on exit (not on resize!)
	if(!is_resize) shadow_map.shadow_image = nullptr;
}

void common_renderer::create_skybox() {
	static const char* skybox_filenames[] {
		"skybox/posx.png",
		"skybox/negx.png",
		"skybox/posy.png",
		"skybox/negy.png",
		"skybox/posz.png",
		"skybox/negz.png",
	};
	
	array<SDL_Surface*, size(skybox_filenames)> skybox_surfaces;
	auto surf_iter = begin(skybox_surfaces);
	for(const auto& filename : skybox_filenames) {
		const auto tex = obj_loader::load_texture(floor::data_path(filename));
		if(!tex.first) {
			log_error("failed to load sky box");
			return;
		}
		*surf_iter++ = tex.second.surface;
	}
	
	//
	COMPUTE_IMAGE_TYPE image_type = obj_loader::floor_image_type_format(skybox_surfaces[0]);
	image_type |= (COMPUTE_IMAGE_TYPE::IMAGE_CUBE |
				   //COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED |
				   COMPUTE_IMAGE_TYPE::READ);
	
	// need to copy the data to a continuous array
	const uint2 skybox_dim { uint32_t(skybox_surfaces[0]->w), uint32_t(skybox_surfaces[0]->h) };
	const auto skybox_size = image_data_size_from_types(skybox_dim, image_type);
	const auto slice_size = image_slice_data_size_from_types(skybox_dim, image_type);
	auto skybox_pixels = make_unique<uint8_t[]>(skybox_size);
	for(size_t i = 0, count = size(skybox_surfaces); i < count; ++i) {
		memcpy(skybox_pixels.get() + i * slice_size, skybox_surfaces[i]->pixels, slice_size);
	}
	
	skybox_tex = warp_state.ctx->create_image(*warp_state.dev_queue,
											  skybox_dim,
											  image_type,
											  skybox_pixels.get(),
											  COMPUTE_MEMORY_FLAG::READ |
											  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE /*|
											  COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS*/);
}

void common_renderer::destroy_skybox() {
	skybox_tex = nullptr;
}

bool common_renderer::resize_handler(EVENT_TYPE type, shared_ptr<event_object>) {
	if(type == EVENT_TYPE::WINDOW_RESIZE) {
		destroy_textures(true);
		create_textures();
		first_frame = true;
		return true;
	}
	return false;
}

bool common_renderer::init() {
	if(!compile_shaders()) {
		return false;
	}
	
	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
	
	// sky box
	create_skybox();
	
	// shadow map
	shadow_map.shadow_image = warp_state.ctx->create_image(*warp_state.dev_queue, shadow_map.dim,
														   COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
														   COMPUTE_IMAGE_TYPE::D32F |
														   COMPUTE_IMAGE_TYPE::READ |
														   COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
														   COMPUTE_MEMORY_FLAG::READ);
	
	// uniforms
	light_mvpm_buffer = warp_state.ctx->create_buffer(*warp_state.dev_queue, sizeof(matrix4f));
	scene_uniforms_buffer = warp_state.ctx->create_buffer(*warp_state.dev_queue, sizeof(scene_uniforms));
	skybox_uniforms_buffer = warp_state.ctx->create_buffer(*warp_state.dev_queue, sizeof(skybox_uniforms));
	
	return true;
}

void common_renderer::destroy() {
	destroy_textures(false);
	destroy_skybox();
}

void common_renderer::render(const floor_obj_model& model, const camera& cam) {
	// time keeping
	static const long double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
	static auto time_keeper = chrono::high_resolution_clock::now();
	const auto now = chrono::high_resolution_clock::now();
	const auto delta = now - time_keeper;
	const auto deltaf = float(((long double)delta.count()) / time_den);
	static float render_delta { 0.0f };
	
	// light handling (TODO: proper light)
	{
		static const float3 light_min { -105.0f, 250.0f, 0.0f }, light_max { 105.0f, 250.0f, 0.0f };
		//static const float3 light_min { -115.0f, 300.0f, 0.0f }, light_max { 115.0f, 0.0f, 0.0f };
		static float light_interp { 0.5f }, light_interp_dir { 1.0f };
		static constexpr const float light_slow_down { 0.0f };
		
		light_interp += (deltaf * light_slow_down) * light_interp_dir;
		if(light_interp > 1.0f) {
			light_interp = 1.0f;
			light_interp_dir *= -1.0f;
		}
		else if(light_interp < 0.0f) {
			light_interp = 0.0f;
			light_interp_dir *= -1.0f;
		}
		light_pos = light_min.interpolated(light_max, light_interp);
	}
	
	static const float frame_limit { 1.0f / float(warp_state.render_frame_count) };
	static size_t warp_frame_num = 0;
	blit_frame = false;
	const bool run_warp_kernel = ((deltaf < frame_limit && !first_frame) || warp_state.is_frame_repeat);
	if(run_warp_kernel) {
		if(deltaf >= frame_limit) { // need to reset when over the limit
			render_delta = deltaf;
			time_keeper = now;
			warp_frame_num = 0;
		}
		
		if(warp_state.is_warping) {
			render_kernels(deltaf, render_delta, warp_frame_num);
			blit_frame = false;
			++warp_frame_num;
		}
		else blit_frame = true;
	}
	
	//
	if(!run_warp_kernel) {
		first_frame = false;
		render_delta = deltaf;
		time_keeper = now;
		warp_frame_num = 0;
		
		render_full_scene(model, cam);
		
		blit_frame = warp_state.is_render_full;
	}
}

void common_renderer::render_kernels(const float& delta, const float& render_delta, const size_t& warp_frame_num) {
//#define WARP_TIMING
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	const auto timing_start = floor_timer::start();
#endif
	
	//
	bool is_first_frame = (warp_frame_num == 0);
	float relative_delta = delta / render_delta;
	if(warp_state.target_frame_count > 0) {
		const uint32_t in_between_frame_count = ((warp_state.target_frame_count - warp_state.render_frame_count) /
												 warp_state.render_frame_count);
		const float delta_per_frame = 1.0f / float(in_between_frame_count + 1u);
		
		// reached the limit
		if(warp_frame_num >= in_between_frame_count) {
			return;
		}
		
		relative_delta = float(warp_frame_num + 1u) * delta_per_frame;
	}
	
	// slow fixed delta for debugging/demo purposes
	static float dbg_delta = 0.0f;
	static constexpr const float delta_eps = 0.00025f;
	if(warp_state.is_debug_delta) {
		if(dbg_delta >= (1.0f - delta_eps)) {
			dbg_delta = delta_eps;
			is_first_frame = true;
		}
		else {
			is_first_frame = (dbg_delta == 0.0f);
			dbg_delta += delta_eps;
		}
		
		relative_delta = dbg_delta;
	}
	
	const libwarp_camera_setup cam_setup {
		.screen_width = floor::get_physical_width(),
		.screen_height = floor::get_physical_height(),
		.field_of_view = warp_state.fov,
		.near_plane = warp_state.near_far_plane.x,
		.far_plane = warp_state.near_far_plane.y,
		.depth_type = LIBWARP_DEPTH_NORMALIZED
	};
	LIBWARP_ERROR_CODE err = LIBWARP_SUCCESS;
	if(warp_state.is_scatter) {
		err = libwarp_scatter_floor(&cam_setup, relative_delta, is_first_frame && warp_state.is_clear_frame,
									scene_fbo.color[0],
									scene_fbo.depth[0],
									scene_fbo.motion[0],
									scene_fbo.compute_color);
	}
	else {
		if(warp_state.is_gather_forward) {
			err = libwarp_gather_forward_only_floor(&cam_setup, relative_delta,
													scene_fbo.color[0],
													scene_fbo.motion[0],
													scene_fbo.compute_color);
		}
		else {
			err = libwarp_gather_floor(&cam_setup, relative_delta,
									   scene_fbo.color[1u - warp_state.cur_fbo],
									   scene_fbo.depth[1u - warp_state.cur_fbo],
									   scene_fbo.color[warp_state.cur_fbo],
									   scene_fbo.depth[warp_state.cur_fbo],
									   scene_fbo.motion[warp_state.cur_fbo * 2],
									   scene_fbo.motion[(1u - warp_state.cur_fbo) * 2 + 1],
									   scene_fbo.motion_depth[warp_state.cur_fbo],
									   scene_fbo.motion_depth[1u - warp_state.cur_fbo],
									   scene_fbo.compute_color);
		}
	}
	if(err != LIBWARP_SUCCESS) log_error("libwarp error: %u", err);
	
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	log_debug("warp timing: %f", double(floor_timer::stop<chrono::microseconds>(timing_start)) / 1000.0);
#endif
}

void common_renderer::render_full_scene(const floor_obj_model&, const camera& cam) {
	// update shadow map uniforms
	{
		const matrix4f light_pm = clip * matrix4f().perspective(120.0f, 1.0f,
																warp_state.shadow_near_far_plane.x,
																warp_state.shadow_near_far_plane.y);
		const matrix4f light_mvm {
			matrix4f::translation(-light_pos) *
			matrix4f::rotation_deg_named<'x'>(90.0f) // rotate downwards
		};
		light_mvpm = light_mvm * light_pm;
		light_bias_mvpm = {
			light_mvpm * matrix4f().scale(0.5f, 0.5f, 0.5f).translate(0.5f, 0.5f, 0.5f)
		};
	}
	
	// update scene uniforms
	{
		pm = clip * matrix4f::perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
										  warp_state.near_far_plane.x, warp_state.near_far_plane.y);
		rmvm = (matrix4f::rotation_deg_named<'y'>(cam.get_rotation().y) *
				matrix4f::rotation_deg_named<'x'>(cam.get_rotation().x));
		mvm = matrix4f::translation(cam.get_position() * float3 { 1.0f, -1.0f, 1.0f }) * rmvm;
		const matrix4f mvpm {
			warp_state.is_scatter ?
			mvm * pm :
			prev_mvm * pm
		};
		const matrix4f next_mvpm { mvm * pm }; // gather
		const matrix4f prev_mvpm { prev_prev_mvm * pm }; // gather
		
		scene_uniforms = {
			.mvpm = mvpm,
			.light_bias_mvpm = light_bias_mvpm,
			.cam_pos = cam.get_position() * float3 { -1.0f, 1.0f, -1.0f },
			.light_pos = light_pos,
			.m0 = (warp_state.is_scatter ? mvm : next_mvpm),
			.m1 = (warp_state.is_scatter ? prev_mvm : prev_mvpm),
		};
	}
	
	// update skybox uniforms
	{
		const matrix4f imvpm {
			warp_state.is_scatter ?
			(rmvm * pm).inverted() :
			(prev_rmvm * pm).inverted()
		};
		const auto prev_imvpm = (prev_rmvm * pm).inverted(); // scatter
		const matrix4f next_mvpm { rmvm * pm }; // gather
		const matrix4f prev_mvpm { prev_prev_rmvm * pm }; // gather
		
		skybox_uniforms = {
			.imvpm = imvpm,
			.m0 = (warp_state.is_scatter ? prev_imvpm : next_mvpm),
			.m1 = (warp_state.is_scatter ? matrix4f() : prev_mvpm),
		};
	}
	
	// update constant buffers
	light_mvpm_buffer->write(*warp_state.dev_queue, &light_mvpm);
	scene_uniforms_buffer->write(*warp_state.dev_queue, &scene_uniforms);
	skybox_uniforms_buffer->write(*warp_state.dev_queue, &skybox_uniforms);
	
	// end
	if(!warp_state.is_scatter && !warp_state.is_gather_forward) {
		// flip state
		warp_state.cur_fbo = 1 - warp_state.cur_fbo;
	}
	
	// remember t-2 and t-1 mvms
	prev_prev_mvm = prev_mvm;
	prev_prev_rmvm = prev_rmvm;
	prev_mvm = mvm;
	prev_rmvm = rmvm;
}

bool common_renderer::compile_shaders(const string add_cli_options) {
	shader_prog = warp_state.ctx->add_program_file(floor::data_path("../warp/src/warp_shaders.cpp"),
												   "-I" + floor::data_path("../warp/src") +
												   " -DWARP_NEAR_PLANE=" + to_string(warp_state.near_far_plane.x) + "f" +
												   " -DWARP_FAR_PLANE=" + to_string(warp_state.near_far_plane.y) + "f" +
												   " -DWARP_SHADOW_NEAR_PLANE=" + to_string(warp_state.shadow_near_far_plane.x) + "f" +
												   " -DWARP_SHADOW_FAR_PLANE=" + to_string(warp_state.shadow_near_far_plane.y) + "f" +
												   add_cli_options);
	
	if(shader_prog == nullptr) {
		log_error("shader compilation failed");
		return false;
	}
	
	// NOTE: corresponds to WARP_SHADER
	static const char* shader_names[warp_shader_count()] {
		"scene_scatter_vs",
		"scene_scatter_fs",
		"scene_gather_vs",
		"scene_gather_fs",
		"scene_gather_vs",
		"scene_gather_fwd_fs",
		"skybox_scatter_vs",
		"skybox_scatter_fs",
		"skybox_gather_vs",
		"skybox_gather_fs",
		"skybox_gather_vs",
		"skybox_gather_fwd_fs",
		"shadow_vs", // NOTE: there is no shadow_fs
		"blit_vs",
		"blit_fs",
		"blit_vs",
		"blit_swizzle_fs",
	};
	
	for(size_t i = 0; i < warp_shader_count(); ++i) {
		shaders[i] = (compute_kernel*)shader_prog->get_kernel(shader_names[i]).get();
		if(shaders[i] == nullptr) {
			log_error("failed to retrieve shader \"%s\" from program", shader_names[i]);
			//return false;
			continue;
		}
		shader_entries[i] = (const compute_kernel::kernel_entry*)shaders[i]->get_kernel_entry(*warp_state.dev);
		if(shader_entries[i] == nullptr) {
			log_error("failed to retrieve shader entry \"%s\" for the current device", shader_names[i]);
			//return false;
			continue;
		}
	}
	
	return true;
}
