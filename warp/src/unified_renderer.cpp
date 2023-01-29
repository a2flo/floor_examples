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

#include "unified_renderer.hpp"

#include <floor/core/timer.hpp>
#include <floor/core/file_io.hpp>
#include <floor/graphics/graphics_renderer.hpp>
#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/graphics/graphics_pass.hpp>
#include <floor/compute/device/common.hpp>
#include <libwarp/libwarp.h>

static atomic<uint32_t> active_render { 0u };

unified_renderer::unified_renderer() :
resize_handler_fnctr(bind(&unified_renderer::resize_handler, this, placeholders::_1, placeholders::_2)) {
	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
}

unified_renderer::~unified_renderer() {
	floor::get_event()->remove_event_handler(resize_handler_fnctr);
	while (active_render > 0) {
		this_thread::sleep_for(50ms);
	}
}

void unified_renderer::create_textures(const COMPUTE_IMAGE_TYPE color_format) {
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
																 COMPUTE_IMAGE_TYPE::READ |
																 COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
																 COMPUTE_MEMORY_FLAG::READ_WRITE);
	}
	
	scene_fbo.compute_color = warp_state.ctx->create_image(*warp_state.dev_queue, scene_fbo.dim,
														   color_format |
														   COMPUTE_IMAGE_TYPE::IMAGE_2D |
														   COMPUTE_IMAGE_TYPE::READ_WRITE,
														   COMPUTE_MEMORY_FLAG::READ_WRITE);
	
	// create appropriately sized s/w depth buffer
	warp_state.scatter_depth_buffer = warp_state.ctx->create_buffer(*warp_state.dev_queue, sizeof(float) *
																	size_t(scene_fbo.dim.x) * size_t(scene_fbo.dim.y));
	
	// TODO: update passes
}

void unified_renderer::destroy_textures(bool is_resize) {
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

void unified_renderer::create_skybox() {
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

void unified_renderer::destroy_skybox() {
	skybox_tex = nullptr;
}

bool unified_renderer::resize_handler(EVENT_TYPE type, shared_ptr<event_object>) {
	if(type == EVENT_TYPE::WINDOW_RESIZE) {
		destroy_textures(true);
		create_textures(warp_state.ctx->get_renderer_image_type());
		first_frame = true;
		return true;
	}
	return false;
}

bool unified_renderer::rebuild_renderer() {
	if (!compile_shaders("")) {
		return false;
	}
	
	create_passes();
	create_pipelines();
	
	return true;
}

bool unified_renderer::init() {
	create_textures(warp_state.ctx->get_renderer_image_type());
	
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
	
	if (!rebuild_renderer()) {
		return false;
	}
	
	return true;
}

void unified_renderer::destroy() {
	destroy_textures(false);
	destroy_skybox();
}

void unified_renderer::create_passes() {
	const auto color_format = scene_fbo.color[0]->get_image_type();
	const auto motion_format = scene_fbo.motion[0]->get_image_type();
	const auto depth_format = scene_fbo.depth[0]->get_image_type();
	const auto motion_depth_format = scene_fbo.motion_depth[0]->get_image_type();
	
	const auto change_load_ops_to_load = [](auto& desc) {
		for (auto& att : desc.attachments) {
			att.load_op = LOAD_OP::LOAD;
		}
	};
	
	{
		render_pass_description pass_desc {
			.attachments = {
				// color
				{
					.format = color_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = clear_color,
				},
				// motion
				{
					.format = motion_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
				},
				// depth
				{
					.format = depth_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.depth = 1.0f,
				},
			}
		};
		scatter_passes[0] = warp_state.ctx->create_graphics_pass(pass_desc);
		change_load_ops_to_load(pass_desc);
		scatter_passes[1] = warp_state.ctx->create_graphics_pass(pass_desc);
	}
	
	{
		render_pass_description pass_desc {
			.attachments = {
				// color
				{
					.format = color_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = clear_color,
				},
				// motion fwd
				{
					.format = motion_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
				},
				// motion bwd
				{
					.format = motion_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
				},
				// motion depth
				{
					.format = motion_depth_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
				},
				// depth
				{
					.format = depth_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.depth = 1.0f,
				},
			}
		};
		gather_passes[0] = warp_state.ctx->create_graphics_pass(pass_desc);
		change_load_ops_to_load(pass_desc);
		gather_passes[1] = warp_state.ctx->create_graphics_pass(pass_desc);
	}
	
	{
		render_pass_description pass_desc {
			.attachments = {
				// color
				{
					.format = color_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = clear_color,
				},
				// motion
				{
					.format = motion_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
				},
				// depth
				{
					.format = depth_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.depth = 1.0f,
				},
			}
		};
		gather_fwd_passes[0] = warp_state.ctx->create_graphics_pass(pass_desc);
		change_load_ops_to_load(pass_desc);
		gather_fwd_passes[1] = warp_state.ctx->create_graphics_pass(pass_desc);
	}
	
	{
		const render_pass_description pass_desc {
			.attachments = {
				// depth
				{
					.format = depth_format,
					.load_op = LOAD_OP::CLEAR,
					.store_op = STORE_OP::STORE,
					.clear.depth = 1.0f,
				},
			}
		};
		shadow_pass = warp_state.ctx->create_graphics_pass(pass_desc);
	}
	
	{
		const render_pass_description pass_desc {
			.attachments = {
				// color
				{
					.format = color_format,
					.load_op = LOAD_OP::DONT_CARE, // don't care b/c drawing the whole screen
					.store_op = STORE_OP::STORE,
					.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
				},
			}
		};
		blit_pass = warp_state.ctx->create_graphics_pass(pass_desc);
	}
}

void unified_renderer::create_pipelines() {
	const auto color_format = scene_fbo.color[0]->get_image_type();
	const auto motion_format = scene_fbo.motion[0]->get_image_type();
	const auto depth_format = scene_fbo.depth[0]->get_image_type();
	const auto motion_depth_format = scene_fbo.motion_depth[0]->get_image_type();
	
	{
		render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[warp_state.dev->tessellation_support ? SCENE_SCATTER_TES : SCENE_SCATTER_VS],
			.fragment_shader = shaders[SCENE_SCATTER_FS],
			.primitive = PRIMITIVE::TRIANGLE,
			.cull_mode = CULL_MODE::BACK,
			.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
			.depth = {
				.write = true,
				.compare = DEPTH_COMPARE::LESS,
			},
			.color_attachments = {
				{ .format = color_format, },
				{ .format = motion_format, },
			},
			.depth_attachment = { .format = depth_format },
		};
		if (warp_state.dev->tessellation_support) {
			pipeline_desc.tessellation = {
				.max_factor = warp_state.dev->max_tessellation_factor,
				.vertex_attributes = {
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::FLOAT2,
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::UINT1,
				},
				.spacing = TESSELLATION_SPACING::FRACTIONAL_EVEN,
				.winding = TESSELLATION_WINDING::COUNTER_CLOCKWISE,
				.is_indexed_draw = true,
			};
		}
		scatter_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
		
		auto skybox_desc = pipeline_desc;
		skybox_desc.vertex_shader = shaders[SKYBOX_SCATTER_VS];
		skybox_desc.fragment_shader = shaders[SKYBOX_SCATTER_FS];
		skybox_desc.depth.compare = DEPTH_COMPARE::LESS_OR_EQUAL;
		skybox_scatter_pipeline = warp_state.ctx->create_graphics_pipeline(skybox_desc);
	}
	{
		render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[warp_state.dev->tessellation_support ? SCENE_GATHER_TES : SCENE_GATHER_VS],
			.fragment_shader = shaders[SCENE_GATHER_FS],
			.primitive = PRIMITIVE::TRIANGLE,
			.cull_mode = CULL_MODE::BACK,
			.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
			.depth = {
				.write = true,
				.compare = DEPTH_COMPARE::LESS,
			},
			.color_attachments = {
				{ .format = color_format, },
				{ .format = motion_format, },
				{ .format = motion_format, },
				{ .format = motion_depth_format, },
			},
			.depth_attachment = { .format = depth_format },
		};
		if (warp_state.dev->tessellation_support) {
			pipeline_desc.tessellation = {
				.max_factor = warp_state.dev->max_tessellation_factor,
				.vertex_attributes = {
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::FLOAT2,
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::FLOAT3,
					VERTEX_FORMAT::UINT1,
				},
				.spacing = TESSELLATION_SPACING::FRACTIONAL_EVEN,
				.winding = TESSELLATION_WINDING::COUNTER_CLOCKWISE,
				.is_indexed_draw = true,
			};
		}
		gather_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
		
		auto skybox_desc = pipeline_desc;
		skybox_desc.vertex_shader = shaders[SKYBOX_GATHER_VS];
		skybox_desc.fragment_shader = shaders[SKYBOX_GATHER_FS];
		skybox_desc.depth.compare = DEPTH_COMPARE::LESS_OR_EQUAL;
		skybox_desc.tessellation = render_pipeline_description::tessellation_t {}; // reset tessellation
		skybox_gather_pipeline = warp_state.ctx->create_graphics_pipeline(skybox_desc);
	}
	{
		const render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[SCENE_GATHER_FWD_VS],
			.fragment_shader = shaders[SCENE_GATHER_FWD_FS],
			.primitive = PRIMITIVE::TRIANGLE,
			.cull_mode = CULL_MODE::BACK,
			.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
			.depth = {
				.write = true,
				.compare = DEPTH_COMPARE::LESS,
			},
			.color_attachments = {
				{ .format = color_format, },
				{ .format = motion_format, },
			},
			.depth_attachment = { .format = depth_format },
		};
		gather_fwd_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
		
		auto skybox_desc = pipeline_desc;
		skybox_desc.vertex_shader = shaders[SKYBOX_GATHER_FWD_VS];
		skybox_desc.fragment_shader = shaders[SKYBOX_GATHER_FWD_FS];
		skybox_desc.depth.compare = DEPTH_COMPARE::LESS_OR_EQUAL;
		skybox_gather_fwd_pipeline = warp_state.ctx->create_graphics_pipeline(skybox_desc);
	}
	
	{
		const render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[SHADOW_VS],
			.fragment_shader = nullptr, // only rendering z/depth
			.primitive = PRIMITIVE::TRIANGLE,
			.cull_mode = CULL_MODE::BACK,
			.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
			.viewport = shadow_map.dim,
			.depth = {
				.write = true,
				.compare = DEPTH_COMPARE::LESS,
			},
			.depth_attachment = { .format = depth_format },
		};
		shadow_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
	}
	
	{
		const render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[BLIT_VS],
			.fragment_shader = shaders[BLIT_FS],
			.primitive = PRIMITIVE::TRIANGLE,
			.cull_mode = CULL_MODE::BACK,
			.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
			.depth = {
				.write = false,
				.compare = DEPTH_COMPARE::ALWAYS,
			},
			.color_attachments = {
				{ .format = color_format, },
			},
		};
		blit_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
	}
}

void unified_renderer::render(const floor_obj_model& model, const camera& cam) {
	// time keeping
	static const long double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
	static auto time_keeper = chrono::high_resolution_clock::now();
	const auto now = chrono::high_resolution_clock::now();
	const auto delta = now - time_keeper;
	const auto deltaf = float(((long double)delta.count()) / time_den);
	static float render_delta { 0.0f };
	
	// light handling (TODO: proper light)
	{
		static const float3 light_min { -80.0f, 250.0f, 0.0f }, light_max { 80.0f, 250.0f, 0.0f };
		//static const float3 light_min { -115.0f, 300.0f, 0.0f }, light_max { 115.0f, 0.0f, 0.0f };
		static float light_interp { 0.5f }, light_interp_dir { 1.0f };
		static constexpr const float light_slow_down { 0.01f };
		
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
	
	// decide if we are going to perform warping or render the full scene
	static const float frame_limit { 1.0f / float(warp_state.render_frame_count) };
	static size_t warp_frame_num = 0;
	blit_frame = false;
	const bool run_warp_kernel = ((deltaf < frame_limit && !first_frame) || warp_state.is_frame_repeat);
	if (run_warp_kernel) {
		if (deltaf >= frame_limit) { // need to reset when over the limit
			render_delta = deltaf;
			time_keeper = now;
			warp_frame_num = 0;
		}
		
		if (warp_state.is_warping) {
			render_kernels(deltaf, render_delta, warp_frame_num);
			blit_frame = false;
			++warp_frame_num;
		} else {
			blit_frame = true;
		}
	} else {
		first_frame = false;
		render_delta = deltaf;
		time_keeper = now;
		warp_frame_num = 0;
		
		render_full_scene(model, cam);
		
		blit_frame = warp_state.is_render_full;
	}
	
	// blit to window
	{
		// create the blit renderer
		shared_ptr<graphics_renderer> renderer = warp_state.ctx->create_graphics_renderer(*warp_state.dev_queue, *blit_pass, *blit_pipeline);
		if (!renderer->is_valid()) {
			return;
		}
		
		// setup drawable and attachments
		auto drawable = renderer->get_next_drawable();
		if (drawable == nullptr || !drawable->is_valid()) {
			log_error("failed to get next drawable");
			return;
		}
		// prevent whole renderer destruction by signaling that we're rendering a frame
		++active_render;
		renderer->set_attachments(drawable);
		renderer->begin();
		
		static const vector<graphics_renderer::multi_draw_entry> blit_draw_info {{
			.vertex_count = 3 // fullscreen triangle
		}};
		renderer->multi_draw(blit_draw_info,
							 // NOTE: for gather: 1 - cur_fbo, because this is flipped in render_full_scene (-> this is the current FBO)
							 blit_frame ? scene_fbo.color[warp_state.is_scatter || warp_state.is_gather_forward ?
														  0u : 1u - warp_state.cur_fbo] : scene_fbo.compute_color);
		
		renderer->end();
		renderer->present();
		renderer->commit([retained_renderer = move(renderer)] {
			// must retain renderer object until completion -> auto-destruct via shared_ptr
			
			// signal that this frame is done -> may destruct the whole renderer (if 0)
			--active_render;
		});
	}
}

void unified_renderer::render_kernels(const float& delta, const float& render_delta, const size_t& warp_frame_num) {
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
	static constexpr const float delta_eps = 0.0025f;
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
	if(err != LIBWARP_SUCCESS) log_error("libwarp error: $", err);
	
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	log_debug("warp timing: $", double(floor_timer::stop<chrono::microseconds>(timing_start)) / 1000.0);
#endif
}

void unified_renderer::render_full_scene(const floor_obj_model& model, const camera& cam) {
	//////////////////////////////////////////
	// update uniforms
	
	// update shadow map uniforms
	{
		const matrix4f light_pm = matrix4f::perspective(110.0f, 1.0f,
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
		pm = matrix4f::perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
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
	
	// our draw info is static, so only create it once
	static const vector<graphics_renderer::multi_draw_indexed_entry> scene_draw_info {{
		.index_buffer = model.indices_buffer.get(),
		.index_count = model.index_count
	}};
	static const graphics_renderer::patch_draw_indexed_entry scene_draw_patch_info {
		.control_point_buffers = {
			model.vertices_buffer.get(),
			model.tex_coords_buffer.get(),
			model.normals_buffer.get(),
			model.binormals_buffer.get(),
			model.tangents_buffer.get(),
			model.materials_data_buffer.get(),
		},
		.control_point_index_buffer = model.indices_buffer.get(),
		.patch_control_point_count = 3u,
		.first_index = 0u,
		.patch_count = model.index_count / 3u,
		.first_patch = 0u,
	};
	static const vector<graphics_renderer::multi_draw_entry> skybox_draw_info {{
		.vertex_count = 3
	}};
	
	//////////////////////////////////////////
	// update tessellation factors
	{
		if (!tess_factors_buffer) {
			const auto tess_factors_size = sizeof(triangle_tessellation_levels_t) * (model.index_count / 3u);
			tess_factors_buffer = warp_state.ctx->create_buffer(*warp_state.dev_queue, tess_factors_size);
		}
		warp_state.dev_queue->execute(*shaders[TESS_UPDATE_FACTORS],
									  uint1 { model.index_count / 3u }, uint1 { 1024u },
									  model.vertices_buffer,
									  model.indices_buffer,
									  model.materials_data_buffer,
									  tess_factors_buffer,
									  model.index_count / 3u,
									  scene_uniforms.cam_pos,
									  float(gather_pipeline->get_description(false).tessellation.max_factor));
	}
	
	//////////////////////////////////////////
	// draw shadow map
	{
		auto renderer = warp_state.ctx->create_graphics_renderer(*warp_state.dev_queue, *shadow_pass, *shadow_pipeline);
		if (!renderer->is_valid()) {
			return;
		}
		
		renderer->set_attachments(shadow_map.shadow_image);
		renderer->begin();
		
		renderer->multi_draw_indexed(scene_draw_info, model.vertices_buffer, light_mvpm);
		
		renderer->end();
		renderer->commit();
	}
	
	//////////////////////////////////////////
	// render actual scene
	{
		auto renderer = warp_state.ctx->create_graphics_renderer(*warp_state.dev_queue,
																 (warp_state.is_scatter ? *scatter_passes[0] :
																  (warp_state.is_gather_forward ? *gather_fwd_passes[0] : *gather_passes[0])),
																 (warp_state.is_scatter ? *scatter_pipeline :
																  (warp_state.is_gather_forward ? *gather_fwd_pipeline : *gather_pipeline)));
		if (!renderer->is_valid()) {
			return;
		}
		
		if (warp_state.is_scatter || warp_state.is_gather_forward) {
			renderer->set_attachments(scene_fbo.color[0], scene_fbo.motion[0], scene_fbo.depth[0]);
		} else { // gather
			// for bidirectional gather rendering, this switches every frame
			renderer->set_attachments(scene_fbo.color[warp_state.cur_fbo],
									  scene_fbo.motion[warp_state.cur_fbo * 2],
									  scene_fbo.motion[warp_state.cur_fbo * 2 + 1],
									  scene_fbo.motion_depth[warp_state.cur_fbo],
									  scene_fbo.depth[warp_state.cur_fbo]);
		}
		
		// scene
		renderer->begin();
		if (warp_state.dev->tessellation_support) {
			renderer->set_tessellation_factors(*tess_factors_buffer);
			if (!warp_state.use_material_argument_buffer) {
				renderer->draw_patches_indexed(scene_draw_patch_info,
											   // vertex shader
											   model.displacement_textures,
											   warp_state.displacement_mode,
											   scene_uniforms,
											   // fragment shader
											   warp_state.displacement_mode,
											   model.diffuse_textures,
											   model.specular_textures,
											   model.normal_textures,
											   model.mask_textures,
											   model.displacement_textures,
											   shadow_map.shadow_image);
			} else {
				renderer->draw_patches_indexed(scene_draw_patch_info,
											   // vertex shader
											   model.displacement_textures,
											   warp_state.displacement_mode,
											   scene_uniforms,
											   // fragment shader
											   warp_state.displacement_mode,
											   model.materials_arg_buffer,
											   shadow_map.shadow_image);
			}
		} else {
			if (!warp_state.use_material_argument_buffer) {
				renderer->multi_draw_indexed(scene_draw_info,
											 // vertex shader
											 model.vertices_buffer,
											 model.tex_coords_buffer,
											 model.normals_buffer,
											 model.binormals_buffer,
											 model.tangents_buffer,
											 model.materials_data_buffer,
											 scene_uniforms,
											 // fragment shader
											 warp_state.displacement_mode,
											 model.diffuse_textures,
											 model.specular_textures,
											 model.normal_textures,
											 model.mask_textures,
											 model.displacement_textures,
											 shadow_map.shadow_image);
			} else {
				renderer->multi_draw_indexed(scene_draw_info,
											 // vertex shader
											 model.vertices_buffer,
											 model.tex_coords_buffer,
											 model.normals_buffer,
											 model.binormals_buffer,
											 model.tangents_buffer,
											 model.materials_data_buffer,
											 scene_uniforms,
											 // fragment shader
											 warp_state.displacement_mode,
											 model.materials_arg_buffer,
											 shadow_map.shadow_image);
			}
		}
		renderer->end();
		renderer->commit();
	}
	
	//////////////////////////////////////////
	// render sky box
	{
		auto renderer = warp_state.ctx->create_graphics_renderer(*warp_state.dev_queue,
																 (warp_state.is_scatter ? *scatter_passes[1] : (warp_state.is_gather_forward ? *gather_fwd_passes[1] : *gather_passes[1])),
																 (warp_state.is_scatter ? *skybox_scatter_pipeline : (warp_state.is_gather_forward ? *skybox_gather_fwd_pipeline : *skybox_gather_pipeline)));
		if (!renderer->is_valid()) {
			return;
		}
		
		if (warp_state.is_scatter || warp_state.is_gather_forward) {
			renderer->set_attachments(scene_fbo.color[0], scene_fbo.motion[0], scene_fbo.depth[0]);
		} else { // gather
			// for bidirectional gather rendering, this switches every frame
			renderer->set_attachments(scene_fbo.color[warp_state.cur_fbo],
									  scene_fbo.motion[warp_state.cur_fbo * 2],
									  scene_fbo.motion[warp_state.cur_fbo * 2 + 1],
									  scene_fbo.motion_depth[warp_state.cur_fbo],
									  scene_fbo.depth[warp_state.cur_fbo]);
		}
		
		// skybox
		renderer->begin();
		renderer->multi_draw(skybox_draw_info, skybox_uniforms, skybox_tex);
		renderer->end();
		renderer->commit();
	}
	
	//////////////////////////////////////////
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

bool unified_renderer::compile_shaders(const string add_cli_options) {
	shared_ptr<compute_program> new_shader_prog;
	array<compute_kernel*, size_t(WARP_SHADER::__MAX_WARP_SHADER)> new_shaders;
	array<const compute_kernel::kernel_entry*, size_t(WARP_SHADER::__MAX_WARP_SHADER)> new_shader_entries;
	
	new_shader_prog = warp_state.ctx->add_program_file(floor::data_path("../warp/src/warp_shaders.cpp"),
													   "-I" + floor::data_path("../warp/src") +
													   " -DWARP_NEAR_PLANE=" + to_string(warp_state.near_far_plane.x) + "f" +
													   " -DWARP_FAR_PLANE=" + to_string(warp_state.near_far_plane.y) + "f" +
													   " -DWARP_SHADOW_FAR_PLANE=" + to_string(warp_state.shadow_near_far_plane.y) + "f" +
													   (floor::get_wide_gamut() && floor::get_hdr() ? "" : " -DWARP_APPLY_GAMMA") +
													   (warp_state.use_material_argument_buffer ? " -DWARP_USE_MATERIAL_ARGUMENT_BUFFER=1" : "")+
													   add_cli_options);
	
	if (new_shader_prog == nullptr) {
		log_error("shader compilation failed");
		return false;
	}
	
	// NOTE: corresponds to WARP_SHADER
	static const char* shader_names[warp_shader_count()] {
		"scene_scatter_vs",
		(warp_state.dev->tessellation_support ? "scene_scatter_tes" : "scene_scatter_vs"),
		"scene_scatter_fs",
		"scene_gather_vs",
		(warp_state.dev->tessellation_support ? "scene_gather_tes" : "scene_gather_vs"),
		"scene_gather_fs",
		"scene_gather_vs",
		(warp_state.dev->tessellation_support ? "scene_gather_tes" : "scene_gather_vs"),
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
		"tess_update_factors",
	};
	
	for (size_t i = 0; i < warp_shader_count(); ++i) {
		new_shaders[i] = (compute_kernel*)new_shader_prog->get_kernel(shader_names[i]).get();
		if (new_shaders[i] == nullptr) {
			log_error("failed to retrieve shader \"$\" from program", shader_names[i]);
			return false;
		}
		new_shader_entries[i] = (const compute_kernel::kernel_entry*)new_shaders[i]->get_kernel_entry(*warp_state.dev);
		if (new_shader_entries[i] == nullptr) {
			log_error("failed to retrieve shader entry \"$\" for the current device", shader_names[i]);
			return false;
		}
	}
	
	// successful
	shader_prog = new_shader_prog;
	shaders = new_shaders;
	shader_entries = new_shader_entries;
	return true;
}

void unified_renderer::init_model_materials_arg_buffer(const compute_queue& dev_queue, floor_obj_model& model) {
	// NOTE: scene_scatter_fs/scene_gather_fs/scene_gather_fwd_fs have the same function signature -> doesn't matter which one we take
	model.materials_arg_buffer = shaders[WARP_SHADER::SCENE_GATHER_FS]->create_argument_buffer(dev_queue, 2);
	if (!model.materials_arg_buffer) {
		throw runtime_error("failed to create materials argument buffer");
	}
	model.materials_arg_buffer->set_arguments(dev_queue,
											  model.diffuse_textures,
											  model.specular_textures,
											  model.normal_textures,
											  model.mask_textures,
											  model.displacement_textures);
}
