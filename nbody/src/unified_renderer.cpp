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

#include "unified_renderer.hpp"
#include "nbody_state.hpp"
#include <floor/compute/compute_context.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_image.hpp>
#include <floor/compute/compute_queue.hpp>
#include <floor/compute/compute_kernel.hpp>
#include <floor/graphics/graphics_renderer.hpp>
#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/graphics/graphics_pass.hpp>

// renderer
static unique_ptr<graphics_pass> renderer_pass;
static unique_ptr<graphics_pipeline> renderer_pipeline;

static array<shared_ptr<compute_image>, 2> body_textures {};
static void create_textures(const compute_context& ctx, const compute_queue& dev_queue) {
	// create/generate an opengl texture and bind it
	for (size_t i = 0; i < body_textures.size(); ++i) {
		// create texture
		static constexpr uint2 texture_size { 64, 64 };
		static constexpr float2 texture_sizef { texture_size };
		array<ushort4, texture_size.x * texture_size.y> pixel_data;
		for (uint32_t y = 0; y < texture_size.y; ++y) {
			for (uint32_t x = 0; x < texture_size.x; ++x) {
				float2 dir = (float2(x, y) / texture_sizef) * 2.0f - 1.0f;
#if 1 // smoother, less of a center point
				float fval = dir.dot();
#else
				float fval = dir.length();
#endif
				const auto val = 1.0f - const_math::clamp(fval, 0.0f, 1.0f);
				const auto alpha_val = (val > 0.25f ? 1.0f : val);
				const auto half_val = soft_f16::float_to_half(val);
				const auto half_alpha_val = soft_f16::float_to_half(alpha_val);
				pixel_data[y * texture_size.x + x] = { half_val, half_val, half_val, half_alpha_val };
			}
		}
		
		body_textures[i] = ctx.create_image(dev_queue, texture_size,
											(COMPUTE_IMAGE_TYPE::IMAGE_2D |
											 COMPUTE_IMAGE_TYPE::RGBA16F |
											 COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED |
											 COMPUTE_IMAGE_TYPE::READ),
											&pixel_data[0],
											(COMPUTE_MEMORY_FLAG::READ |
											 COMPUTE_MEMORY_FLAG::HOST_WRITE |
											 COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS));
	}
}

bool unified_renderer::init(const compute_context& ctx, const compute_queue& dev_queue, const compute_kernel& vs, const compute_kernel& fs) {
	create_textures(ctx, dev_queue);
	
	const auto render_format = ctx.get_renderer_image_type();

	const render_pass_description pass_desc {
		.attachments = {
			{
				.format = render_format,
				.load_op = LOAD_OP::CLEAR,
				.store_op = STORE_OP::STORE,
				.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
			}
		}
	};
	renderer_pass = ctx.create_graphics_pass(pass_desc);
	
	const render_pipeline_description pipeline_desc {
		.vertex_shader = &vs,
		.fragment_shader = &fs,
		.primitive = PRIMITIVE::POINT,
		.cull_mode = CULL_MODE::BACK,
		.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
		.viewport = floor::get_physical_screen_size(),
		.depth = {
			.write = false,
			.compare = DEPTH_COMPARE::ALWAYS,
		},
		.color_attachments = {
			{
				.format = render_format,
				.blend = {
					.enable = true,
					.src_color_factor = BLEND_FACTOR::ONE,
					.dst_color_factor = BLEND_FACTOR::ONE_MINUS_SRC_COLOR,
					.color_blend_op = BLEND_OP::ADD,
					.src_alpha_factor = BLEND_FACTOR::ONE,
					.dst_alpha_factor = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA,
					.alpha_blend_op = BLEND_OP::ADD,
				},
			}
		},
	};
	renderer_pipeline = ctx.create_graphics_pipeline(pipeline_desc);
	
	return true;
}

void unified_renderer::destroy(const compute_context& ctx floor_unused) {
	for (auto& tex : body_textures) {
		tex = nullptr;
	}
	
	// clean up renderer stuff
	renderer_pass = nullptr;
	renderer_pipeline = nullptr;
}

void unified_renderer::render(const compute_context& ctx, const compute_queue& dev_queue, const compute_buffer& position_buffer) {
	// create the renderer for this frame
	auto renderer = ctx.create_graphics_renderer(dev_queue, *renderer_pass, *renderer_pipeline);
	if (!renderer->is_valid()) {
		return;
	}
	
	// setup drawable and attachments
	auto drawable = renderer->get_next_drawable();
	if (drawable == nullptr || !drawable->is_valid()) {
		log_error("failed to get next drawable");
		return;
	}
	renderer->set_attachments(drawable);
	
	// attachments set, can begin rendering now
	renderer->begin();
	
	// setup uniforms
	const matrix4f mproj { matrix4f().perspective(90.0f, float(floor::get_width()) / float(floor::get_height()),
												  0.25f, nbody_state.max_distance) };
	const matrix4f mview { nbody_state.cam_rotation.to_matrix4() * matrix4f().translate(0.0f, 0.0f, -nbody_state.distance) };
	const struct {
		matrix4f mvpm;
		matrix4f mvm;
		float2 mass_minmax;
	} uniforms {
		.mvpm = mview * mproj,
		.mvm = mview,
		.mass_minmax = nbody_state.mass_minmax
	};
	
	// draw
	static const vector<graphics_renderer::multi_draw_entry> nbody_draw_info {{
		.vertex_count = nbody_state.body_count
	}};
	renderer->multi_draw(nbody_draw_info,
						 // vs args
						 position_buffer,
						 uniforms,
						 // fs args
						 body_textures[0]);
	renderer->end();
	
	// present to the screen and commit
	renderer->present();
	renderer->commit();
}
