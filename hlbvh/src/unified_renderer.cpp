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

#include "unified_renderer.hpp"

#include "hlbvh_state.hpp"
#include <floor/device/device_context.hpp>
#include <floor/device/device_function.hpp>
#include <floor/device/graphics_renderer.hpp>
#include <floor/device/graphics_pipeline.hpp>
#include <floor/device/graphics_pass.hpp>

// renderer
static std::unique_ptr<graphics_pass> render_pass;
static std::unique_ptr<graphics_pipeline> render_pipeline;
static std::shared_ptr<device_buffer> uniforms_buffer;

static struct {
	std::shared_ptr<device_image> depth;
	uint2 dim;
} scene_fbo;

struct uniforms_t {
	matrix4f mvpm;
	float4 default_color;
	float3 light_dir;
};

void unified_renderer::destroy() {
	render_pass = nullptr;
	render_pipeline = nullptr;
	scene_fbo.depth = nullptr;
	uniforms_buffer = nullptr;
}

static void create_resources() {
	scene_fbo.dim = floor::get_physical_screen_size();
	
	scene_fbo.depth = hlbvh_state.rctx->create_image(*hlbvh_state.rqueue, scene_fbo.dim,
													 IMAGE_TYPE::IMAGE_DEPTH |
													 IMAGE_TYPE::D32F |
													 IMAGE_TYPE::READ |
													 IMAGE_TYPE::FLAG_RENDER_TARGET,
													 MEMORY_FLAG::READ);
	
	uniforms_buffer = hlbvh_state.rctx->create_buffer(*hlbvh_state.rqueue, sizeof(uniforms_t),
													  MEMORY_FLAG::READ |
													  MEMORY_FLAG::HOST_WRITE |
													  MEMORY_FLAG::VULKAN_HOST_COHERENT);
	uniforms_buffer->set_debug_label("uniforms");
}

bool unified_renderer::init(std::shared_ptr<device_function> vs,
							std::shared_ptr<device_function> fs) {
	// check vs/fs and get state
	if (!vs) {
		log_error("vertex shader not found");
		return false;
	}
	if (!fs) {
		log_error("fragment shader not found");
		return false;
	}
	
	//
	create_resources();
	
	const auto screen_format = hlbvh_state.rctx->get_renderer_image_type();
	const auto depth_format = scene_fbo.depth->get_image_type();
	
	render_pass_description pass_desc {
		.attachments = {
			// color
			{
				.format = screen_format,
				.load_op = LOAD_OP::CLEAR,
				.store_op = STORE_OP::STORE,
				.clear.color = {},
			},
			// depth
			{
				.format = depth_format,
				.load_op = LOAD_OP::CLEAR,
				.store_op = STORE_OP::STORE,
				.clear.depth = 1.0f,
			},
		},
		.debug_label = "hlbvh_render_pass",
	};
	render_pass = hlbvh_state.rctx->create_graphics_pass(pass_desc);
	if (!render_pass) {
		log_error("failed to create render_pass");
		return false;
	}
	
	render_pipeline_description pipeline_desc {
		.vertex_shader = vs.get(),
		.fragment_shader = fs.get(),
		.primitive = PRIMITIVE::TRIANGLE,
		.cull_mode = CULL_MODE::BACK,
		.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
		.depth = {
			.write = true,
			.compare = DEPTH_COMPARE::LESS,
		},
		.color_attachments = {
			{ .format = screen_format, },
		},
		.depth_attachment = { .format = depth_format },
		.support_indirect_rendering = false,
		.debug_label = "hlbvh_render_pipeline",
	};
	render_pipeline = hlbvh_state.rctx->create_graphics_pipeline(pipeline_desc);
	if (!render_pipeline) {
		log_error("failed to create render_pipeline");
		return false;
	}
	
	return true;
}

void unified_renderer::render(const std::vector<std::unique_ptr<animation>>& models,
							  const bool cam_mode,
							  const camera& cam) {
	auto renderer = hlbvh_state.rctx->create_graphics_renderer(*hlbvh_state.rqueue, *render_pass, *render_pipeline, false);
	if (!renderer->is_valid()) {
		return;
	}
	
	auto drawable = renderer->get_next_drawable(false);
	if (drawable == nullptr || !drawable->is_valid()) {
		log_error("failed to get next drawable");
		return;
	}
	
	renderer->set_attachments(drawable, scene_fbo.depth);
	renderer->begin();
	
	//
	const auto mproj = matrix4f::perspective(72.0f, float(floor::get_width()) / float(floor::get_height()),
											 0.25f, hlbvh_state.max_distance);
	matrix4f mview;
	if (!cam_mode) {
		mview = hlbvh_state.cam_rotation.to_matrix4() * matrix4f::translation(0.0f, 0.0f, -hlbvh_state.distance);
	} else {
		const auto rmvm = (matrix4f::rotation_deg_named<'y'>(float(cam.get_rotation().y)) *
						   matrix4f::rotation_deg_named<'x'>(float(cam.get_rotation().x)));
		mview = matrix4f::translation(cam.get_position() * float3 { 1.0f, -1.0f, 1.0f }) * rmvm;
	}
	
	static constexpr const float4 default_color { 0.9f, 0.9f, 0.9f, 1.0f };
	uniforms_t uniforms {
		.mvpm = mview * mproj,
		.default_color = default_color,
		.light_dir = { 1.0f, 0.0f, 0.0f },
	};
	static_assert(sizeof(uniforms) == 92, "invalid uniforms size");
	uniforms_buffer->write(*hlbvh_state.rqueue, &uniforms);
	hlbvh_state.rqueue->finish();
	
	for (const auto& mdl : models) {
		const auto cur_frame = (const floor_obj_model*)mdl->frames[mdl->cur_frame].get();
		const auto next_frame = (const floor_obj_model*)mdl->frames[mdl->next_frame].get();
		
		for (const auto& obj : cur_frame->objects) {
			const graphics_renderer::multi_draw_indexed_entry model_draw_info {
				.index_buffer = obj->indices_floor_vbo.get(),
				.index_count = uint32_t(obj->index_count)
			};
			renderer->draw_indexed(model_draw_info,
								   // vertex shader
								   cur_frame->vertices_buffer,
								   next_frame->vertices_buffer,
								   cur_frame->normals_buffer,
								   next_frame->normals_buffer,
								   uniforms_buffer,
								   mdl->step,
								   hlbvh_state.triangle_vis ? 1u : 0u,
								   mdl->colliding_vertices,
								   // fragment shader
								   uniforms_buffer);
		}
	}
	
	//
	renderer->end();
	renderer->present();
	renderer->commit_and_finish();
}
