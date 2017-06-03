/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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
#include "nbody_state.hpp"
#include <floor/core/gl_shader.hpp>

static unordered_map<string, floor_shader_object> shader_objects;

static array<GLuint, 2> body_textures {{ 0u, 0u }};
static void create_textures() {
	// create/generate an opengl texture and bind it
	glGenTextures(2, &body_textures[0]);
	for(size_t i = 0; i < body_textures.size(); ++i) {
		glBindTexture(GL_TEXTURE_2D, body_textures[i]);
		
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
#if 1 // smoother, less of a center point
				float fval = dir.dot();
#else
				float fval = dir.length();
#endif
				uint32_t val = 255u - uint8_t(const_math::clamp(fval, 0.0f, 1.0f) * 255.0f);
				if(i == 0) {
					pixel_data[y * texture_size.x + x] = val + (val << 8u) + (val << 16u) + (val << 24u);
				}
				else {
					uint32_t alpha_val = (val > 16u ? 0xFFu : val);
					pixel_data[y * texture_size.x + x] = val + (val << 8u) + (val << 16u) + (alpha_val << 24u);
				}
			}
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_size.x, texture_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixel_data[0]);
		
		// build mipmaps
		glGenerateMipmap(GL_TEXTURE_2D);
	}
}

static uint32_t global_vao { 0 };
bool gl_renderer::init() {
	floor::init_gl();
	glFrontFace(GL_CCW);
	glGenVertexArrays(1, &global_vao);
	glBindVertexArray(global_vao);
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	create_textures();
	
	return compile_shaders();
}

void gl_renderer::render(shared_ptr<compute_queue> dev_queue,
						 shared_ptr<compute_buffer> position_buffer) {
	// draws ogl stuff
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindVertexArray(global_vao);
	glViewport(0, 0, (GLsizei)floor::get_width(), (GLsizei)floor::get_height());
	
	// clear the color/depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	//
	glEnable(GL_BLEND);
	if(!nbody_state.alpha_mask) {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		glDepthMask(GL_FALSE);
	}
	else {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_TRUE);
	}
	
	glEnable(GL_PROGRAM_POINT_SIZE);
	
	//
	position_buffer->release_opengl_object(dev_queue);
	
	auto& shd = (nbody_state.render_sprites ?
				 (nbody_state.alpha_mask ? shader_objects["SPRITE_DISCARD"] : shader_objects["SPRITE"]) :
				 (nbody_state.alpha_mask ? shader_objects["POINT_DISCARD"] : shader_objects["POINT"]));
	glUseProgram(shd.program.program);
	
	glUniform1i((GLint)shd.program.uniforms["tex"].location, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (nbody_state.alpha_mask ? body_textures[1] : body_textures[0]));
	
	const matrix4f mproj { matrix4f().perspective(90.0f, float(floor::get_width()) / float(floor::get_height()), 0.25f, nbody_state.max_distance) };
	const matrix4f mview { nbody_state.cam_rotation.to_matrix4() * matrix4f().translate(0.0f, 0.0f, -nbody_state.distance) };
	const matrix4f mvpm { mview * mproj };
	glUniformMatrix4fv((GLint)shd.program.uniforms["mvpm"].location, 1, false, &mvpm.data[0]);
	if(nbody_state.render_sprites) glUniformMatrix4fv((GLint)shd.program.uniforms["mvm"].location, 1, false, &mview.data[0]);
	
	glUniform2f((GLint)shd.program.uniforms["mass_minmax"].location, nbody_state.mass_minmax.x, nbody_state.mass_minmax.y);
	
	glBindBuffer(GL_ARRAY_BUFFER, position_buffer->get_opengl_object());
	const GLuint vertices_location = (GLuint)shd.program.attributes["in_vertex"].location;
	glEnableVertexAttribArray(vertices_location);
	glVertexAttribPointer(vertices_location, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
	
	glDrawArrays(GL_POINTS, 0, (GLsizei)nbody_state.body_count);
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	position_buffer->acquire_opengl_object(dev_queue);
}

bool gl_renderer::compile_shaders() {
	static const string nbody_color_gradient_compute { u8R"RAWSTR(
		vec4 compute_gradient(const in float interpolator) {
			const vec4 gradients[4] = vec4[4](vec4(1.0, 0.2, 0.0, 1.0),
											  vec4(1.0, 1.0, 0.0, 1.0),
											  vec4(1.0, 1.0, 1.0, 1.0),
											  vec4(0.5, 1.0, 1.0, 1.0));
			
			vec4 color = vec4(0.0);
			// built-in step function can not be trusted -> branch instead ...
			if(interpolator < 0.33333) {
				float interp = smoothstep(0.0, 0.33333, interpolator);
				color += mix(gradients[0], gradients[1], interp);
			}
			else if(interpolator < 0.66666) {
				float interp = smoothstep(0.33333, 0.66666, interpolator);
				color += mix(gradients[1], gradients[2], interp);
			}
			else if(interpolator <= 1.0) {
				float interp = smoothstep(0.66666, 1.0, interpolator);
				color += mix(gradients[2], gradients[3], interp);
			}
			return color;
		}
	)RAWSTR"};
	static const string nbody_vs_text { nbody_color_gradient_compute + u8R"RAWSTR(
		uniform mat4 mvpm;
		uniform vec2 mass_minmax;
		in vec4 in_vertex;
		out vec2 tex_coord;
		out vec4 color;
		void main() {
			tex_coord = vec2(0.0, 0.0);
			color = compute_gradient((in_vertex.w - mass_minmax.x) / (mass_minmax.y - mass_minmax.x));
			gl_Position = mvpm * vec4(in_vertex.xyz, 1.0);
		}
	)RAWSTR"};
	static const string nbody_vs_sprite_text { nbody_color_gradient_compute + u8R"RAWSTR(
		uniform mat4 mvpm;
		uniform mat4 mvm;
		uniform vec2 mass_minmax;
		in vec4 in_vertex;
		out vec2 tex_coord;
		out vec4 color;
		
		void main() {
			tex_coord = vec2(0.5, 0.5);
			float size = 128.0 / (1.0 - vec3(mvm * vec4(in_vertex.xyz, 1.0)).z);
			float mass_scale = (in_vertex.w - mass_minmax.x) / (mass_minmax.y - mass_minmax.x);
			mass_scale *= mass_scale; // ^2
			//mass_scale *= mass_scale; // ^4
			//mass_scale *= mass_scale; // ^8
			size *= mass_scale;
			gl_PointSize = clamp(size, 2.0, 255.0);
			gl_Position = mvpm * vec4(in_vertex.xyz, 1.0);
			
			color = compute_gradient((in_vertex.w - mass_minmax.x) / (mass_minmax.y - mass_minmax.x));
		}
	)RAWSTR"};
	static const string nbody_fs_text { u8R"RAWSTR(
		in vec4 color;
		uniform mat4 mvpm;
		uniform sampler2D tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			frag_color = texture(tex, gl_PointCoord.xy) * color;
		}
	)RAWSTR"};
	static const string nbody_fs_discard_text { u8R"RAWSTR(
		in vec4 color;
		uniform mat4 mvpm;
		uniform sampler2D tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			vec4 tex_color = texture(tex, gl_PointCoord.xy);
			if(tex_color.w <= 0.1 || tex_color.x <= 0.1) discard;
			frag_color = vec4(tex_color.xyz * color.xyz * tex_color.w, tex_color.w);
		}
	)RAWSTR"};
	
	{
		const auto shd = floor_compile_shader("POINT", nbody_vs_text.c_str(), nbody_fs_text.c_str());
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("POINT_DISCARD", nbody_vs_text.c_str(), nbody_fs_discard_text.c_str());
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("SPRITE", nbody_vs_sprite_text.c_str(), nbody_fs_text.c_str());
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	{
		const auto shd = floor_compile_shader("SPRITE_DISCARD", nbody_vs_sprite_text.c_str(), nbody_fs_discard_text.c_str());
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	return true;
}
