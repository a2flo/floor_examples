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
#include <floor/vr/vr_context.hpp>

// renderer
static unique_ptr<graphics_pass> renderer_pass;
static unique_ptr<graphics_pipeline> renderer_pipeline;
static shared_ptr<compute_image> render_image;
static unique_ptr<graphics_pass> blit_pass;
static unique_ptr<graphics_pipeline> blit_pipeline;
static bool is_vr_renderer { false };

static array<shared_ptr<compute_image>, 2> body_textures {};
static void create_textures(const compute_context& ctx, const compute_queue& dev_queue) {
	// create/generate an opengl texture and bind it
	for (auto& body_texture : body_textures) {
		// create texture
		static constexpr uint2 texture_size { 64, 64 };
		static constexpr float2 texture_sizef { texture_size };
		array<ushort4, texture_size.x * texture_size.y> pixel_data;
		for (uint32_t y = 0; y < texture_size.y; ++y) {
			for (uint32_t x = 0; x < texture_size.x; ++x) {
				float2 dir = (uint2(x, y).cast<float>() / texture_sizef) * 2.0f - 1.0f;
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

		body_texture = ctx.create_image(dev_queue, texture_size,
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

bool unified_renderer::init(const compute_context& ctx, const compute_queue& dev_queue,
							const compute_kernel& vs, const compute_kernel& fs,
							const compute_kernel* blit_vs, const compute_kernel* blit_fs, const compute_kernel* blit_fs_layered) {
	if (blit_vs == nullptr || blit_fs == nullptr || blit_fs_layered == nullptr) {
		log_error("missing blit shader(s)");
		return false;
	}

	create_textures(ctx, dev_queue);

	is_vr_renderer = ctx.is_vr_supported();

	// nbody rendering pipeline/pass
	const auto render_format = COMPUTE_IMAGE_TYPE::RGBA16F;

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
	if (!renderer_pass) {
		return false;
	}
	
	const render_pipeline_description pipeline_desc {
		.vertex_shader = &vs,
		.fragment_shader = &fs,
		.primitive = PRIMITIVE::POINT,
		.cull_mode = CULL_MODE::BACK,
		.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
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
	if (!renderer_pipeline) {
		return false;
	}

	// intermediate render output image
	render_image = ctx.create_image(dev_queue, ctx.get_renderer_image_dim(),
									render_format |
									(!is_vr_renderer ? COMPUTE_IMAGE_TYPE::IMAGE_2D : COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY) |
									COMPUTE_IMAGE_TYPE::READ_WRITE |
									COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
									COMPUTE_MEMORY_FLAG::READ_WRITE);

	// blit pipeline/pass
	const auto screen_format = ctx.get_renderer_image_type();

	const render_pass_description blit_pass_desc {
		.attachments = {
			// color
			{
				.format = screen_format,
				.load_op = LOAD_OP::DONT_CARE, // don't care b/c drawing the whole screen
				.store_op = STORE_OP::STORE,
				.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
			},
		}
	};
	blit_pass = ctx.create_graphics_pass(blit_pass_desc);

	const render_pipeline_description blit_pipeline_desc {
		.vertex_shader = blit_vs,
		.fragment_shader = (!is_vr_renderer ? blit_fs : blit_fs_layered),
		.primitive = PRIMITIVE::TRIANGLE,
		.cull_mode = CULL_MODE::BACK,
		.front_face = FRONT_FACE::COUNTER_CLOCKWISE,
		.depth = {
			.write = false,
			.compare = DEPTH_COMPARE::ALWAYS,
		},
		.color_attachments = {
			{ .format = screen_format, },
		},
	};
	blit_pipeline = ctx.create_graphics_pipeline(blit_pipeline_desc);
	
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
	// render scene
	{
		// create the renderer for this frame
		auto renderer = ctx.create_graphics_renderer(dev_queue, *renderer_pass, *renderer_pipeline, is_vr_renderer);
		if (!renderer->is_valid()) {
			return;
		}
		
		renderer->set_attachments(render_image);
		
		// attachments set, can begin rendering now
		renderer->begin();

		// setup uniforms
		struct uniforms_t {
			matrix4f mvpms[2];
			matrix4f mvms[2];
			float2 mass_minmax;
		};

		uniforms_t uniforms {
			.mass_minmax = nbody_state.mass_minmax
		};
		const matrix4f mview_scene { nbody_state.cam_rotation.to_matrix4() * matrix4f::translation(0.0f, 0.0f, -nbody_state.distance) };
		if (!is_vr_renderer) {
			const matrix4f mproj { matrix4f::perspective(90.0f, render_image->get_aspect_ratio(), 0.25f, nbody_state.max_distance) };
			uniforms.mvpms[0] = mview_scene * mproj;
			uniforms.mvms[0] = mview_scene;
		} else {
#if !defined(FLOOR_NO_VR)
			const auto vr_ctx = ctx.get_renderer_vr_context();

			auto mview_hmd = vr_ctx->get_hmd_matrix();
			mview_hmd.set_translation(0.0f, 0.0f, 0.0f); // don't want any HMD translation here
			const auto mview_eye_left = vr_ctx->get_eye_matrix(VR_EYE::LEFT);
			const auto mview_eye_right = vr_ctx->get_eye_matrix(VR_EYE::RIGHT);

			uniforms.mvms[0] = mview_scene * mview_hmd * mview_eye_left;
			uniforms.mvms[1] = mview_scene * mview_hmd * mview_eye_right;
			uniforms.mvpms[0] = uniforms.mvms[0] * vr_ctx->get_projection_matrix(VR_EYE::LEFT, 0.25f, nbody_state.max_distance);
			uniforms.mvpms[1] = uniforms.mvms[1] * vr_ctx->get_projection_matrix(VR_EYE::RIGHT, 0.25f, nbody_state.max_distance);
#endif
		}
		
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
		renderer->commit();
	}

	// blit to screen
	{
		auto blitter = ctx.create_graphics_renderer(dev_queue, *blit_pass, *blit_pipeline, is_vr_renderer);
		if (!blitter->is_valid()) {
			return;
		}
		
		auto drawable = blitter->get_next_drawable(is_vr_renderer);
		if (drawable == nullptr || !drawable->is_valid()) {
			log_error("failed to get next drawable");
			return;
		}
		blitter->set_attachments(drawable);
		blitter->begin();
		
		static const vector<graphics_renderer::multi_draw_entry> blit_draw_info {{
			.vertex_count = 3 // fullscreen triangle
		}};
		blitter->multi_draw(blit_draw_info, render_image);
		
		blitter->end();
		blitter->present();
		blitter->commit();
	}
}
