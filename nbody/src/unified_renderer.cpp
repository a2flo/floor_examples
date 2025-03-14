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
static shared_ptr<compute_image> resolve_image;
static unique_ptr<graphics_pass> blit_pass;
static unique_ptr<graphics_pipeline> blit_pipeline;
static bool is_vr_renderer { false };
static event::handler resize_handler_fnctr = bind(&unified_renderer::resize_handler, placeholders::_1, placeholders::_2);

static const compute_context* ctx_ptr { nullptr };
static const compute_queue* dev_queue_ptr { nullptr };
static bool enable_msaa { false };
static constexpr const auto msaa_flags = COMPUTE_IMAGE_TYPE::FLAG_MSAA | COMPUTE_IMAGE_TYPE::FLAG_TRANSIENT | COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_4;
static constexpr const auto render_format = COMPUTE_IMAGE_TYPE::RGBA16F;

static array<shared_ptr<compute_image>, 2> body_textures {};
static void create_textures(const compute_context& ctx, const compute_queue& dev_queue) {
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
										{ (uint8_t*)pixel_data.data(), pixel_data.size() * sizeof(ushort4) },
										(COMPUTE_MEMORY_FLAG::READ |
										 COMPUTE_MEMORY_FLAG::HOST_WRITE |
										 COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS));
	}
}

bool unified_renderer::resize_handler(EVENT_TYPE type, shared_ptr<event_object>) {
	if (type == EVENT_TYPE::WINDOW_RESIZE) {
		// intermediate render output image
		const auto frame_dim = ctx_ptr->get_renderer_image_dim();
		render_image = ctx_ptr->create_image(*dev_queue_ptr, frame_dim,
											 render_format |
											 (!is_vr_renderer ? COMPUTE_IMAGE_TYPE::IMAGE_2D : COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY) |
											 (enable_msaa ? msaa_flags : COMPUTE_IMAGE_TYPE::READ) |
											 COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
											 COMPUTE_MEMORY_FLAG::READ_WRITE);
		render_image->set_debug_label("render_image");
		if (enable_msaa) {
			resolve_image = ctx_ptr->create_image(*dev_queue_ptr, frame_dim,
												  render_format |
												  (!is_vr_renderer ? COMPUTE_IMAGE_TYPE::IMAGE_2D : COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY) |
												  COMPUTE_IMAGE_TYPE::READ |
												  COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
												  COMPUTE_MEMORY_FLAG::READ_WRITE);
			resolve_image->set_debug_label("resolve_image");
		}
		return true;
	}
	return false;
}

bool unified_renderer::init(const compute_context& ctx, const compute_queue& dev_queue, const bool enable_msaa_,
							const compute_kernel& vs, const compute_kernel& fs,
							const compute_kernel* blit_vs, const compute_kernel* blit_fs, const compute_kernel* blit_fs_layered) {
	if (blit_vs == nullptr || blit_fs == nullptr || blit_fs_layered == nullptr) {
		log_error("missing blit shader(s)");
		return false;
	}

	enable_msaa = enable_msaa_;
	dev_queue_ptr = &dev_queue;
	ctx_ptr = &ctx;

	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);

	create_textures(ctx, dev_queue);

	is_vr_renderer = ctx.is_vr_supported();

	// nbody rendering pipeline/pass
	const render_pass_description pass_desc {
		.attachments = {
			{
				.format = render_format | (enable_msaa ? msaa_flags : COMPUTE_IMAGE_TYPE::NONE),
				.load_op = LOAD_OP::CLEAR,
				.store_op = (enable_msaa ? STORE_OP::RESOLVE : STORE_OP::STORE),
				.clear.color = { 0.0f, 0.0f, 0.0f, 0.0f },
			}
		},
		.debug_label = "nbody_render_pass",
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
		.sample_count = (enable_msaa ? 4 : 1),
		.depth = {
			.write = false,
			.compare = DEPTH_COMPARE::ALWAYS,
		},
		.color_attachments = {
			{
				.format = render_format | (enable_msaa ? msaa_flags : COMPUTE_IMAGE_TYPE::NONE),
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
		.debug_label = "nbody_render_pipeline",
	};
	renderer_pipeline = ctx.create_graphics_pipeline(pipeline_desc);
	if (!renderer_pipeline) {
		return false;
	}

	// trigger resize to create the output image
	resize_handler(EVENT_TYPE::WINDOW_RESIZE, nullptr);

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
		},
		.debug_label = "nbody_blit_pass",
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
		.debug_label = "nbody_blit_pipeline",
	};
	blit_pipeline = ctx.create_graphics_pipeline(blit_pipeline_desc);
	
	return true;
}

void unified_renderer::destroy(const compute_context& ctx floor_unused) {
	floor::get_event()->remove_event_handler(resize_handler_fnctr);

	for (auto& tex : body_textures) {
		tex = nullptr;
	}

	// clean up renderer stuff
	blit_pipeline = nullptr;
	blit_pass = nullptr;
	renderer_pipeline = nullptr;
	renderer_pass = nullptr;
	render_image = nullptr;
	resolve_image = nullptr;
}

void unified_renderer::render(const compute_context& ctx, const compute_queue& dev_queue, const compute_buffer& position_buffer) {
	// render scene
	{
		// create the renderer for this frame
		auto renderer = ctx.create_graphics_renderer(dev_queue, *renderer_pass, *renderer_pipeline, is_vr_renderer);
		if (!renderer->is_valid()) {
			return;
		}
		
		if (!resolve_image) {
			renderer->set_attachments(render_image);
		} else {
			graphics_renderer::resolve_and_store_attachment_t att { *render_image, *resolve_image };
			renderer->set_attachments(att);
		}
		
		// attachments set, can begin rendering now
		const auto render_dim = render_image->get_image_dim().xy;
		renderer->begin({
			.viewport = render_dim,
			.scissor = { { { 0u, 0u }, render_dim } },
		});

		// setup uniforms
		struct uniforms_t {
			matrix4f mvpms[2];
			matrix4f mvms[2];
			float2 mass_minmax;
		};

		uniforms_t uniforms {
			.mass_minmax = nbody_state.mass_minmax
		};
		const matrix4f mview_scene {
			nbody_state.cam_rotation.to_matrix4() *
			matrix4f::translation(0.0f, 0.0f, -nbody_state.distance)
		};
		if (!is_vr_renderer) {
			const matrix4f mproj {
				matrix4f::perspective(90.0f, render_image->get_aspect_ratio(),
									  0.25f, nbody_state.max_distance)
			};
			uniforms.mvpms[0] = mview_scene * mproj;
			uniforms.mvms[0] = mview_scene;
		} else {
			const auto vr_ctx = ctx.get_renderer_vr_context();
			const auto [hmd_pos, eye_distance, mv_left, mv_right, pm_left, pm_right] =
				vr_ctx->get_frame_view_state(0.25f, nbody_state.max_distance, false /* MV w/o position*/);
			uniforms.mvms[0] = mview_scene * matrix4f::translation(hmd_pos) * mv_left;
			uniforms.mvms[1] = mview_scene * matrix4f::translation(hmd_pos) * mv_right;
			uniforms.mvpms[0] = uniforms.mvms[0] * pm_left;
			uniforms.mvpms[1] = uniforms.mvms[1] * pm_right;
		}
		
		// draw
		static const graphics_renderer::multi_draw_entry nbody_draw_info {
			.vertex_count = nbody_state.body_count
		};
		renderer->draw(nbody_draw_info,
					   // vs args
					   position_buffer,
					   uniforms,
					   // fs args
					   body_textures[0]);
		renderer->end();
		renderer->commit_and_finish();
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
		const auto render_dim = drawable->image->get_image_dim().xy;
		blitter->begin({
			.viewport = render_dim,
			.scissor = { { { 0u, 0u }, render_dim } },
		});
		
		static const graphics_renderer::multi_draw_entry blit_draw_info {
			.vertex_count = 3 // fullscreen triangle
		};
		const auto hdr_scaler = (floor::get_hdr() && floor::get_hdr_linear() ? ctx.get_hdr_range_max() : 1.0f);
		blitter->draw(blit_draw_info, resolve_image ? resolve_image : render_image, hdr_scaler);
		
		blitter->end();
		blitter->present();
		blitter->commit_and_finish();
	}
}
