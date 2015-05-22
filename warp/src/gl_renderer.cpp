/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include "gl_renderer.hpp"
#include <floor/core/gl_shader.hpp>
#include <floor/core/timer.hpp>

static unordered_map<string, shared_ptr<floor_shader_object>> shader_objects;
static GLuint dummy_texture { 0u };
static GLuint vbo_fullscreen_triangle { 0 };
static struct {
	GLuint fbo { 0u };
	GLuint color { 0u };
	GLuint depth_as_color { 0u };
	GLuint depth { 0u };
	GLuint motion { 0u };
	
	GLuint compute_fbo { 0u };
	GLuint compute_color { 0u };
	
	int2 dim;
} scene_fbo;
static struct {
	GLuint fbo { 0u };
	GLuint shadow_tex { 0u };
#if defined(__APPLE__)
	int2 dim { 2048 };
#else
	// "anything worth doing, is worth overdoing!"
	int2 dim { 16384 };
#endif
} shadow_map;
static float3 light_pos;
static shared_ptr<compute_image> compute_color, compute_scene_color, compute_scene_depth, compute_scene_motion;
static matrix4f prev_mvm, prev_imvm, prev_mvpm, cur_imvm;
static constexpr const float4 clear_color { 0.215f, 0.412f, 0.6f, 0.0f };

static void create_textures() {
	// create/generate an opengl texture and bind it
	glGenTextures(1, &dummy_texture);
	{
		glBindTexture(GL_TEXTURE_2D, dummy_texture);
		
		// texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		// create texture
		static constexpr uint2 texture_size { 64, 64 };
		static constexpr float2 texture_sizef { texture_size };
		array<uint32_t, texture_size.x * texture_size.y> pixel_data;
		for(uint32_t y = 0; y < texture_size.y; ++y) {
			for(uint32_t x = 0; x < texture_size.x; ++x) {
				float2 dir = (float2(x, y) / texture_sizef) * 2.0f - 1.0f;
				float fval = dir.dot();
				uint32_t val = 255u - uint8_t(const_math::clamp(fval, 0.0f, 1.0f) * 255.0f);
				pixel_data[y * texture_size.x + x] = val + (val << 8u) + (val << 16u) + (val << 24u);
			}
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_size.x, texture_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixel_data[0]);
		
		// build mipmaps
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	
	// create scene fbo (TODO: resizable)
	{
		glGenFramebuffers(1, &scene_fbo.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo.fbo);
		
		scene_fbo.dim = { int(floor::get_width()), int(floor::get_height()) };
		
		glGenTextures(1, &scene_fbo.color);
		glBindTexture(GL_TEXTURE_2D, scene_fbo.color);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, scene_fbo.dim.x, scene_fbo.dim.y, 0,
					 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_fbo.color, 0);
		
		glGenTextures(1, &scene_fbo.depth_as_color);
		glBindTexture(GL_TEXTURE_2D, scene_fbo.depth_as_color);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene_fbo.dim.x, scene_fbo.dim.y, 0,
					 GL_RED, GL_FLOAT, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, scene_fbo.depth_as_color, 0);
		
		glGenTextures(1, &scene_fbo.motion);
		glBindTexture(GL_TEXTURE_2D, scene_fbo.motion);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, scene_fbo.dim.x, scene_fbo.dim.y, 0,
					 GL_RGBA, GL_FLOAT, nullptr);
		
		glGenTextures(1, &scene_fbo.depth);
		glBindTexture(GL_TEXTURE_2D, scene_fbo.depth);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#if 1
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, scene_fbo.dim.x, scene_fbo.dim.y, 0,
					 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
#else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, scene_fbo.dim.x, scene_fbo.dim.y, 0,
					 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
#endif
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, scene_fbo.depth, 0);
		
		//
		const auto err = glGetError();
		const auto fbo_err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(err != 0 || fbo_err != GL_FRAMEBUFFER_COMPLETE) {
			log_error("scene fbo/tex error: %X %X", err, fbo_err);
		}
		
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	
	// create compute scene fbo (TODO: resizable)
	{
		glGenFramebuffers(1, &scene_fbo.compute_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo.compute_fbo);
		
		glGenTextures(1, &scene_fbo.compute_color);
		glBindTexture(GL_TEXTURE_2D, scene_fbo.compute_color);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, scene_fbo.dim.x, scene_fbo.dim.y, 0,
					 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_fbo.compute_color, 0);
		
		//
		const auto err = glGetError();
		const auto fbo_err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(err != 0 || fbo_err != GL_FRAMEBUFFER_COMPLETE) {
			log_error("scene fbo/tex error: %X %X", err, fbo_err);
		}
		
		glClear(GL_COLOR_BUFFER_BIT);
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	
	// create shadow map fbo/tex
	{
		glGenFramebuffers(1, &shadow_map.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, shadow_map.fbo);
		
		glGenTextures(1, &shadow_map.shadow_tex);
		glBindTexture(GL_TEXTURE_2D, shadow_map.shadow_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, shadow_map.dim.x, shadow_map.dim.y, 0,
					 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_map.shadow_tex, 0);
		
		//
		const auto err = glGetError();
		const auto fbo_err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(err != 0 || fbo_err != GL_FRAMEBUFFER_COMPLETE) {
			log_error("shadow map fbo/tex error: %X %X", err, fbo_err);
		}
		
		//
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

static uint32_t global_vao { 0 };
bool gl_renderer::init() {
	floor::init_gl();
	glFrontFace(GL_CCW);
	glGenVertexArrays(1, &global_vao);
	glBindVertexArray(global_vao);
	
	// create fullscreen triangle/quad vbo
	static constexpr const float fullscreen_triangle[6] { 1.0f, 1.0f, 1.0f, -3.0f, -3.0f, 1.0f };
	glGenBuffers(1, &vbo_fullscreen_triangle);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_fullscreen_triangle);
	glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float2), fullscreen_triangle, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	create_textures();
	compute_color = warp_state.ctx->wrap_image(warp_state.dev, scene_fbo.compute_color, GL_TEXTURE_2D,
											   COMPUTE_MEMORY_FLAG::READ_WRITE |
											   COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE);
	compute_scene_color = warp_state.ctx->wrap_image(warp_state.dev, scene_fbo.color, GL_TEXTURE_2D,
													 COMPUTE_MEMORY_FLAG::READ |
													 COMPUTE_MEMORY_FLAG::OPENGL_READ);
	compute_scene_depth = warp_state.ctx->wrap_image(warp_state.dev, scene_fbo.depth_as_color, GL_TEXTURE_2D,
													 COMPUTE_MEMORY_FLAG::READ |
													 COMPUTE_MEMORY_FLAG::OPENGL_READ);
	compute_scene_motion = warp_state.ctx->wrap_image(warp_state.dev, scene_fbo.motion, GL_TEXTURE_2D,
													  COMPUTE_MEMORY_FLAG::READ |
													  COMPUTE_MEMORY_FLAG::OPENGL_READ);
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	
#if 0
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // pre-mul
	//glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); // add
#else
	glDisable(GL_BLEND);
#endif
	
	return compile_shaders();
}

void gl_renderer::destroy() {
	// only kill compute buffers here, let the driver/os handle the gl stuff
	compute_color = nullptr;
	compute_scene_color = nullptr;
	compute_scene_depth = nullptr;
	compute_scene_motion = nullptr;
}

bool gl_renderer::render(const obj_model& model,
						 const camera& cam) {
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
	
	
	//
	static constexpr const float frame_limit { 1.0f / 10.0f };
	static size_t warp_frame_num = 0;
	if(deltaf < frame_limit) {
		if(warp_state.is_warping) {
			render_kernels(cam, deltaf, render_delta, warp_frame_num);
			blit(false);
			++warp_frame_num;
		}
		else blit(true);
		return false;
	}
	else {
		render_delta = deltaf;
		time_keeper = now;
		warp_frame_num = 0;
		
		compute_scene_color->acquire_opengl_object(warp_state.dev_queue);
		compute_scene_depth->acquire_opengl_object(warp_state.dev_queue);
		compute_scene_motion->acquire_opengl_object(warp_state.dev_queue);
		
		render_full_scene(model, cam);
		
		compute_scene_color->release_opengl_object(warp_state.dev_queue);
		compute_scene_depth->release_opengl_object(warp_state.dev_queue);
		compute_scene_motion->release_opengl_object(warp_state.dev_queue);
		
		blit(warp_state.is_render_full);
		return true;
	}
}

void gl_renderer::blit(const bool full_scene) {
	if(full_scene) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo.fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
						  0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
						  GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	else {
		compute_color->acquire_opengl_object(warp_state.dev_queue);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo.compute_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
						  0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
						  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		compute_color->release_opengl_object(warp_state.dev_queue);
	}
}

void gl_renderer::render_kernels(const camera& cam,
								 const float& delta, const float& render_delta,
								 const size_t& warp_frame_num) {
	const matrix4f mproj { matrix4f().perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
												  0.5f, warp_state.view_distance) };
	
//#define WARP_TIMING
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	const auto timing_start = floor_timer2::start();
#endif
	
	// clear if enabled + always clear the first frame
	if(warp_state.is_clear_frame || (warp_frame_num == 0 && warp_state.is_fixup)) {
		warp_state.dev_queue->execute(warp_state.clear_kernel,
									  size2 { scene_fbo.dim },
									  size2 { 32, 32 },
									  compute_color, clear_color);
	}
	warp_state.dev_queue->execute(warp_state.warp_kernel,
								  size2 { scene_fbo.dim },
								  size2 { 32, 32 },
								  compute_scene_color, compute_scene_depth, compute_scene_motion, compute_color,
								  delta / render_delta, mproj,
								  (!warp_state.is_single_frame ?
								   float4 { -1.0f } :
								   float4 { cam.get_single_frame_direction(), 1.0f }
								   ));
	if(warp_state.is_fixup) {
		if(warp_state.ctx->get_compute_type() == COMPUTE_TYPE::CUDA) {
			warp_state.dev_queue->execute(warp_state.fixup_kernel,
										  size2 { scene_fbo.dim },
										  size2 { 32, 32 },
										  compute_color);
		}
		else {
			warp_state.dev_queue->execute(warp_state.fixup_kernel,
										  size2 { scene_fbo.dim },
										  size2 { 32, 32 },
										  compute_color, compute_color);
		}
	}
	
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	log_debug("warp timing: %f", double(floor_timer2::stop<chrono::microseconds>(timing_start)) / 1000.0);
#endif
}

void gl_renderer::render_full_scene(const obj_model& model, const camera& cam) {
	//
	glBindVertexArray(global_vao);
	
	static constexpr const GLenum draw_buffers[] {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
	};
	
	matrix4f light_bias_mvpm;
	if(!warp_state.is_single_frame) {
		// draw shadow map
		const matrix4f light_mproj { matrix4f().perspective(120.0f, 1.0f, 1.0f, light_pos.y + 10.0f) };
		const matrix4f light_mview {
			matrix4f().translate(-light_pos.x, -light_pos.y, -light_pos.z) *
			matrix4f().rotate_x(90.0f) // rotate downwards
		};
		const matrix4f light_mvpm { light_mview * light_mproj };
		light_bias_mvpm = matrix4f {
			light_mvpm * matrix4f().scale(0.5f, 0.5f, 0.5f).translate(0.5f, 0.5f, 0.5f)
		};
		
		glBindFramebuffer(GL_FRAMEBUFFER, shadow_map.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_map.shadow_tex, 0);
		glViewport(0, 0, shadow_map.dim.x, shadow_map.dim.y);
		glDrawBuffer(GL_NONE);
		glClear(GL_DEPTH_BUFFER_BIT);
		
		const auto shadow_shd = shader_objects["SHADOW"];
		glUseProgram(shadow_shd->program.program);
		
		glUniformMatrix4fv(shadow_shd->program.uniforms["mvpm"].location, 1, false, &light_mvpm.data[0]);
		
		glBindBuffer(GL_ARRAY_BUFFER, model.vertices_vbo);
		const GLuint shdw_vertices_location = (GLuint)shadow_shd->program.attributes["in_vertex"].location;
		glEnableVertexAttribArray(shdw_vertices_location);
		glVertexAttribPointer(shdw_vertices_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		for(auto& obj : model.objects) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->indices_vbo);
			glDrawElements(GL_TRIANGLES, obj->triangle_count, GL_UNSIGNED_INT, nullptr);
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
		// draw main scene
		glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, scene_fbo.motion, 0);
		glDrawBuffers(size(draw_buffers), &draw_buffers[0]);
		glViewport(0, 0, (GLsizei)floor::get_width(), (GLsizei)floor::get_height());
		
		// clear the color/depth buffer
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	
	// debug
#if 0
	{
		glFrontFace(GL_CW);
		const auto dbg_shd = shader_objects["SHADOW_DBG"];
		glUseProgram(dbg_shd->program.program);
		
		glUniform1i(dbg_shd->program.uniforms["tex"].location, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, shadow_map.shadow_tex);
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo_fullscreen_triangle);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(0);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glFrontFace(GL_CCW);
	}
#endif
	
	// render actual scene
#if 1
	const auto shd = shader_objects[warp_state.is_single_frame ? "MOTION_ONLY" : "SCENE"];
	glUseProgram(shd->program.program);
	
	const matrix4f mproj { matrix4f().perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
												  0.5f, warp_state.view_distance) };
	const matrix4f rot_mat { matrix4f().rotate_y(cam.get_rotation().y) * matrix4f().rotate_x(cam.get_rotation().x) };
	const matrix4f mview { matrix4f().translate(cam.get_position().x, cam.get_position().y, cam.get_position().z) * rot_mat };

	glUniformMatrix4fv(shd->program.uniforms["mvm"].location, 1, false, &mview.data[0]);
	glUniformMatrix4fv(shd->program.uniforms["prev_mvm"].location, 1, false, &prev_mvm.data[0]);
	prev_mvm = mview;
	prev_imvm = mview;
	prev_imvm.invert();
	const matrix4f mvpm { mview * mproj };
	prev_mvpm = mvpm;
	glUniformMatrix4fv(shd->program.uniforms["mvpm"].location, 1, false, &mvpm.data[0]);
	
	if(!warp_state.is_single_frame) {
		glUniformMatrix4fv(shd->program.uniforms["light_bias_mvpm"].location, 1, false, &light_bias_mvpm.data[0]);
		
		const float3 cam_pos { -cam.get_position() };
		glUniform3fv(shd->program.uniforms["cam_pos"].location, 1, cam_pos.data());
		
		glUniform3fv(shd->program.uniforms["light_pos"].location, 1, light_pos.data());
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, model.vertices_vbo);
	const GLuint vertices_location = (GLuint)shd->program.attributes["in_vertex"].location;
	glEnableVertexAttribArray(vertices_location);
	glVertexAttribPointer(vertices_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	glBindBuffer(GL_ARRAY_BUFFER, model.tex_coords_vbo);
	const GLuint tex_coords_location = (GLuint)shd->program.attributes["in_tex_coord"].location;
	glEnableVertexAttribArray(tex_coords_location);
	glVertexAttribPointer(tex_coords_location, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	if(!warp_state.is_single_frame) {
		glBindBuffer(GL_ARRAY_BUFFER, model.normals_vbo);
		const GLuint normals_location = (GLuint)shd->program.attributes["in_normal"].location;
		glEnableVertexAttribArray(normals_location);
		glVertexAttribPointer(normals_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		glBindBuffer(GL_ARRAY_BUFFER, model.binormals_vbo);
		const GLuint binormals_location = (GLuint)shd->program.attributes["in_binormal"].location;
		glEnableVertexAttribArray(binormals_location);
		glVertexAttribPointer(binormals_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		glBindBuffer(GL_ARRAY_BUFFER, model.tangents_vbo);
		const GLuint tangents_location = (GLuint)shd->program.attributes["in_tangent"].location;
		glEnableVertexAttribArray(tangents_location);
		glVertexAttribPointer(tangents_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

		const GLint shadow_loc { shd->program.uniforms["shadow_tex"].location };
		const GLint shadow_num { shd->program.samplers["shadow_tex"] };
		glUniform1i(shadow_loc, shadow_num);
		glActiveTexture((GLenum)(GL_TEXTURE0 + shadow_num));
		glBindTexture(GL_TEXTURE_2D, shadow_map.shadow_tex);
	}
	else {
		glDrawBuffers(1, &draw_buffers[0]);
	}
	
	const GLint diff_loc { shd->program.uniforms["diff_tex"].location };
	const GLint diff_num { shd->program.samplers["diff_tex"] };
	const GLint spec_loc { shd->program.uniforms["spec_tex"].location };
	const GLint spec_num { shd->program.samplers["spec_tex"] };
	const GLint norm_loc { shd->program.uniforms["norm_tex"].location };
	const GLint norm_num { shd->program.samplers["norm_tex"] };
	const GLint mask_loc { shd->program.uniforms["mask_tex"].location };
	const GLint mask_num { shd->program.samplers["mask_tex"] };
	for(const auto& obj : model.objects) {
		if(!warp_state.is_single_frame) {
			glUniform1i(diff_loc, diff_num);
			glActiveTexture((GLenum)(GL_TEXTURE0 + diff_num));
			glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].diffuse);
			
			glUniform1i(spec_loc, spec_num);
			glActiveTexture((GLenum)(GL_TEXTURE0 + spec_num));
			glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].specular);
			
			glUniform1i(norm_loc, norm_num);
			glActiveTexture((GLenum)(GL_TEXTURE0 + norm_num));
			glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].normal);
		}
		
		glUniform1i(mask_loc, mask_num);
		glActiveTexture((GLenum)(GL_TEXTURE0 + mask_num));
		glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].mask);
		
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->indices_vbo);
		glDrawElements(GL_TRIANGLES, obj->triangle_count, GL_UNSIGNED_INT, nullptr);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glUseProgram(0);
	glDrawBuffers(1, &draw_buffers[0]); // reset to 1 draw buffer
#endif
}

bool gl_renderer::compile_shaders() {
	static const char motion_only_vs_text[] { u8R"RAWSTR(#version 150 core
		uniform mat4 mvpm;
		uniform mat4 mvm;
		uniform mat4 prev_mvm;
		
		in vec2 in_tex_coord;
		in vec3 in_vertex;
		
		out vec2 tex_coord;
		out vec4 motion;
		
		void main() {
			tex_coord = in_tex_coord;
			
			vec4 vertex = vec4(in_vertex.xyz, 1.0);
			gl_Position = mvpm * vertex;
			
			vec4 prev_pos = prev_mvm * vertex;
			vec4 cur_pos = mvm * vertex;
			motion = cur_pos - prev_pos;
		}
	)RAWSTR"};
	static const char motion_only_fs_text[] { u8R"RAWSTR(#version 150 core
		uniform sampler2D mask_tex;
		
		in vec2 tex_coord;
		in vec4 motion;
		
		out vec4 motion_color;
		
		void main() {
			// still need to properly handle this
			float mask = textureLod(mask_tex, tex_coord, 0).x;
			if(mask < 0.5) discard;
			
			motion_color = motion;
		}
	)RAWSTR"};
	static const char scene_vs_text[] { u8R"RAWSTR(#version 150 core
		uniform mat4 mvpm;
		uniform mat4 mvm;
		uniform mat4 prev_mvm;
		uniform mat4 prev_mvpm;
		uniform mat4 light_bias_mvpm;
		uniform vec3 cam_pos;
		uniform vec3 light_pos;
		
		in vec3 in_vertex;
		in vec2 in_tex_coord;
		in vec3 in_normal;
		in vec3 in_binormal;
		in vec3 in_tangent;
		
		out vec2 tex_coord;
		out vec4 shadow_coord;
		out vec3 view_dir;
		out vec3 light_dir;
		out vec4 motion;
		
		void main() {
			tex_coord = in_tex_coord;
			
			vec4 vertex = vec4(in_vertex.xyz, 1.0);
			shadow_coord = light_bias_mvpm * vertex;
			gl_Position = mvpm * vertex;
			
			vec4 prev_pos = prev_mvm * vertex;
			vec4 cur_pos = mvm * vertex;
			motion = cur_pos - prev_pos;
			
			vec3 vview = cam_pos - in_vertex;
			view_dir.x = dot(vview, in_tangent);
			view_dir.y = dot(vview, in_binormal);
			view_dir.z = dot(vview, in_normal);
			
			vec3 vlight = light_pos - in_vertex;
			light_dir.x = dot(vlight, in_tangent);
			light_dir.y = dot(vlight, in_binormal);
			light_dir.z = dot(vlight, in_normal);
		}
	)RAWSTR"};
	static const char scene_fs_text[] { u8R"RAWSTR(#version 150 core
		uniform sampler2D diff_tex;
		uniform sampler2D spec_tex;
		uniform sampler2D norm_tex;
		uniform sampler2D mask_tex;
		uniform sampler2DShadow shadow_tex;
		
		in vec2 tex_coord;
		in vec4 shadow_coord;
		in vec3 view_dir;
		in vec3 light_dir;
		in vec4 motion;
		
		out vec4 frag_color;
		out float frag_depth;
		out vec4 motion_color;
		
		// props to https://code.google.com/p/opengl-tutorial-org/source/browse/#hg%2Ftutorial16_shadowmaps for this
		const vec2 poisson_disk[16] = vec2[](vec2(-0.94201624, -0.39906216),
											 vec2(0.94558609, -0.76890725),
											 vec2(-0.094184101, -0.92938870),
											 vec2(0.34495938, 0.29387760),
											 vec2(-0.91588581, 0.45771432),
											 vec2(-0.81544232, -0.87912464),
											 vec2(-0.38277543, 0.27676845),
											 vec2(0.97484398, 0.75648379),
											 vec2(0.44323325, -0.97511554),
											 vec2(0.53742981, -0.47373420),
											 vec2(-0.26496911, -0.41893023),
											 vec2(0.79197514, 0.19090188),
											 vec2(-0.24188840, 0.99706507),
											 vec2(-0.81409955, 0.91437590),
											 vec2(0.19984126, 0.78641367),
											 vec2(0.14383161, -0.14100790));
		
		void main() {
			float mask = textureLod(mask_tex, tex_coord, 0).x;
			if(mask < 0.5) discard;
			
			vec4 diff = texture(diff_tex, tex_coord);
			
			//
			const float fixed_bias = 0.00006;
			const float ambient = 0.2;
			const float attenuation = 0.9;
			float lighting = 0.0;
			float light_vis = 1.0;
			
			//
			vec3 norm_view_dir = normalize(view_dir);
			vec3 norm_light_dir = normalize(light_dir);
			vec3 norm_half_dir = normalize(norm_view_dir + norm_light_dir);
			vec3 normal = texture(norm_tex, tex_coord).xyz * 2.0 - 1.0;
			
			float lambert = dot(normal, norm_light_dir);
			if(lambert > 0.0) {
				// shadow hackery
				float bias_lambert = min(lambert, 0.99995); // clamp to "(0, 1)", already > 0 here
				float slope = sqrt(1.0 - bias_lambert * bias_lambert) / bias_lambert; // = tan(acos(lambert))
				float bias = clamp(fixed_bias * slope, 0.0, fixed_bias * 2.0);
				vec3 coord = shadow_coord.xyz / shadow_coord.w;
#if 0
				if(shadow_coord.w > 0.0) {
					light_vis = texture(shadow_tex, vec3(coord.xy, coord.z - bias));
				}
#else
				for(int i = 0; i < 16; ++i) {
					light_vis -= 0.05 * (1.0 - texture(shadow_tex, vec3(coord.xy + poisson_disk[i] / 2048.0,
																		coord.z - bias)));
				}
#endif
				
				// diffuse
				lighting += lambert;
				
				// specular
				float spec = texture(spec_tex, tex_coord).x;
				float specular = pow(max(dot(norm_half_dir, normal), 0.0), spec * 10.0);
				lighting += specular;
				
				// mul with shadow and light attenuation
				lighting *= light_vis * attenuation;
			}
			lighting = max(lighting, ambient);
			
			frag_color = vec4(diff.xyz * lighting, 0.0);
			//frag_depth = gl_FragCoord.z; // normalized depth
			frag_depth = gl_FragCoord.z / gl_FragCoord.w; // linear depth
			
			motion_color = motion;
		}
	)RAWSTR"};
	static const char shadow_map_vs_text[] { u8R"RAWSTR(#version 150 core
		uniform mat4 mvpm;
		in vec3 in_vertex;
		void main() {
			gl_Position = mvpm * vec4(in_vertex.xyz, 1.0);
		}
	)RAWSTR"};
	static const char shadow_map_fs_text[] { u8R"RAWSTR(#version 150 core
		out float frag_depth;
		void main() {
			frag_depth = gl_FragCoord.z;
		}
	)RAWSTR"};
	static const char shadow_dbg_vs_text[] { u8R"RAWSTR(#version 150 core
		in vec2 in_vertex;
		out vec2 tex_coord;
		void main() {
			tex_coord = in_vertex.xy * 0.5 + 0.5;
			gl_Position = vec4(in_vertex.xy, 0.0, 1.0);
		}
	)RAWSTR" };
	static const char shadow_dbg_fs_text[] { u8R"RAWSTR(#version 150 core
		uniform sampler2DShadow tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			float val = mix(mix(texture(tex, vec3(tex_coord, 0.5)),
								texture(tex, vec3(tex_coord, 0.8)), 0.5),
							mix(texture(tex, vec3(tex_coord, 0.999)),
								texture(tex, vec3(tex_coord, 0.9999)), 0.5), 0.5);
			frag_color = vec4(vec3(val), 1.0);
		}
	)RAWSTR" };
	
	{
		auto shd = make_shared<floor_shader_object>("SCENE");
		if(!floor_compile_shader(*shd.get(), scene_vs_text, scene_fs_text)) return false;
		shader_objects.emplace(shd->name, shd);
	}
	{
		auto shd = make_shared<floor_shader_object>("MOTION_ONLY");
		if(!floor_compile_shader(*shd.get(), motion_only_vs_text, motion_only_fs_text)) return false;
		shader_objects.emplace(shd->name, shd);
	}
	{
		auto shd = make_shared<floor_shader_object>("SHADOW");
		if(!floor_compile_shader(*shd.get(), shadow_map_vs_text, shadow_map_fs_text)) return false;
		shader_objects.emplace(shd->name, shd);
	}
	{
		auto shd = make_shared<floor_shader_object>("SHADOW_DBG");
		if(!floor_compile_shader(*shd.get(), shadow_dbg_vs_text, shadow_dbg_fs_text)) return false;
		shader_objects.emplace(shd->name, shd);
	}
	return true;
}
