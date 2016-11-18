/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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
#include <libwarp/libwarp.h>

#if !defined(FLOOR_IOS)

static unordered_map<string, floor_shader_object> shader_objects;
static GLuint vbo_fullscreen_triangle { 0 };
static struct {
	GLuint fbo[2] { 0u, 0u };
	GLuint color[2] { 0u, 0u };
	GLuint depth[2] { 0u, 0u };
	// if gather: { forward, backward }, { forward, backward }
	GLuint motion[4] { 0u, 0u, 0u, 0u };
	// if gather: packed 2x { forward, backward }
	GLuint motion_depth[2] { 0u, 0u };
	// separate z/w depth buffer to workaround depth buffer sharing issues (this is a color texture!)
	GLuint depth_zw[2] { 0u, 0u };
	
	GLuint compute_fbo { 0u };
	GLuint compute_color { 0u };
	
	int2 dim;
	uint2 dim_multiple;
} scene_fbo;
static struct {
	GLuint fbo { 0u };
	GLuint shadow_tex { 0u };
	int2 dim { 2048 };
} shadow_map;
static GLuint skybox_tex { 0u };
static float3 light_pos;
static shared_ptr<compute_image> compute_color;
static matrix4f prev_mvm, prev_prev_mvm;
static matrix4f prev_rmvm, prev_prev_rmvm;
static constexpr const float4 clear_color { 0.0f };
static bool first_frame { true };

static void destroy_textures() {
	// kill old shared/wrapped compute images (also makes sure these don't access the gl objects any more)
	compute_color = nullptr;
	
	// kill gl stuff
	for(auto& fbo : scene_fbo.fbo) {
		if(fbo > 0) {
			glDeleteFramebuffers(1, &fbo);
			fbo = 0;
		}
	}
	for(auto& tex : scene_fbo.color) {
		if(tex > 0) {
			glDeleteTextures(1, &tex);
			tex = 0;
		}
	}
	for(auto& tex : scene_fbo.motion) {
		if(tex > 0) {
			glDeleteTextures(1, &tex);
			tex = 0;
		}
	}
	for(auto& tex : scene_fbo.motion_depth) {
		if(tex > 0) {
			glDeleteTextures(1, &tex);
			tex = 0;
		}
	}
	for(auto& tex : scene_fbo.depth) {
		if(tex > 0) {
			glDeleteTextures(1, &tex);
			tex = 0;
		}
	}
	for(auto& tex : scene_fbo.depth_zw) {
		if(tex > 0) {
			glDeleteTextures(1, &tex);
			tex = 0;
		}
	}
	
	if(scene_fbo.compute_fbo > 0) {
		glDeleteFramebuffers(1, &scene_fbo.compute_fbo);
		scene_fbo.compute_fbo = 0;
	}
	if(scene_fbo.compute_color > 0) {
		glDeleteTextures(1, &scene_fbo.compute_color);
		scene_fbo.compute_color = 0;
	}
}

static void create_textures() {
	// create scene fbo
	{
		scene_fbo.dim = floor::get_physical_screen_size();
		
		// kernel work-group size is { 32, 16 } -> round global size to a multiple of it
		scene_fbo.dim_multiple = scene_fbo.dim.rounded_next_multiple(warp_state.tile_size);
		
		for(size_t i = 0, count = size(scene_fbo.fbo); i < count; ++i) {
			glGenFramebuffers(1, &scene_fbo.fbo[i]);
			glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo.fbo[i]);
			
			glGenTextures(1, &scene_fbo.color[i]);
			glBindTexture(GL_TEXTURE_2D, scene_fbo.color[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, scene_fbo.dim.x, scene_fbo.dim.y, 0,
						 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_fbo.color[i], 0);
			
			for(size_t j = 0; j < 2; ++j) {
				glGenTextures(1, &scene_fbo.motion[i * 2 + j]);
				glBindTexture(GL_TEXTURE_2D, scene_fbo.motion[i * 2 + j]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, scene_fbo.dim.x, scene_fbo.dim.y, 0,
							 GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 + (GLenum)j, GL_TEXTURE_2D,
									   scene_fbo.motion[i * 2 + j], 0);
			}
			glGenTextures(1, &scene_fbo.motion_depth[i]);
			glBindTexture(GL_TEXTURE_2D, scene_fbo.motion_depth[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, scene_fbo.dim.x, scene_fbo.dim.y, 0,
						 GL_RG, GL_HALF_FLOAT, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, scene_fbo.motion_depth[i], 0);
			
			if(warp_state.is_zw_depth) {
				glGenTextures(1, &scene_fbo.depth_zw[i]);
				glBindTexture(GL_TEXTURE_2D, scene_fbo.depth_zw[i]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene_fbo.dim.x, scene_fbo.dim.y, 0,
							 GL_RED, GL_FLOAT, nullptr);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, scene_fbo.depth_zw[i], 0);
			}
			
			glGenTextures(1, &scene_fbo.depth[i]);
			glBindTexture(GL_TEXTURE_2D, scene_fbo.depth[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, scene_fbo.dim.x, scene_fbo.dim.y, 0,
						 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, scene_fbo.depth[i], 0);
			
			//
			const auto err = glGetError();
			const auto fbo_err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if(err != 0 || fbo_err != GL_FRAMEBUFFER_COMPLETE) {
				log_error("scene fbo/tex error: %X %X", err, fbo_err);
			}
			
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	
	// create compute scene fbo
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
}

static void create_skybox() {
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
	
	// now create/generate an opengl texture and bind it
	glGenTextures(1, &skybox_tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);
	
	// texture parameters
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	
	static const GLenum cubemap_enums[size(skybox_filenames)] {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};
	
	for(size_t i = 0, count = size(cubemap_enums); i < count; ++i) {
		glTexImage2D(cubemap_enums[i], 0, GL_RGB,
					 skybox_surfaces[i]->w, skybox_surfaces[i]->h,
					 0, GL_RGB, GL_UNSIGNED_BYTE,
					 skybox_surfaces[i]->pixels);
	}
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static void destroy_skybox() {
	if(skybox_tex > 0) {
		glDeleteTextures(1, &skybox_tex);
		skybox_tex = 0;
	}
}

static bool resize_handler(EVENT_TYPE type, shared_ptr<event_object>) {
	if(type == EVENT_TYPE::WINDOW_RESIZE) {
		destroy_textures();
		create_textures();
		first_frame = true;
		return true;
	}
	return false;
}
static event::handler resize_handler_fnctr(&resize_handler);

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
	
	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
	create_textures();
	create_skybox();
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	
	glDisable(GL_BLEND);
	
	return compile_shaders();
}

void gl_renderer::destroy() {
	destroy_textures();
	destroy_skybox();
}

bool gl_renderer::render(const gl_obj_model& model,
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
	static const float frame_limit { 1.0f / float(warp_state.render_frame_count) };
	static size_t warp_frame_num = 0;
	if((deltaf < frame_limit && !first_frame) || warp_state.is_frame_repeat) {
		if(deltaf >= frame_limit) { // need to reset when over the limit
			render_delta = deltaf;
			time_keeper = now;
			warp_frame_num = 0;
		}
		
		if(warp_state.is_warping) {
			render_kernels(deltaf, render_delta, warp_frame_num);
			blit(false);
			++warp_frame_num;
		}
		else blit(true);
		return false;
	}
	else {
		first_frame = false;
		render_delta = deltaf;
		time_keeper = now;
		warp_frame_num = 0;
		
		render_full_scene(model, cam);
		
		// on opencl: need to make sure that the depth color buffer has finished drawing (oddly enough not needed for other color images)
		if(warp_state.is_zw_depth &&
		   warp_state.ctx->get_compute_type() == COMPUTE_TYPE::OPENCL) {
			glFinish();
		}
		
		blit(warp_state.is_render_full);
		
		return true;
	}
}

void gl_renderer::blit(const bool full_scene) {
	if(!warp_state.is_split_view) {
		if(full_scene) {
			// if gather: this is the previous frame (i.e. if we are at time t and have just rendered I_t, this blits I_t-1)
			glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo.fbo[warp_state.cur_fbo]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
							  0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
							  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		else {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo.compute_fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
							  0, 0, scene_fbo.dim.x, scene_fbo.dim.y,
							  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	}
	else {
		// split view rendering
		// clear first, so that we have a black bar in the middle
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		// right side is always the original scene
		glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo.fbo[warp_state.cur_fbo]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(scene_fbo.dim.x / 2 + 2, 0, scene_fbo.dim.x, scene_fbo.dim.y,
						  scene_fbo.dim.x / 2 + 2, 0, scene_fbo.dim.x, scene_fbo.dim.y,
						  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// left side: either the original scene (for full frames), or the warped frame (for in-between)
		if(full_scene) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo.fbo[warp_state.cur_fbo]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, 0, scene_fbo.dim.x / 2 - 2, scene_fbo.dim.y,
							  0, 0, scene_fbo.dim.x / 2 - 2, scene_fbo.dim.y,
							  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		else {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo.compute_fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, 0, scene_fbo.dim.x / 2 - 2, scene_fbo.dim.y,
							  0, 0, scene_fbo.dim.x / 2 - 2, scene_fbo.dim.y,
							  GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	}
}

void gl_renderer::render_kernels(const float& delta, const float& render_delta,
								 const size_t& warp_frame_num) {
//#define WARP_TIMING
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	const auto timing_start = floor_timer2::start();
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
		.depth_type = (!warp_state.is_zw_depth ? LIBWARP_DEPTH_NORMALIZED : LIBWARP_DEPTH_Z_DIV_W),
	};
	const uint32_t* depth_buffers = (!warp_state.is_zw_depth ? scene_fbo.depth : scene_fbo.depth_zw);
	LIBWARP_ERROR_CODE err = LIBWARP_SUCCESS;
	if(warp_state.is_scatter) {
		err = libwarp_scatter(&cam_setup, relative_delta, is_first_frame && warp_state.is_clear_frame,
							  scene_fbo.color[0], depth_buffers[0], scene_fbo.motion[0],
							  scene_fbo.compute_color);
	}
	else {
		if(warp_state.is_gather_forward) {
			err = libwarp_gather_forward_only(&cam_setup, relative_delta,
											  scene_fbo.color[0], scene_fbo.motion[0],
											  scene_fbo.compute_color);
		}
		else {
			err = libwarp_gather(&cam_setup, relative_delta,
								 scene_fbo.color[1u - warp_state.cur_fbo],
								 depth_buffers[1u - warp_state.cur_fbo],
								 scene_fbo.color[warp_state.cur_fbo],
								 depth_buffers[warp_state.cur_fbo],
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
	log_debug("warp timing: %f", double(floor_timer2::stop<chrono::microseconds>(timing_start)) / 1000.0);
#endif
}

void gl_renderer::render_full_scene(const gl_obj_model& model, const camera& cam) {
	//
	glBindVertexArray(global_vao);
	
	// draw shadow map
	const matrix4f light_pm { matrix4f().perspective(120.0f, 1.0f,
													 warp_state.shadow_near_far_plane.x,
													 warp_state.shadow_near_far_plane.y) };
	const matrix4f light_mvm {
		matrix4f::translation(-light_pos) *
		matrix4f::rotation_deg_named<'x'>(90.0f) // rotate downwards
	};
	const matrix4f light_mvpm { light_mvm * light_pm };
	matrix4f light_bias_mvpm {
		light_mvpm * matrix4f().scale(0.5f, 0.5f, 0.5f).translate(0.5f, 0.5f, 0.5f)
	};
	
	glBindFramebuffer(GL_FRAMEBUFFER, shadow_map.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_map.shadow_tex, 0);
	glViewport(0, 0, shadow_map.dim.x, shadow_map.dim.y);
	glDrawBuffer(GL_NONE);
	glClear(GL_DEPTH_BUFFER_BIT);
	
	auto& shadow_shd = shader_objects["SHADOW"];
	glUseProgram(shadow_shd.program.program);
	
	glUniformMatrix4fv(shadow_shd.program.uniforms["mvpm"].location, 1, false, &light_mvpm.data[0]);
	
	glBindBuffer(GL_ARRAY_BUFFER, model.vertices_vbo);
	const GLuint shdw_vertices_location = (GLuint)shadow_shd.program.attributes["in_vertex"].location;
	glEnableVertexAttribArray(shdw_vertices_location);
	glVertexAttribPointer(shdw_vertices_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	for(auto& obj : model.objects) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->indices_gl_vbo);
		glDrawElements(GL_TRIANGLES, obj->index_count, GL_UNSIGNED_INT, nullptr);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	// draw main scene
	glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo.fbo[warp_state.cur_fbo]);
	int32_t draw_buffer_count = 0;
	const GLenum* draw_buffers = nullptr;
	if(!warp_state.is_scatter) {
		// gather: always 4 attachments + 1 if z/w depth
		static constexpr const GLenum gather_draw_buffers[] {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2,
			GL_COLOR_ATTACHMENT3,
			GL_COLOR_ATTACHMENT4,
		};
		draw_buffer_count = (!warp_state.is_zw_depth ? 4 : 5);
		draw_buffers = &gather_draw_buffers[0];
	}
	else {
		// normal scatter: always 2 attachments + 1 if z/w depth
		if(!warp_state.is_bidir_scatter) {
			static constexpr const GLenum scatter_draw_buffers[] {
				GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1,
				GL_COLOR_ATTACHMENT4,
			};
			draw_buffer_count = (!warp_state.is_zw_depth ? 2 : 3);
			draw_buffers = &scatter_draw_buffers[0];
		}
		// bidir scatter: always 3 attachments + 1 if z/w depth
		else {
			static constexpr const GLenum bidir_scatter_draw_buffers[] {
				GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1,
				GL_COLOR_ATTACHMENT2,
				GL_COLOR_ATTACHMENT4,
			};
			draw_buffer_count = (!warp_state.is_zw_depth ? 3 : 4);
			draw_buffers = &bidir_scatter_draw_buffers[0];
		}
	}
	glDrawBuffers(draw_buffer_count, draw_buffers);
	glViewport(0, 0, scene_fbo.dim.x, scene_fbo.dim.y);
	
	// clear the color/depth buffer
	glClearDepthf(1.0f);
	glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// render actual scene
	auto& shd = shader_objects[!warp_state.is_scatter ? "SCENE_GATHER" :
							   !warp_state.is_bidir_scatter ? "SCENE_SCATTER" : "SCENE_SCATTER_BIDIR"];
	glUseProgram(shd.program.program);
	
	const matrix4f pm { matrix4f().perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
											   warp_state.near_far_plane.x, warp_state.near_far_plane.y) };
	const matrix4f rmvm {
		matrix4f::rotation_deg_named<'y'>(cam.get_rotation().y) *
		matrix4f::rotation_deg_named<'x'>(cam.get_rotation().x)
	};
	const matrix4f mvm { matrix4f::translation(cam.get_position() * float3 { 1.0f, -1.0f, 1.0f }) * rmvm };

	matrix4f mvpm;
	if(warp_state.is_scatter) {
		if(!warp_state.is_bidir_scatter) {
			glUniformMatrix4fv(shd.program.uniforms["mvm"].location, 1, false, &mvm.data[0]);
			glUniformMatrix4fv(shd.program.uniforms["prev_mvm"].location, 1, false, &prev_mvm.data[0]);
			
#if defined(FORWARD_PROJECTION)
			mvpm = mvm * pm;
#else
			mvpm = prev_mvm * pm;
#endif
			glUniformMatrix4fv(shd.program.uniforms["mvpm"].location, 1, false, &mvpm.data[0]);
		}
		else {
			// bidirectional
			mvpm = prev_mvm * pm;
			glUniformMatrix4fv(shd.program.uniforms["next_mvm"].location, 1, false, &mvm.data[0]);
			glUniformMatrix4fv(shd.program.uniforms["mvm"].location, 1, false, &prev_mvm.data[0]);
			glUniformMatrix4fv(shd.program.uniforms["prev_mvm"].location, 1, false, &prev_prev_mvm.data[0]);
			glUniformMatrix4fv(shd.program.uniforms["mvpm"].location, 1, false, &mvpm.data[0]);
		}
	}
	else {
		mvpm = prev_mvm * pm;
		const matrix4f next_mvpm { mvm * pm };
		const matrix4f prev_mvpm { prev_prev_mvm * pm };
		glUniformMatrix4fv(shd.program.uniforms["mvpm"].location, 1, false, &mvpm.data[0]);
		glUniformMatrix4fv(shd.program.uniforms["next_mvpm"].location, 1, false, &next_mvpm.data[0]);
		glUniformMatrix4fv(shd.program.uniforms["prev_mvpm"].location, 1, false, &prev_mvpm.data[0]);
	}
	
	glUniformMatrix4fv(shd.program.uniforms["light_bias_mvpm"].location, 1, false, &light_bias_mvpm.data[0]);
	
	const float3 cam_pos {
#if !defined(FORWARD_PROJECTION)
		(warp_state.is_scatter && !warp_state.is_bidir_scatter) ?
		cam.get_position() : cam.get_prev_position()
#else
		cam.get_prev_position()
#endif
		* float3 { -1.0f, 1.0f, -1.0f }
	};
	glUniform3fv(shd.program.uniforms["cam_pos"].location, 1, cam_pos.data());
	
	glUniform3fv(shd.program.uniforms["light_pos"].location, 1, light_pos.data());
	
	glBindBuffer(GL_ARRAY_BUFFER, model.vertices_vbo);
	const GLuint vertices_location = (GLuint)shd.program.attributes["in_vertex"].location;
	glEnableVertexAttribArray(vertices_location);
	glVertexAttribPointer(vertices_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	glBindBuffer(GL_ARRAY_BUFFER, model.tex_coords_vbo);
	const GLuint tex_coords_location = (GLuint)shd.program.attributes["in_tex_coord"].location;
	glEnableVertexAttribArray(tex_coords_location);
	glVertexAttribPointer(tex_coords_location, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	glBindBuffer(GL_ARRAY_BUFFER, model.normals_vbo);
	const GLuint normals_location = (GLuint)shd.program.attributes["in_normal"].location;
	glEnableVertexAttribArray(normals_location);
	glVertexAttribPointer(normals_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	glBindBuffer(GL_ARRAY_BUFFER, model.binormals_vbo);
	const GLuint binormals_location = (GLuint)shd.program.attributes["in_binormal"].location;
	glEnableVertexAttribArray(binormals_location);
	glVertexAttribPointer(binormals_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	glBindBuffer(GL_ARRAY_BUFFER, model.tangents_vbo);
	const GLuint tangents_location = (GLuint)shd.program.attributes["in_tangent"].location;
	glEnableVertexAttribArray(tangents_location);
	glVertexAttribPointer(tangents_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	const GLint shadow_loc { shd.program.uniforms["shadow_tex"].location };
	const GLint shadow_num { shd.program.samplers["shadow_tex"] };
	glUniform1i(shadow_loc, shadow_num);
	glActiveTexture((GLenum)(GL_TEXTURE0 + shadow_num));
	glBindTexture(GL_TEXTURE_2D, shadow_map.shadow_tex);
	
	const GLint diff_loc { shd.program.uniforms["diff_tex"].location };
	const GLint diff_num { shd.program.samplers["diff_tex"] };
	const GLint spec_loc { shd.program.uniforms["spec_tex"].location };
	const GLint spec_num { shd.program.samplers["spec_tex"] };
	const GLint norm_loc { shd.program.uniforms["norm_tex"].location };
	const GLint norm_num { shd.program.samplers["norm_tex"] };
	const GLint mask_loc { shd.program.uniforms["mask_tex"].location };
	const GLint mask_num { shd.program.samplers["mask_tex"] };
	for(const auto& obj : model.objects) {
		glUniform1i(diff_loc, diff_num);
		glActiveTexture((GLenum)(GL_TEXTURE0 + diff_num));
		glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].diffuse);
		
		glUniform1i(spec_loc, spec_num);
		glActiveTexture((GLenum)(GL_TEXTURE0 + spec_num));
		glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].specular);
		
		glUniform1i(norm_loc, norm_num);
		glActiveTexture((GLenum)(GL_TEXTURE0 + norm_num));
		glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].normal);
		
		glUniform1i(mask_loc, mask_num);
		glActiveTexture((GLenum)(GL_TEXTURE0 + mask_num));
		glBindTexture(GL_TEXTURE_2D, model.materials[obj->mat_idx].mask);
		
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->indices_gl_vbo);
		glDrawElements(GL_TRIANGLES, obj->index_count, GL_UNSIGNED_INT, nullptr);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	// draw sky box
	auto& skybox_shd = shader_objects[warp_state.is_scatter ? "SKYBOX_SCATTER" : "SKYBOX_GATHER"];
	glUseProgram(skybox_shd.program.program);
	
	const GLint skybox_loc { skybox_shd.program.uniforms["skybox_tex"].location };
	const GLint skybox_num { skybox_shd.program.samplers["skybox_tex"] };
	glUniform1i(skybox_loc, skybox_num);
	glActiveTexture((GLenum)(GL_TEXTURE0 + skybox_num));
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);
	
	matrix4f irmvpm;
	if(warp_state.is_scatter) {
#if defined(FORWARD_PROJECTION)
		irmvpm = (rmvm * pm).inverted();
#else
		irmvpm = (prev_rmvm * pm).inverted();
#endif
		const auto prev_imvpm = (prev_rmvm * pm).inverted();
		glUniformMatrix4fv(skybox_shd.program.uniforms["prev_imvpm"].location, 1, false, &prev_imvpm.data[0]);
	}
	else {
		irmvpm = (prev_rmvm * pm).inverted();
		const matrix4f next_mvpm { rmvm * pm };
		const matrix4f prev_mvpm { prev_prev_rmvm * pm };
		glUniformMatrix4fv(skybox_shd.program.uniforms["next_mvpm"].location, 1, false, &next_mvpm.data[0]);
		glUniformMatrix4fv(skybox_shd.program.uniforms["prev_mvpm"].location, 1, false, &prev_mvpm.data[0]);
	}
	glUniformMatrix4fv(skybox_shd.program.uniforms["imvpm"].location, 1, false, &irmvpm.data[0]);
	
	glDepthFunc(GL_LEQUAL); // need to overwrite clear depth of 1.0
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDepthFunc(GL_LESS);
	
	glUseProgram(0);
	glDrawBuffers(1, &draw_buffers[0]); // reset to 1 draw buffer
	
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

bool gl_renderer::compile_shaders() {
	static const char scene_vs_text[] { u8R"RAWSTR(
		uniform mat4 mvpm; // @t
#if defined(WARP_GATHER)
		uniform mat4 next_mvpm; // @t+1
		uniform mat4 prev_mvpm; // @t-1
#else
		uniform mat4 mvm;
		uniform mat4 prev_mvm;
#if defined(WARP_SCATTER_BIDIR)
		uniform mat4 next_mvm;
#endif
#endif
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
		
#if !defined(WARP_GATHER)
		out vec3 motion;
#if defined(WARP_SCATTER_BIDIR)
		out vec3 motion_next;
#endif
#else
		out vec4 motion_prev;
		out vec4 motion_now;
		out vec4 motion_next;
#endif
		
		void main() {
			tex_coord = in_tex_coord;
			
			vec4 vertex = vec4(in_vertex.xyz, 1.0);
			shadow_coord = light_bias_mvpm * vertex;
			gl_Position = mvpm * vertex;
			
#if !defined(WARP_GATHER)
			vec4 prev_pos = prev_mvm * vertex;
			vec4 cur_pos = mvm * vertex;
			motion = cur_pos.xyz - prev_pos.xyz;
#if defined(WARP_SCATTER_BIDIR)
			vec4 next_pos = next_mvm * vertex;
			//motion_next = next_pos.xyz - cur_pos.xyz;
			motion_next = cur_pos.xyz - next_pos.xyz;
#endif
#else
			motion_prev = prev_mvpm * vertex;
			motion_now = mvpm * vertex;
			motion_next = next_mvpm * vertex;
#endif
			
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
	static const char scene_fs_text[] { u8R"RAWSTR(
		uniform sampler2D diff_tex;
		uniform sampler2D spec_tex;
		uniform sampler2D norm_tex;
		uniform sampler2D mask_tex;
		uniform sampler2DShadow shadow_tex;
		
		in vec2 tex_coord;
		in vec4 shadow_coord;
		in vec3 view_dir;
		in vec3 light_dir;
		
#if !defined(WARP_GATHER)
		in vec3 motion;
#if defined(WARP_SCATTER_BIDIR)
		in vec3 motion_next;
#endif
#else
		in vec4 motion_prev;
		in vec4 motion_now;
		in vec4 motion_next;
#endif
		
		layout (location = 0) out vec4 frag_color;
#if !defined(WARP_GATHER)
#if defined(WARP_SCATTER_BIDIR)
		layout (location = 1) out uint motion_forward;
		layout (location = 2) out uint motion_backward;
#define DEPTH_Z_W_LOCATION 3
#else
		layout (location = 1) out uint motion_color;
#define DEPTH_Z_W_LOCATION 2
#endif
#else
		layout (location = 1) out uint motion_forward;
		layout (location = 2) out uint motion_backward;
		layout (location = 3) out vec2 motion_depth;
#define DEPTH_Z_W_LOCATION 4
#endif
		
#if DEPTH_Z_W == 1
		layout (location = DEPTH_Z_W_LOCATION) out float frag_depth;
#endif
		
		uint encode_3d_motion(in vec3 motion) {
			const float range = 64.0; // [-range, range]
			vec3 signs = sign(motion);
			vec3 cmotion = clamp(abs(motion), 0.0, range);
			// use log2 scaling
			cmotion = log2(cmotion + 1.0);
			// encode x and z with 10-bit, y with 9-bit
			cmotion *= 1.0 / log2(range + 1.0);
			cmotion.xz *= 1024.0; // 2^10
			cmotion.y *= 512.0; // 2^9
			return ((signs.x < 0.0 ? 0x80000000u : 0u) |
					(signs.y < 0.0 ? 0x40000000u : 0u) |
					(signs.z < 0.0 ? 0x20000000u : 0u) |
					(clamp(uint(cmotion.x), 0u, 1023u) << 19u) |
					(clamp(uint(cmotion.y), 0u, 511u) << 10u) |
					(clamp(uint(cmotion.z), 0u, 1023u)));
		}
		
		uint encode_2d_motion(in vec2 motion) { // NOTE: with GLSL 4.20, could also directly use packSnorm2x16(motion) here
			// uniform in [-1, 1]
			vec2 cmotion = clamp(motion * 32767.0, -32767.0, 32767.0); // +/- 2^15 - 1, fit into 16 bits
			// weird bit reinterpretation chain, b/c there is no direct way to interpret an ivec2 as an uvec2
			uvec2 umotion = floatBitsToUint(intBitsToFloat(ivec2(cmotion))) & 0xFFFFu;
			return (umotion.x | (umotion.y << 16u));
		}
		
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
#if !defined(WARP_GATHER)
#if defined(WARP_SCATTER_BIDIR)
			motion_forward = encode_3d_motion(motion_next);
			motion_backward = encode_3d_motion(motion);
#else
			motion_color = encode_3d_motion(motion);
#endif
#else
			motion_forward = encode_2d_motion((motion_next.xy / motion_next.w) - (motion_now.xy / motion_now.w));
			motion_backward = encode_2d_motion((motion_prev.xy / motion_prev.w) - (motion_now.xy / motion_now.w));
			motion_depth = vec2((motion_next.z / motion_next.w) - (motion_now.z / motion_now.w),
								(motion_prev.z / motion_prev.w) - (motion_now.z / motion_now.w));
#endif
			
#if DEPTH_Z_W == 1
			frag_depth = gl_FragCoord.z / gl_FragCoord.w;
#endif
		}
	)RAWSTR"};
	static const char shadow_map_vs_text[] { u8R"RAWSTR(
		uniform mat4 mvpm;
		in vec3 in_vertex;
		void main() {
			gl_Position = mvpm * vec4(in_vertex.xyz, 1.0);
		}
	)RAWSTR"};
	static const char shadow_map_fs_text[] { u8R"RAWSTR(
		out float frag_depth;
		void main() {
			frag_depth = gl_FragCoord.z;
		}
	)RAWSTR"};
	static const char skybox_vs_text[] { u8R"RAWSTR(
		uniform mat4 imvpm;
#if defined(WARP_GATHER)
		uniform mat4 next_mvpm; // @t+1
		uniform mat4 prev_mvpm; // @t-1
#else
		uniform mat4 prev_imvpm;
#endif
		
		out vec3 cube_tex_coord;
#if !defined(WARP_GATHER)
		out vec4 cur_pos;
		out vec4 prev_pos;
#else
		out vec4 motion_prev;
		out vec4 motion_now;
		out vec4 motion_next;
#endif
		
		void main() {
			const vec2 fullscreen_triangle[3] = vec2[](vec2(0.0, 2.0), vec2(-3.0, -1.0), vec2(3.0, -1.0));
			vec4 vertex = vec4(fullscreen_triangle[gl_VertexID], 1.0, 1.0);
			gl_Position = vertex;
			
			vec4 proj_vertex = imvpm * vertex;
			cube_tex_coord = proj_vertex.xyz;
			
#if !defined(WARP_GATHER)
			cur_pos = proj_vertex;
			prev_pos = prev_imvpm * vertex;
#else
			motion_prev = prev_mvpm * proj_vertex;
			motion_now = vertex;
			motion_next = next_mvpm * proj_vertex;
#endif
		}
	)RAWSTR"};
	static const char skybox_fs_text[] { u8R"RAWSTR(
		uniform samplerCube skybox_tex;
		
		in vec3 cube_tex_coord;
		
#if !defined(WARP_GATHER)
		in vec4 cur_pos;
		in vec4 prev_pos;
#else
		in vec4 motion_prev;
		in vec4 motion_now;
		in vec4 motion_next;
#endif
		
		layout (location = 0) out vec4 frag_color;
#if !defined(WARP_GATHER)
#if defined(WARP_SCATTER_BIDIR)
		layout (location = 1) out uint motion_forward;
		layout (location = 2) out uint motion_backward;
#define DEPTH_Z_W_LOCATION 3
#else
		layout (location = 1) out uint motion_color;
#define DEPTH_Z_W_LOCATION 2
#endif
#else
		layout (location = 1) out uint motion_forward;
		layout (location = 2) out uint motion_backward;
		layout (location = 3) out vec2 motion_depth;
#define DEPTH_Z_W_LOCATION 4
#endif
		
#if DEPTH_Z_W == 1
		layout (location = DEPTH_Z_W_LOCATION) out float frag_depth;
#endif
		
		uint encode_3d_motion(in vec3 motion) {
			const float range = 64.0; // [-range, range]
			vec3 signs = sign(motion);
			vec3 cmotion = clamp(abs(motion), 0.0, range);
			// use log2 scaling
			cmotion = log2(cmotion + 1.0);
			// encode x and z with 10-bit, y with 9-bit
			cmotion *= 1.0 / log2(range + 1.0);
			cmotion.xz *= 1024.0; // 2^10
			cmotion.y *= 512.0; // 2^9
			return ((signs.x < 0.0 ? 0x80000000u : 0u) |
					(signs.y < 0.0 ? 0x40000000u : 0u) |
					(signs.z < 0.0 ? 0x20000000u : 0u) |
					(clamp(uint(cmotion.x), 0u, 1023u) << 19u) |
					(clamp(uint(cmotion.y), 0u, 511u) << 10u) |
					(clamp(uint(cmotion.z), 0u, 1023u)));
		}
		
		uint encode_2d_motion(in vec2 motion) { // NOTE: with GLSL 4.20, could also directly use packSnorm2x16(motion) here
			// uniform in [-1, 1]
			vec2 cmotion = clamp(motion * 32767.0, -32767.0, 32767.0); // +/- 2^15 - 1, fit into 16 bits
			// weird bit reinterpretation chain, b/c there is no direct way to interpret an ivec2 as an uvec2
			uvec2 umotion = floatBitsToUint(intBitsToFloat(ivec2(cmotion))) & 0xFFFFu;
			return (umotion.x | (umotion.y << 16u));
		}
		
		void main() {
			frag_color = texture(skybox_tex, cube_tex_coord);
#if !defined(WARP_GATHER)
			motion_color = encode_3d_motion(prev_pos.xyz - cur_pos.xyz); // not sure why inverted?
#else
			motion_forward = encode_2d_motion((motion_next.xy / motion_next.w) - (motion_now.xy / motion_now.w));
			motion_backward = encode_2d_motion((motion_prev.xy / motion_prev.w) - (motion_now.xy / motion_now.w));
			motion_depth = vec2((motion_next.z / motion_next.w) - (motion_now.z / motion_now.w),
								(motion_prev.z / motion_prev.w) - (motion_now.z / motion_now.w));
#endif
			
#if DEPTH_Z_W == 1
			frag_depth = gl_FragCoord.z / gl_FragCoord.w;
#endif
		}
	)RAWSTR"};
	
	{
		const auto shd = floor_compile_shader("SCENE_SCATTER", scene_vs_text, scene_fs_text, 330,
											  { { "DEPTH_Z_W", warp_state.is_zw_depth ? 1 : 0 } });
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("SCENE_GATHER", scene_vs_text, scene_fs_text, 330,
											  { { "WARP_GATHER", 1 }, { "DEPTH_Z_W", warp_state.is_zw_depth ? 1 : 0 } });
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("SCENE_SCATTER_BIDIR", scene_vs_text, scene_fs_text, 330,
											  { { "WARP_SCATTER_BIDIR", 1 }, { "DEPTH_Z_W", warp_state.is_zw_depth ? 1 : 0 } });
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("SKYBOX_SCATTER", skybox_vs_text, skybox_fs_text, 330,
											  { { "DEPTH_Z_W", warp_state.is_zw_depth ? 1 : 0 } });
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("SKYBOX_GATHER", skybox_vs_text, skybox_fs_text, 330,
											  { { "WARP_GATHER", 1 }, { "DEPTH_Z_W", warp_state.is_zw_depth ? 1 : 0 } });
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("SHADOW", shadow_map_vs_text, shadow_map_fs_text);
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	return true;
}

#else
bool gl_renderer::init() { return false; }
void gl_renderer::destroy() {}
bool gl_renderer::render(const gl_obj_model&, const camera&) { return false; }
#endif
