/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
#include <condition_variable>

#if 0
#define warp_log_wip(...) log_warn(__VA_ARGS__)
#else
#define warp_log_wip(...)
#endif

//! render thread implementation:
//! this performs the rendering and screen-blitting/present of a single frame
class render_thread final : public thread_base {
public:
	render_thread(const string name, unified_renderer& renderer_, const uint32_t frame_idx_) :
	thread_base(name), renderer(renderer_), frame_idx(frame_idx_) {
		// never sleep or yield, will wait on "render_cv" in run()
		set_thread_delay(0u);
		set_yield_after_run(false);
	}
	~render_thread() override {
		// nop
	}
	
	void run() override;
	
	void render();
	
	void start() {
		thread_base::start();
	}
	
protected:
	//! ref to the renderer
	unified_renderer& renderer;
	//! associated frame index
	const uint32_t frame_idx { ~0u };
	//! required lock for "render_cv"
	mutex render_cv_lock;
	//! will be signaled once the frame should be rendered
	condition_variable render_cv;
};

unified_renderer::unified_renderer() :
resize_handler_fnctr(bind(&unified_renderer::resize_handler, this, placeholders::_1, placeholders::_2)) {
	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
}

unified_renderer::~unified_renderer() {
	floor::get_event()->remove_event_handler(resize_handler_fnctr);
	
	// signal renderer halt and shut down all render threads
	halt_renderer = true;
	for (auto& frame : frame_objects) {
		frame.rthread->finish();
	}
	
	destroy_frame_buffer_images(false);
	destroy_skybox();
}

void unified_renderer::create_frame_buffer_images(const COMPUTE_IMAGE_TYPE color_format) {
	const auto frame_dim = floor::get_physical_screen_size();
	// kernel work-group size is { 32, 16 } -> round global size to a multiple of it
	const auto frame_dim_multiple = frame_dim.rounded_next_multiple(warp_state.tile_size);
	
	uint32_t frame_counter = 0;
	for (auto& frame : frame_objects) {
		frame.scene_fbo.dim = frame_dim;
		frame.scene_fbo.dim_multiple = frame_dim_multiple;
		
		const auto frame_str = to_string(frame_counter++);
		
		frame.scene_fbo.color = warp_state.ctx->create_image(*frame.dev_queue, frame.scene_fbo.dim,
															 color_format |
															 COMPUTE_IMAGE_TYPE::IMAGE_2D |
															 COMPUTE_IMAGE_TYPE::READ_WRITE |
															 COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
															 COMPUTE_MEMORY_FLAG::READ_WRITE);
		frame.scene_fbo.color->set_debug_label("scene_color#" + frame_str);
		
		frame.scene_fbo.depth = warp_state.ctx->create_image(*frame.dev_queue, frame.scene_fbo.dim,
															 COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
															 COMPUTE_IMAGE_TYPE::D32F |
															 COMPUTE_IMAGE_TYPE::READ |
															 COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
															 COMPUTE_MEMORY_FLAG::READ);
		frame.scene_fbo.depth->set_debug_label("scene_depth#" + frame_str);
		
		for (size_t j = 0; j < 2; ++j) {
			frame.scene_fbo.motion[j] = warp_state.ctx->create_image(*frame.dev_queue, frame.scene_fbo.dim,
																	 COMPUTE_IMAGE_TYPE::IMAGE_2D |
																	 COMPUTE_IMAGE_TYPE::R32UI |
																	 COMPUTE_IMAGE_TYPE::READ_WRITE |
																	 COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
																	 COMPUTE_MEMORY_FLAG::READ_WRITE);
		}
		frame.scene_fbo.motion[0]->set_debug_label("scene_motion_0#" + frame_str);
		frame.scene_fbo.motion[1]->set_debug_label("scene_motion_1#" + frame_str);
		
		frame.scene_fbo.motion_depth = warp_state.ctx->create_image(*frame.dev_queue, frame.scene_fbo.dim,
																	COMPUTE_IMAGE_TYPE::IMAGE_2D |
																	COMPUTE_IMAGE_TYPE::RG16F |
																	COMPUTE_IMAGE_TYPE::READ |
																	COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
																	COMPUTE_MEMORY_FLAG::READ_WRITE);
		frame.scene_fbo.motion_depth->set_debug_label("scene_motion_depth#" + frame_str);
		
		frame.scene_fbo.compute_color = warp_state.ctx->create_image(*frame.dev_queue, frame.scene_fbo.dim,
																	 color_format |
																	 COMPUTE_IMAGE_TYPE::IMAGE_2D |
																	 COMPUTE_IMAGE_TYPE::READ_WRITE,
																	 COMPUTE_MEMORY_FLAG::READ_WRITE);
		frame.scene_fbo.compute_color->set_debug_label("scene_compute_color");
		
		if (!frame.shadow_map.shadow_image) {
			// shadow map (only created once)
			frame.shadow_map.shadow_image = warp_state.ctx->create_image(*frame.dev_queue, frame.shadow_map.dim,
																		 COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
																		 COMPUTE_IMAGE_TYPE::D32F |
																		 COMPUTE_IMAGE_TYPE::READ |
																		 COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
																		 COMPUTE_MEMORY_FLAG::READ);
			frame.shadow_map.shadow_image->set_debug_label("shadow_map");
		}
	}
}

void unified_renderer::destroy_frame_buffer_images(bool is_resize) {
	// TODO: need to properly sync this (wait until drawing is fully complete)
	// TODO: need to reencode pipelines!
	
	for (auto& frame : frame_objects) {
		frame.scene_fbo.compute_color = nullptr;
		frame.scene_fbo.color = nullptr;
		frame.scene_fbo.depth = nullptr;
		for (auto& img : frame.scene_fbo.motion) {
			img = nullptr;
		}
		frame.scene_fbo.motion_depth = nullptr;
		
		// only need to destroy this on exit (not on resize!)
		if (!is_resize) {
			frame.shadow_map.shadow_image = nullptr;
		}
	}
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
	
	skybox_tex = warp_state.ctx->create_image(*warp_state.main_dev_queue,
											  skybox_dim,
											  image_type,
											  skybox_pixels.get(),
											  COMPUTE_MEMORY_FLAG::READ |
											  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE /*|
											  COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS*/);
	skybox_tex->set_debug_label("skybox_tex");
}

void unified_renderer::destroy_skybox() {
	skybox_tex = nullptr;
}

bool unified_renderer::resize_handler(EVENT_TYPE type, shared_ptr<event_object>) {
	if (type == EVENT_TYPE::WINDOW_RESIZE) {
		destroy_frame_buffer_images(true);
		create_frame_buffer_images(warp_state.ctx->get_renderer_image_type());
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
	if (!create_pipelines()) {
		return false;
	}
	
	return true;
}

bool unified_renderer::init() {
	// init initial per-frame objects
	uint32_t frame_counter = 0;
	for (auto& frame : frame_objects) {
		const auto frame_idx_str = to_string(frame_counter);
		frame.dev_queue = warp_state.ctx->create_queue(*warp_state.dev);
		frame.rthread = make_unique<render_thread>("render_thrd#" + frame_idx_str, *this, frame_counter);
		frame.frame_uniforms_buffer = warp_state.ctx->create_buffer(*frame.dev_queue, sizeof(warp_shaders::frame_uniforms_t),
																	COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
		frame.frame_uniforms_buffer->set_debug_label("frame_uniforms#" + frame_idx_str);
		++frame_counter;
	}
	
	create_frame_buffer_images(warp_state.ctx->get_renderer_image_type());
	
	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
	
	// sky box
	create_skybox();
	
	if (!rebuild_renderer()) {
		return false;
	}
	
	// ensure we're all done
	for (auto& frame : frame_objects) {
		frame.dev_queue->finish();
	}
	warp_state.main_dev_queue->finish();
	
	// start render threads
	for (auto& frame : frame_objects) {
		frame.rthread->start();
	}
	
	return true;
}

void unified_renderer::create_passes() {
	const auto color_format = frame_objects[0].scene_fbo.color->get_image_type();
	const auto motion_format = frame_objects[0].scene_fbo.motion[0]->get_image_type();
	const auto depth_format = frame_objects[0].scene_fbo.depth->get_image_type();
	const auto motion_depth_format = frame_objects[0].scene_fbo.motion_depth->get_image_type();
	
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

bool unified_renderer::create_pipelines() {
	const auto color_format = frame_objects[0].scene_fbo.color->get_image_type();
	const auto motion_format = frame_objects[0].scene_fbo.motion[0]->get_image_type();
	const auto depth_format = frame_objects[0].scene_fbo.depth->get_image_type();
	const auto motion_depth_format = frame_objects[0].scene_fbo.motion_depth->get_image_type();
	
	{
		render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[warp_state.use_tessellation ? SCENE_SCATTER_TES : SCENE_SCATTER_VS],
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
			.support_indirect_rendering = warp_state.use_indirect_commands,
			.debug_label = "scatter_pipeline",
		};
		if (warp_state.use_tessellation) {
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
		if (!scatter_pipeline) {
			log_error("failed to create scatter_pipeline");
			return false;
		}
		
		auto skybox_desc = pipeline_desc;
		skybox_desc.vertex_shader = shaders[SKYBOX_SCATTER_VS];
		skybox_desc.fragment_shader = shaders[SKYBOX_SCATTER_FS];
		skybox_desc.depth.compare = DEPTH_COMPARE::LESS_OR_EQUAL;
		skybox_desc.debug_label = "skybox_scatter_pipeline";
		skybox_scatter_pipeline = warp_state.ctx->create_graphics_pipeline(skybox_desc);
		if (!skybox_scatter_pipeline) {
			log_error("failed to create skybox_scatter_pipeline");
			return false;
		}
	}
	{
		render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[warp_state.use_tessellation ? SCENE_GATHER_TES : SCENE_GATHER_VS],
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
			.support_indirect_rendering = warp_state.use_indirect_commands,
			.debug_label = "gather_pipeline",
		};
		if (warp_state.use_tessellation) {
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
		if (!gather_pipeline) {
			log_error("failed to create gather_pipeline");
			return false;
		}
		
		auto skybox_desc = pipeline_desc;
		skybox_desc.vertex_shader = shaders[SKYBOX_GATHER_VS];
		skybox_desc.fragment_shader = shaders[SKYBOX_GATHER_FS];
		skybox_desc.depth.compare = DEPTH_COMPARE::LESS_OR_EQUAL;
		skybox_desc.tessellation = render_pipeline_description::tessellation_t {}; // reset tessellation
		skybox_desc.debug_label = "skybox_gather_pipeline";
		skybox_gather_pipeline = warp_state.ctx->create_graphics_pipeline(skybox_desc);
		if (!skybox_gather_pipeline) {
			log_error("failed to create skybox_gather_pipeline");
			return false;
		}
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
			.support_indirect_rendering = warp_state.use_indirect_commands,
			.debug_label = "gather_fwd_pipeline",
		};
		gather_fwd_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
		if (!gather_fwd_pipeline) {
			log_error("failed to create gather_fwd_pipeline");
			return false;
		}
		
		auto skybox_desc = pipeline_desc;
		skybox_desc.vertex_shader = shaders[SKYBOX_GATHER_FWD_VS];
		skybox_desc.fragment_shader = shaders[SKYBOX_GATHER_FWD_FS];
		skybox_desc.depth.compare = DEPTH_COMPARE::LESS_OR_EQUAL;
		skybox_desc.debug_label = "skybox_gather_fwd_pipeline";
		skybox_gather_fwd_pipeline = warp_state.ctx->create_graphics_pipeline(skybox_desc);
		if (!skybox_gather_fwd_pipeline) {
			log_error("failed to create skybox_gather_fwd_pipeline");
			return false;
		}
	}
	
	{
		const render_pipeline_description pipeline_desc {
			.vertex_shader = shaders[SHADOW_VS],
			.fragment_shader = nullptr, // only rendering z/depth
			.primitive = PRIMITIVE::TRIANGLE,
			.cull_mode = CULL_MODE::BACK,
			.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
			.viewport = frame_objects[0].shadow_map.dim,
			.depth = {
				.write = true,
				.compare = DEPTH_COMPARE::LESS,
			},
			.depth_attachment = { .format = depth_format },
			.support_indirect_rendering = warp_state.use_indirect_commands,
			.debug_label = "shadow_pipeline",
		};
		shadow_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
		if (!shadow_pipeline) {
			log_error("failed to create shadow_pipeline");
			return false;
		}
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
			.debug_label = "blit_pipeline",
		};
		blit_pipeline = warp_state.ctx->create_graphics_pipeline(pipeline_desc);
		if (!blit_pipeline) {
			log_error("failed to create blit_pipeline");
			return false;
		}
	}
	
	// create fences
	for (auto& frame : frame_objects) {
		frame.render_post_tess_update_fence = warp_state.ctx->create_fence(*frame.dev_queue);
		frame.render_post_shadow_fence = warp_state.ctx->create_fence(*frame.dev_queue);
		frame.render_post_scene_fence = warp_state.ctx->create_fence(*frame.dev_queue);
		frame.render_post_skybox_fence = warp_state.ctx->create_fence(*frame.dev_queue);
		frame.render_post_blit_fence = warp_state.ctx->create_fence(*frame.dev_queue);
		if (!frame.render_post_tess_update_fence ||
			!frame.render_post_shadow_fence ||
			!frame.render_post_scene_fence ||
			!frame.render_post_skybox_fence ||
			!frame.render_post_blit_fence) {
			log_error("failed to create render fences");
			return false;
		}
	}
	
	// create indirect pipelines
	if (warp_state.use_indirect_commands) {
		uint32_t frame_counter = 0;
		for (auto& frame : frame_objects) {
			const auto frame_str = "#" + to_string(frame_counter++);
			
			{
				indirect_command_description desc {
					.command_type = indirect_command_description::COMMAND_TYPE::RENDER,
					.max_command_count = 1,
					.debug_label = "indirect_shadow_pipeline" + frame_str
				};
				desc.compute_buffer_counts_from_functions(*warp_state.dev, {
					shaders[SHADOW_VS]
				});
				frame.indirect_shadow_pipeline = warp_state.ctx->create_indirect_command_pipeline(desc);
				if (!frame.indirect_shadow_pipeline->is_valid()) {
					log_error("failed to create indirect_shadow_pipeline");
					return false;
				}
			}
			{
				indirect_command_description desc {
					.command_type = indirect_command_description::COMMAND_TYPE::RENDER,
					.max_command_count = 1,
					.debug_label = "indirect_scene_scatter_pipeline" + frame_str
				};
				vector<const compute_kernel*> functions {
					shaders[SCENE_SCATTER_VS], shaders[SCENE_SCATTER_FS],
					shaders[SCENE_GATHER_VS], shaders[SCENE_GATHER_FS],
					shaders[SCENE_GATHER_FWD_VS], shaders[SCENE_GATHER_FWD_FS],
				};
				if (warp_state.use_tessellation) {
					functions.emplace_back(shaders[SCENE_SCATTER_TES]);
					functions.emplace_back(shaders[SCENE_GATHER_TES]);
					functions.emplace_back(shaders[SCENE_GATHER_FWD_TES]);
				}
				desc.compute_buffer_counts_from_functions(*warp_state.dev, functions);
				
				frame.indirect_scene_scatter_pipeline = warp_state.ctx->create_indirect_command_pipeline(desc);
				if (!frame.indirect_scene_scatter_pipeline->is_valid()) {
					log_error("failed to create indirect_scene_scatter_pipeline");
					return false;
				}
				
				desc.debug_label = "indirect_scene_gather_pipeline" + frame_str;
				frame.indirect_scene_gather_pipeline = warp_state.ctx->create_indirect_command_pipeline(desc);
				if (!frame.indirect_scene_gather_pipeline->is_valid()) {
					log_error("failed to create indirect_scene_gather_pipeline");
					return false;
				}
			}
			{
				indirect_command_description desc {
					.command_type = indirect_command_description::COMMAND_TYPE::RENDER,
					.max_command_count = 1,
					.debug_label = "indirect_skybox_scatter_pipeline" + frame_str
				};
				desc.compute_buffer_counts_from_functions(*warp_state.dev, {
					shaders[SKYBOX_SCATTER_VS], shaders[SKYBOX_SCATTER_FS],
					shaders[SKYBOX_GATHER_VS], shaders[SKYBOX_GATHER_FS],
					shaders[SKYBOX_GATHER_FWD_VS], shaders[SKYBOX_GATHER_FWD_FS],
				});
				frame.indirect_skybox_scatter_pipeline = warp_state.ctx->create_indirect_command_pipeline(desc);
				if (!frame.indirect_skybox_scatter_pipeline->is_valid()) {
					log_error("failed to create indirect_skybox_scatter_pipeline");
					return false;
				}
				
				desc.debug_label = "indirect_skybox_gather_pipeline" + frame_str;
				frame.indirect_skybox_gather_pipeline = warp_state.ctx->create_indirect_command_pipeline(desc);
				if (!frame.indirect_skybox_gather_pipeline->is_valid()) {
					log_error("failed to create indirect_skybox_gather_pipeline");
					return false;
				}
			}
		}
	}
	
	return true;
}

void unified_renderer::encode_indirect_pipelines(const uint32_t frame_idx) {
	if (!warp_state.use_indirect_commands) {
		return;
	}
	auto& frame = frame_objects[frame_idx];
	if (!frame.indirect_shadow_pipeline ||
		!frame.indirect_scene_scatter_pipeline ||
		!frame.indirect_scene_gather_pipeline ||
		!frame.indirect_skybox_scatter_pipeline ||
		!frame.indirect_skybox_gather_pipeline) {
		log_error("indirect pipelines were not created");
		return;
	}
	
	frame.indirect_shadow_pipeline->reset();
	frame.indirect_scene_scatter_pipeline->reset();
	frame.indirect_scene_gather_pipeline->reset();
	frame.indirect_skybox_scatter_pipeline->reset();
	frame.indirect_skybox_gather_pipeline->reset();
	
	frame.indirect_shadow_pipeline->add_render_command(*frame.dev_queue, *shadow_pipeline)
		.set_arguments(model->vertices_buffer, frame.frame_uniforms_buffer)
		.draw_indexed(*model->indices_buffer, model->index_count);
	
	// NOTE: signatures are identical for scatter/gather-fwd/gather + we can only select between scatter and gather at run-time
	// -> only need one pipeline for gather
	const auto& scene_gather_pipeline = (warp_state.is_gather_forward ? *gather_fwd_pipeline : *gather_pipeline);
	if (warp_state.use_tessellation) {
		vector<const compute_buffer*> vbuffers {
			model->vertices_buffer.get(),
			model->tex_coords_buffer.get(),
			model->normals_buffer.get(),
			model->binormals_buffer.get(),
			model->tangents_buffer.get(),
			model->materials_data_buffer.get(),
		};
		frame.indirect_scene_scatter_pipeline->add_render_command(*frame.dev_queue, *scatter_pipeline)
			.set_arguments(// tes
						   model->materials_arg_buffer,
						   frame.frame_uniforms_buffer,
						   // fs
						   frame.frame_uniforms_buffer,
						   model->materials_arg_buffer,
						   frame.indirect_scene_fs_data)
			.draw_patches_indexed(vbuffers, *model->indices_buffer, *frame.tess_factors_buffer, 3u, model->index_count / 3u);
		frame.indirect_scene_gather_pipeline->add_render_command(*frame.dev_queue, scene_gather_pipeline)
			.set_arguments(// tes
						   model->materials_arg_buffer,
						   frame.frame_uniforms_buffer,
						   // fs
						   frame.frame_uniforms_buffer,
						   model->materials_arg_buffer,
						   frame.indirect_scene_fs_data)
			.draw_patches_indexed(vbuffers, *model->indices_buffer, *frame.tess_factors_buffer, 3u, model->index_count / 3u);
	} else {
		frame.indirect_scene_scatter_pipeline->add_render_command(*frame.dev_queue, *scatter_pipeline)
			.set_arguments(// vs
						   model->model_data_arg_buffer,
						   frame.frame_uniforms_buffer,
						   // fs
						   frame.frame_uniforms_buffer,
						   model->materials_arg_buffer,
						   frame.indirect_scene_fs_data)
			.draw_indexed(*model->indices_buffer, model->index_count);
		frame.indirect_scene_gather_pipeline->add_render_command(*frame.dev_queue, scene_gather_pipeline)
			.set_arguments(// vs
						   model->model_data_arg_buffer,
						   frame.frame_uniforms_buffer,
						   // fs
						   frame.frame_uniforms_buffer,
						   model->materials_arg_buffer,
						   frame.indirect_scene_fs_data)
			.draw_indexed(*model->indices_buffer, model->index_count);
	}
	
	frame.indirect_skybox_scatter_pipeline->add_render_command(*frame.dev_queue, *skybox_scatter_pipeline)
		.set_arguments(frame.frame_uniforms_buffer, frame.indirect_sky_fs_data)
		.draw(3u);
	frame.indirect_skybox_gather_pipeline->add_render_command(*frame.dev_queue,
															  (warp_state.is_gather_forward ? *skybox_gather_fwd_pipeline : *skybox_gather_pipeline))
		.set_arguments(frame.frame_uniforms_buffer, frame.indirect_sky_fs_data)
		.draw(3u);
	
	frame.indirect_shadow_pipeline->complete();
	frame.indirect_scene_scatter_pipeline->complete();
	frame.indirect_scene_gather_pipeline->complete();
	frame.indirect_skybox_scatter_pipeline->complete();
	frame.indirect_skybox_gather_pipeline->complete();
}

void unified_renderer::render() {
	if (in_flight_frames >= max_frames_in_flight || halt_renderer) {
		return;
	}
	
	try {
		const auto render_frame_idx = next_frame_idx;
		const auto& frame = frame_objects[render_frame_idx];
		if (!frame.present_done) {
			return;
		}
		
		// throttle rendering (there is no way to run vertex shading + compute at the same time anyways)
		if (frame_render_state.load() == 0) {
			return;
		}
		frame_render_state = 0;
		
		// start a new frame
		++in_flight_frames;
		frame_objects[render_frame_idx].rthread->render();
		next_frame_idx = (next_frame_idx + 1u) % max_frames_in_flight;
	} catch (exception& exc) {
		log_error("render abort: $", exc.what());
	}
}

void render_thread::render() {
	// wake up render thread
	// NOTE: we need to check the frame epoch as well, since the rendering might finish really fast,
	//       causing this loop/check not to catch it otherwise
	const auto& frame = renderer.frame_objects[frame_idx];
	const auto cur_epoch = frame.get_epoch();
	while (frame.present_done && cur_epoch == frame.get_epoch()) {
		render_cv.notify_one();
	}
}

void render_thread::run() {
	try {
		// wait until we should render a new frame,
		// time out after 500ms in case the renderer is being shut down or halted
		unique_lock<mutex> render_lock_guard(render_cv_lock);
		if (render_cv.wait_for(render_lock_guard, 500ms) == cv_status::timeout) {
			return;
		}
		
		// start rendering
		auto& frame = renderer.frame_objects[frame_idx];
		frame.next_epoch();
		frame.render_done.store(false);
		frame.present_done.store(false);
		
		const auto prev_frame_obj_idx = (frame_idx + renderer.max_frames_in_flight - 1) % renderer.max_frames_in_flight;
		auto& prev_frame = renderer.frame_objects[prev_frame_obj_idx];
		
		// TODO: make all of this thread-safe ...
		// time keeping
		static constexpr const long double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
		static auto time_keeper = chrono::high_resolution_clock::now();
		const auto now = chrono::high_resolution_clock::now();
		const auto delta = now - time_keeper;
		const auto deltaf = float(((long double)delta.count()) / time_den);
		static float render_delta { 0.0f };
		
		// decide if we are going to perform warping or render the full scene
		static const float frame_limit { 1.0f / float(warp_state.render_frame_count) };
		static size_t warp_frame_num = 0;
		const bool run_warp_kernel = ((deltaf < frame_limit && !renderer.first_frame) || warp_state.is_frame_repeat) && !warp_state.is_always_render;
		
		bool blit_frame = false;
		if (run_warp_kernel) {
			if (deltaf >= frame_limit) { // need to reset when over the limit
				render_delta = deltaf;
				time_keeper = now;
				warp_frame_num = 0;
			}
			
			if (warp_state.is_warping) {
				renderer.render_kernels(deltaf, render_delta, warp_frame_num, frame, prev_frame);
				blit_frame = false;
				++warp_frame_num;
			} else {
				blit_frame = true;
			}
		} else {
			renderer.first_frame = false;
			render_delta = deltaf;
			time_keeper = now;
			warp_frame_num = 0;
			
			renderer.render_full_scene(frame_idx, deltaf);
			
			blit_frame = warp_state.is_render_full;
		}
		
		// blit to window
		{
			// create the blit renderer
			shared_ptr<graphics_renderer> present_renderer = warp_state.ctx->create_graphics_renderer(*frame.dev_queue, *renderer.blit_pass,
																									  *renderer.blit_pipeline);
			if (!present_renderer->is_valid()) {
				return;
			}
			
			// only allow a single blit at a time
			safe_mutex blit_lock;
			GUARD(blit_lock);
			
			// setup drawable and attachments
			auto drawable = present_renderer->get_next_drawable();
			if (drawable == nullptr || !drawable->is_valid()) {
				log_error("failed to get next drawable");
				return;
			}
			
			present_renderer->set_attachments(drawable);
			present_renderer->begin();
			if (!run_warp_kernel) {
				present_renderer->wait_for_fence(*frame.render_post_skybox_fence);
			}
			
			static const graphics_renderer::multi_draw_entry blit_draw_info {
				.vertex_count = 3 // fullscreen triangle
			};
			present_renderer->draw(blit_draw_info, blit_frame ? frame.scene_fbo.color : frame.scene_fbo.compute_color);
			
			if (!run_warp_kernel) {
				present_renderer->signal_fence(*frame.render_post_blit_fence);
			}
			present_renderer->end();
			present_renderer->present();
#if 0
			// NOTE: must (and can) block this thread, now that rendering is fully async
			warp_log_wip("-> blit (fr $, ep $)", frame_idx, frame.get_epoch());
			present_renderer->commit(true);
			warp_log_wip("<- blit (fr $, ep $)", frame_idx, frame.get_epoch());
			renderer.finish_frame(frame_idx);
#else
			present_renderer->commit_and_release(std::move(present_renderer), [this] {
				/*// signal that this frame is done -> may destruct the whole renderer (if 0)
				--active_render;
				
				frame_ptr->rendering_active = false;*/
				
				warp_log_wip("blit done ($)", frame_idx);
				renderer.finish_frame(frame_idx);
			});
#endif
		}
	} catch (exception& exc) {
		log_error("exception during frame #$ render: $", frame_idx, exc.what());
	}
}

void unified_renderer::finish_frame(const uint32_t rendered_frame_idx) {
	// signal that the present is done and we can render a new frame
	frame_objects[rendered_frame_idx].present_done = true;
	--in_flight_frames;
	
	// update FPS stats
	{
		GUARD(fps_state_lock);
		
		// time when this frame finished
		const auto cur_time = chrono::steady_clock::now();
		
		// finished a frame
		++fps_state.fps_count;
		
		// if 1s has passed since "sec_start_point", we can provide a new FPS count
		if (cur_time - fps_state.sec_start_point > 1s) {
			// set new "sec_start_point"
			fps_state.sec_start_point = cur_time;
			cur_fps_count = fps_state.fps_count;
			new_fps_count = true;
			fps_state.fps_count = 0;
		}
	}
}

void unified_renderer::render_kernels(const float& delta, const float& render_delta, const size_t& warp_frame_num,
									  const frame_object_t& cur_frame, const frame_object_t& prev_frame) {
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
	if (warp_state.is_debug_delta) {
		if (dbg_delta >= (1.0f - delta_eps)) {
			dbg_delta = delta_eps;
			is_first_frame = true;
		} else {
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
	if (warp_state.is_scatter) {
		err = libwarp_scatter_floor(&cam_setup, relative_delta, is_first_frame && warp_state.is_clear_frame,
									cur_frame.scene_fbo.color,
									cur_frame.scene_fbo.depth,
									cur_frame.scene_fbo.motion[0],
									cur_frame.scene_fbo.compute_color);
	} else {
		if (warp_state.is_gather_forward) {
			err = libwarp_gather_forward_only_floor(&cam_setup, relative_delta,
													cur_frame.scene_fbo.color,
													cur_frame.scene_fbo.motion[0],
													cur_frame.scene_fbo.compute_color);
		} else {
			err = libwarp_gather_floor(&cam_setup, relative_delta,
									   cur_frame.scene_fbo.color,
									   cur_frame.scene_fbo.depth,
									   prev_frame.scene_fbo.color,
									   prev_frame.scene_fbo.depth,
									   prev_frame.scene_fbo.motion[0],
									   cur_frame.scene_fbo.motion[1],
									   prev_frame.scene_fbo.motion_depth,
									   cur_frame.scene_fbo.motion_depth,
									   cur_frame.scene_fbo.compute_color);
		}
	}
	if(err != LIBWARP_SUCCESS) log_error("libwarp error: $", err);
	
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	log_debug("warp timing: $", double(floor_timer::stop<chrono::microseconds>(timing_start)) / 1000.0);
#endif
}

void unified_renderer::update_uniforms(const uint32_t frame_idx, const float time_delta) {
	auto& frame = frame_objects[frame_idx];
	
	// this should never run in more than one thread at once
	static safe_mutex render_full_scene_mutex;
	GUARD(render_full_scene_mutex);
	
	// update camera state
	frame.cam_state = cam->get_current_state();
	
	warp_log_wip("update uniforms: $", frame_idx);
	
	// light handling (TODO: proper light)
	{
		static const float3 light_min { -80.0f, 250.0f, 0.0f }, light_max { 80.0f, 250.0f, 0.0f };
		//static const float3 light_min { -115.0f, 300.0f, 0.0f }, light_max { 115.0f, 0.0f, 0.0f };
		static float light_interp { 0.5f }, light_interp_dir { 1.0f };
		static constexpr const float light_slow_down { 0.01f };
		
		// TODO: should really compute time delta in here (for most current value)
		light_interp += (time_delta * light_slow_down) * light_interp_dir;
		if (light_interp > 1.0f) {
			light_interp = 1.0f;
			light_interp_dir *= -1.0f;
		} else if(light_interp < 0.0f) {
			light_interp = 0.0f;
			light_interp_dir *= -1.0f;
		}
		light_pos = light_min.interpolated(light_max, light_interp);
	}
	
	frame_uniforms = warp_shaders::frame_uniforms_t {
		.cam_pos = frame.cam_state.position * float3 { -1.0f, 1.0f, -1.0f },
		.light_pos = light_pos,
		.displacement_mode = warp_state.displacement_mode,
	};
	
	// update shadow map uniforms
	{
		const matrix4f light_pm = matrix4f::perspective(110.0f, 1.0f,
														warp_state.shadow_near_far_plane.x,
														warp_state.shadow_near_far_plane.y);
		const matrix4f light_mvm {
			matrix4f::translation(-light_pos) *
			matrix4f::rotation_deg_named<'x'>(90.0f) // rotate downwards
		};
		frame_uniforms.light_mvpm = light_mvm * light_pm;
		frame_uniforms.light_bias_mvpm = {
			frame_uniforms.light_mvpm * matrix4f().scale(0.5f, 0.5f, 0.5f).translate(0.5f, 0.5f, 0.5f)
		};
	}
	
	// update scene uniforms
	{
		pm = matrix4f::perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
								   warp_state.near_far_plane.x, warp_state.near_far_plane.y);
		rmvm = (matrix4f::rotation_deg_named<'y'>(float(frame.cam_state.rotation.y)) *
				matrix4f::rotation_deg_named<'x'>(float(frame.cam_state.rotation.x)));
		mvm = matrix4f::translation(frame.cam_state.position * float3 { 1.0f, -1.0f, 1.0f }) * rmvm;
		const matrix4f mvpm {
			warp_state.is_scatter ?
			mvm * pm :
			prev_mvm * pm
		};
		const matrix4f next_mvpm { mvm * pm }; // gather
		const matrix4f prev_mvpm { prev_prev_mvm * pm }; // gather
		frame_uniforms.mvpm = mvpm;
		frame_uniforms.scatter.scene_mvm = mvm;
		frame_uniforms.scatter.scene_prev_mvm = prev_mvm;
		frame_uniforms.gather.scene_next_mvpm = next_mvpm;
		frame_uniforms.gather.scene_prev_mvpm = prev_mvpm;
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
		frame_uniforms.sky_imvpm = imvpm;
		frame_uniforms.scatter.sky_prev_imvpm = prev_imvpm;
		frame_uniforms.gather.sky_next_mvpm = next_mvpm;
		frame_uniforms.gather.sky_prev_mvpm = prev_mvpm;
	}
	
	// write uniforms on the GPU side (indirect pipeline) or write host-side structs (direct pipeline)
	if (warp_state.use_indirect_commands) {
		// TODO: this may actually already write data before the last frame has finished rendering ... need render_post_scene_fence wait here?
		frame.frame_uniforms_buffer->write(*frame.dev_queue, &frame_uniforms);
		//frame.dev_queue->finish(); // TODO: very unfortunate ... use separate queues? use other mem type?
	} else {
		scene_uniforms = {
			.mvpm = frame_uniforms.mvpm,
			.light_bias_mvpm = frame_uniforms.light_bias_mvpm,
			.cam_pos = frame_uniforms.cam_pos,
			.light_pos = frame_uniforms.light_pos,
			.m0 = (warp_state.is_scatter ? frame_uniforms.scatter.scene_mvm : frame_uniforms.gather.scene_next_mvpm),
			.m1 = (warp_state.is_scatter ? frame_uniforms.scatter.scene_prev_mvm : frame_uniforms.gather.scene_prev_mvpm),
		};
		skybox_uniforms = {
			.imvpm = frame_uniforms.sky_imvpm,
			.m0 = (warp_state.is_scatter ? frame_uniforms.scatter.sky_prev_imvpm : frame_uniforms.gather.sky_next_mvpm),
			.m1 = (warp_state.is_scatter ? matrix4f() : frame_uniforms.gather.sky_prev_mvpm),
		};
	}
	
	// remember t-2 and t-1 mvms
	prev_prev_mvm = prev_mvm;
	prev_prev_rmvm = prev_rmvm;
	prev_mvm = mvm;
	prev_rmvm = rmvm;
}

void unified_renderer::render_full_scene(const uint32_t frame_idx, const float time_delta) {
	// frame object that is used to render this frame
	auto& frame = frame_objects[frame_idx];
	warp_log_wip("render (fr $, ep $; delta: $ / $ms)", frame_idx, frame.get_epoch(), time_delta, time_delta * 1000.0f);
	
	// when using direct rendering (-> uploading uniforms in each draw call), we must already update all uniforms here
	if (!warp_state.use_indirect_commands) {
		update_uniforms(frame_idx, time_delta);
	}
	//frame_render_state.store(1u);
	
	//////////////////////////////////////////
	// create and encode all renderers
	
	unique_ptr<graphics_renderer> shadow_map_renderer;
	unique_ptr<graphics_renderer> scene_renderer;
	unique_ptr<graphics_renderer> sky_box_renderer;
	
	// our draw info is static, so only create it once
	static const graphics_renderer::multi_draw_indexed_entry scene_draw_info {
		.index_buffer = model->indices_buffer.get(),
		.index_count = model->index_count
	};
	static const graphics_renderer::patch_draw_indexed_entry scene_draw_patch_info {
		.control_point_buffers = {
			model->vertices_buffer.get(),
			model->tex_coords_buffer.get(),
			model->normals_buffer.get(),
			model->binormals_buffer.get(),
			model->tangents_buffer.get(),
			model->materials_data_buffer.get(),
		},
		.control_point_index_buffer = model->indices_buffer.get(),
		.patch_control_point_count = 3u,
		.first_index = 0u,
		.patch_count = model->index_count / 3u,
		.first_patch = 0u,
	};
	static const graphics_renderer::multi_draw_entry skybox_draw_info {
		.vertex_count = 3
	};
	
	// shadow map
	{
		shadow_map_renderer = warp_state.ctx->create_graphics_renderer(*frame.dev_queue, *shadow_pass, *shadow_pipeline);
		if (!shadow_map_renderer->is_valid()) {
			return;
		}
		shadow_map_renderer->set_attachments(frame.shadow_map.shadow_image);
		shadow_map_renderer->begin();
		
		shadow_map_renderer->wait_for_fence(*frame.render_post_tess_update_fence);
		if (!warp_state.use_indirect_commands) {
			shadow_map_renderer->draw_indexed(scene_draw_info, model->vertices_buffer, frame_uniforms.light_mvpm);
		} else {
			shadow_map_renderer->execute_indirect(*frame.indirect_shadow_pipeline);
		}
		shadow_map_renderer->signal_fence(*frame.render_post_shadow_fence, compute_fence::SYNC_STAGE::FRAGMENT /* must be fragment, b/c we write a depth buffer */);
		
		shadow_map_renderer->end();
	}
	
	// actual scene
	{
		scene_renderer = warp_state.ctx->create_graphics_renderer(*frame.dev_queue,
																  (warp_state.is_scatter ? *scatter_passes[0] :
																   (warp_state.is_gather_forward ? *gather_fwd_passes[0] : *gather_passes[0])),
																  (warp_state.is_scatter ? *scatter_pipeline :
																   (warp_state.is_gather_forward ? *gather_fwd_pipeline : *gather_pipeline)));
		if (!scene_renderer->is_valid()) {
			return;
		}
		
		if (warp_state.is_scatter || warp_state.is_gather_forward) {
			scene_renderer->set_attachments(frame.scene_fbo.color, frame.scene_fbo.motion[0], frame.scene_fbo.depth);
		} else { // gather
			// for bidirectional gather rendering, this switches every frame
			scene_renderer->set_attachments(frame.scene_fbo.color,
											frame.scene_fbo.motion[0],
											frame.scene_fbo.motion[1],
											frame.scene_fbo.motion_depth,
											frame.scene_fbo.depth);
		}
		
		// scene
		scene_renderer->begin();
		//scene_renderer->wait_for_fence(*frame.render_post_blit_fence /* from previous frame */); // TODO: needed?
		scene_renderer->wait_for_fence(*frame.render_post_shadow_fence);
		if (!warp_state.use_indirect_commands) {
			// -> direct rendering
			if (warp_state.use_tessellation) {
				scene_renderer->set_tessellation_factors(*frame.tess_factors_buffer);
				if (!warp_state.use_argument_buffer) {
					scene_renderer->draw_patches_indexed(scene_draw_patch_info,
														 // vertex shader
														 model->displacement_textures,
														 warp_state.displacement_mode,
														 scene_uniforms,
														 // fragment shader
														 warp_state.displacement_mode,
														 model->diffuse_textures,
														 model->specular_textures,
														 model->normal_textures,
														 model->mask_textures,
														 model->displacement_textures,
														 frame.shadow_map.shadow_image);
				} else {
					scene_renderer->draw_patches_indexed(scene_draw_patch_info,
														 // vertex shader
														 model->displacement_textures,
														 warp_state.displacement_mode,
														 scene_uniforms,
														 // fragment shader
														 warp_state.displacement_mode,
														 model->materials_arg_buffer,
														 frame.shadow_map.shadow_image);
				}
			} else {
				if (!warp_state.use_argument_buffer) {
					scene_renderer->draw_indexed(scene_draw_info,
												 // vertex shader
												 model->vertices_buffer,
												 model->tex_coords_buffer,
												 model->normals_buffer,
												 model->binormals_buffer,
												 model->tangents_buffer,
												 model->materials_data_buffer,
												 scene_uniforms,
												 // fragment shader
												 warp_state.displacement_mode,
												 model->diffuse_textures,
												 model->specular_textures,
												 model->normal_textures,
												 model->mask_textures,
												 model->displacement_textures,
												 frame.shadow_map.shadow_image);
				} else {
					scene_renderer->draw_indexed(scene_draw_info,
												 // vertex shader
												 model->model_data_arg_buffer,
												 scene_uniforms,
												 // fragment shader
												 warp_state.displacement_mode,
												 model->materials_arg_buffer,
												 frame.shadow_map.shadow_image);
				}
			}
		} else {
			// -> indirect rendering
			scene_renderer->execute_indirect(warp_state.is_scatter ? *frame.indirect_scene_scatter_pipeline : *frame.indirect_scene_gather_pipeline);
		}
		scene_renderer->signal_fence(*frame.render_post_scene_fence, compute_fence::SYNC_STAGE::FRAGMENT);
		scene_renderer->end();
	}
	
	// sky box
	{
		sky_box_renderer = warp_state.ctx->create_graphics_renderer(*frame.dev_queue,
																	(warp_state.is_scatter ? *scatter_passes[1] :
																	 (warp_state.is_gather_forward ? *gather_fwd_passes[1] : *gather_passes[1])),
																	(warp_state.is_scatter ? *skybox_scatter_pipeline :
																	 (warp_state.is_gather_forward ? *skybox_gather_fwd_pipeline : *skybox_gather_pipeline)));
		if (!sky_box_renderer->is_valid()) {
			return;
		}
		
		if (warp_state.is_scatter || warp_state.is_gather_forward) {
			sky_box_renderer->set_attachments(frame.scene_fbo.color, frame.scene_fbo.motion[0], frame.scene_fbo.depth);
		} else { // gather
			sky_box_renderer->set_attachments(frame.scene_fbo.color,
											  frame.scene_fbo.motion[0],
											  frame.scene_fbo.motion[1],
											  frame.scene_fbo.motion_depth,
											  frame.scene_fbo.depth);
		}
		
		// skybox
		sky_box_renderer->begin();
		//sky_box_renderer->wait_for_fence(*frame.render_post_blit_fence /* from previous frame */); // TODO: needed?
		sky_box_renderer->wait_for_fence(*frame.render_post_scene_fence);
		if (!warp_state.use_indirect_commands) {
			sky_box_renderer->draw(skybox_draw_info, skybox_uniforms, skybox_tex);
		} else {
			sky_box_renderer->execute_indirect(warp_state.is_scatter ? *frame.indirect_skybox_scatter_pipeline : *frame.indirect_skybox_gather_pipeline);
		}
		sky_box_renderer->signal_fence(*frame.render_post_skybox_fence, compute_fence::SYNC_STAGE::FRAGMENT);
		sky_box_renderer->end();
	}
	
	// when using indirect rendering (-> uniforms are located in one global buffer), we can delay the uniform update until this point
	// NOTE: this allows us to use a more current camera state!
	if (warp_state.use_indirect_commands) {
		update_uniforms(frame_idx, time_delta);
	}
	
	//////////////////////////////////////////
	// update tessellation factors
	if (warp_state.use_tessellation) {
		compute_queue::execution_parameters_t exec_params {
			.global_work_size = uint1 { model->index_count / 3u },
			.local_work_size = uint1 { 1024u },
			.args = {
				model->model_data_arg_buffer,
				frame.tess_factors_buffer,
				frame_uniforms.cam_pos,
				float(gather_pipeline->get_description(false).tessellation.max_factor),
			},
			.wait_fences = { frame.render_post_scene_fence.get() /* from previous frame */ },
			.signal_fences = { frame.render_post_tess_update_fence.get() },
			.debug_label = "tess_update_factors",
		};
		frame.dev_queue->execute_with_parameters(*shaders[TESS_UPDATE_FACTORS], exec_params);
	}
	
	//////////////////////////////////////////
	// commit & run renderers
	shadow_map_renderer->commit_and_release(std::move(shadow_map_renderer));
#if 1
	scene_renderer->commit_and_release(std::move(scene_renderer));
#else // -> "bandwidth"
	scene_renderer->commit([this, frame_idx]() {
		// only start new frame once this is done
		warp_log_wip("> allow render (from $)", frame_idx);
		frame_render_state.store(1u);
	});
#endif
#if 0
	sky_box_renderer->commit();
#else // -> "latency"
	sky_box_renderer->commit_and_release(std::move(sky_box_renderer), [this, frame_idx]() {
		// only start new frame once this is done
		warp_log_wip("> allow render (from $)", frame_idx);
		frame_render_state.store(1u);
	});
#endif
	
	//////////////////////////////////////////
	// end
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
													   (warp_state.use_argument_buffer ? " -DWARP_USE_ARGUMENT_BUFFER=1" : "") +
													   (warp_state.use_indirect_commands ? " -DWARP_USE_INDIRECT_COMMANDS=1" : "") +
													   add_cli_options);
	
	if (new_shader_prog == nullptr) {
		log_error("shader compilation failed");
		return false;
	}
	
	// NOTE: corresponds to WARP_SHADER
	static const char* shader_names[warp_shader_count()] {
		"scene_scatter_vs",
		(warp_state.use_tessellation ? "scene_scatter_tes" : "scene_scatter_vs"),
		"scene_scatter_fs",
		"scene_gather_vs",
		(warp_state.use_tessellation ? "scene_gather_tes" : "scene_gather_vs"),
		"scene_gather_fs",
		"scene_gather_vs",
		(warp_state.use_tessellation ? "scene_gather_tes" : "scene_gather_vs"),
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

void unified_renderer::post_init(const compute_queue& dev_queue, floor_obj_model& model_, const camera& cam_) {
	model = &model_;
	cam = &cam_;
	
	if (warp_state.use_argument_buffer) {
		// NOTE: scene_scatter_fs/scene_gather_fs/scene_gather_fwd_fs have the same function signature -> doesn't matter which one we take
		model->materials_arg_buffer = shaders[WARP_SHADER::SCENE_GATHER_FS]->create_argument_buffer(dev_queue, 2);
		if (!model->materials_arg_buffer) {
			throw runtime_error("failed to create materials argument buffer");
		}
		model->materials_arg_buffer->set_arguments(dev_queue,
												   model->diffuse_textures,
												   model->specular_textures,
												   model->normal_textures,
												   model->mask_textures,
												   model->displacement_textures);
		
		// model arg buffer has the same signature everywhere -> doesn't matter which one we take (use scene_gather_vs)
		model->model_data_arg_buffer = shaders[WARP_SHADER::SCENE_GATHER_VS]->create_argument_buffer(dev_queue, 0);
		if (!model->model_data_arg_buffer) {
			throw runtime_error("failed to create model data argument buffer");
		}
		model->model_data_arg_buffer->set_arguments(dev_queue,
													vector { model->vertices_buffer, model->normals_buffer, model->binormals_buffer, model->tangents_buffer },
													model->tex_coords_buffer,
													model->materials_data_buffer,
													model->indices_buffer,
													model->index_count / 3u);
		
		for (uint32_t frame_idx = 0; frame_idx < max_frames_in_flight; ++frame_idx) {
			auto& frame = frame_objects[frame_idx];
			
			// tessellation factors buffer
			if (warp_state.use_tessellation) {
				const auto tess_factors_size = sizeof(triangle_tessellation_levels_t) * (model->index_count / 3u);
				frame.tess_factors_buffer = warp_state.ctx->create_buffer(*frame.dev_queue, tess_factors_size);
				frame.tess_factors_buffer->set_debug_label("tess_factors#" + to_string(frame_idx));
			}
			
			// initial indirect pipelines encode + required argument buffer creation
			if (warp_state.use_indirect_commands) {
				// argument buffers
				frame.indirect_scene_fs_data = shaders[WARP_SHADER::SCENE_GATHER_FS]->create_argument_buffer(*frame.dev_queue, 3);
				if (!frame.indirect_scene_fs_data) {
					throw runtime_error("failed to create scene-fragment-shader-data argument buffer");
				}
				
				frame.indirect_sky_fs_data = shaders[WARP_SHADER::SKYBOX_GATHER_FS]->create_argument_buffer(*frame.dev_queue, 1);
				if (!frame.indirect_sky_fs_data) {
					throw runtime_error("failed to create scene-fragment-shader-data argument buffer");
				}
				
				frame.indirect_scene_fs_data->set_arguments(*frame.dev_queue, frame.shadow_map.shadow_image);
				frame.indirect_sky_fs_data->set_arguments(*frame.dev_queue, skybox_tex);
				
				// encode
				encode_indirect_pipelines(frame_idx);
			}
		}
	}
}

uint32_t unified_renderer::get_fps() {
	return cur_fps_count.load();
}

bool unified_renderer::is_new_fps_count() {
	return new_fps_count.exchange(false);
}
