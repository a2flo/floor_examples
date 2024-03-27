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

#include "gl_renderer.hpp"
#include "hlbvh_state.hpp"
#include <floor/core/gl_shader.hpp>

static unordered_map<string, floor_shader_object> shader_objects;

static uint32_t global_vao { 0 };
bool gl_renderer::init() {
	floor::init_gl();
	glFrontFace(GL_CCW);
#if 1
	glEnable(GL_CULL_FACE);
#else
	glDisable(GL_CULL_FACE);
#endif
	glGenVertexArrays(1, &global_vao);
	glBindVertexArray(global_vao);
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	return compile_shaders();
}

void gl_renderer::render(const vector<unique_ptr<animation>>& models,
						 const bool cam_mode,
						 const camera& cam) {
	// draws ogl stuff
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindVertexArray(global_vao);
	glViewport(0, 0, (GLsizei)floor::get_width(), (GLsizei)floor::get_height());
	
	// clear the color/depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	//
	auto& shd = shader_objects["SHADE"];
	glUseProgram(shd.program.program);
	
	// fixed locations over the lifetime of this program
	static const auto mvpm_location = (GLint)shd.program.uniforms["mvpm"].location;
	static const auto default_color_location = (GLint)shd.program.uniforms["default_color"].location;
	static const auto light_dir_location = (GLint)shd.program.uniforms["light_dir"].location;
	static const auto delta_location = (GLint)shd.program.uniforms["delta"].location;
	static const auto pos_a_location = (GLuint)shd.program.attributes["in_position_a"].location;
	static const auto pos_b_location = (GLuint)shd.program.attributes["in_position_b"].location;
	static const auto norm_a_location = (GLuint)shd.program.attributes["in_normal_a"].location;
	static const auto norm_b_location = (GLuint)shd.program.attributes["in_normal_b"].location;
	static const auto is_collision_location = (hlbvh_state.triangle_vis ? (GLuint)shd.program.attributes["is_collision"].location : 0);
	
	const matrix4f mproj {
		matrix4f::perspective<false>(72.0f, float(floor::get_width()) / float(floor::get_height()),
									  0.25f, hlbvh_state.max_distance)
	};
	matrix4f mview;
	if(!cam_mode) {
		mview = hlbvh_state.cam_rotation.to_matrix4() * matrix4f().translate(0.0f, 0.0f, -hlbvh_state.distance);
	}
	else {
		const auto rmvm = (matrix4f::rotation_deg_named<'y'>(float(cam.get_rotation().y)) *
						   matrix4f::rotation_deg_named<'x'>(float(cam.get_rotation().x)));
		mview = matrix4f::translation(cam.get_position() * float3 { 1.0f, -1.0f, 1.0f }) * rmvm;
	}
	const struct __attribute__((packed)) {
		matrix4f mvpm;
		float4 default_color;
		float3 light_dir;
	} uniforms {
		.mvpm = mview * mproj,
		.default_color = { 0.9f, 0.9f, 0.9f, 1.0f },
		.light_dir = { 1.0f, 0.0f, 0.0f },
	};
	
	glUniformMatrix4fv(mvpm_location, 1, false, &uniforms.mvpm.data[0]);
	glUniform4fv(default_color_location, 1, uniforms.default_color.data());
	glUniform3fv(light_dir_location, 1, uniforms.light_dir.data());
	
	for (const auto& mdl : models) {
		const auto cur_frame = (const gl_obj_model*)mdl->frames[mdl->cur_frame].get();
		const auto next_frame = (const gl_obj_model*)mdl->frames[mdl->next_frame].get();
		
		glUniform4fv(default_color_location, 1, uniforms.default_color.data());
		glUniform1f(delta_location, mdl->step);
		
		if(hlbvh_state.triangle_vis) {
			mdl->colliding_vertices->release_opengl_object(hlbvh_state.cqueue.get());
			glBindBuffer(GL_ARRAY_BUFFER, mdl->colliding_vertices->get_opengl_object());
			glEnableVertexAttribArray(is_collision_location);
			glVertexAttribPointer(is_collision_location, 1, GL_UNSIGNED_INT, GL_FALSE, 0, nullptr);
		}
		
		glBindBuffer(GL_ARRAY_BUFFER, cur_frame->vertices_vbo);
		glEnableVertexAttribArray(pos_a_location);
		glVertexAttribPointer(pos_a_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		glBindBuffer(GL_ARRAY_BUFFER, next_frame->vertices_vbo);
		glEnableVertexAttribArray(pos_b_location);
		glVertexAttribPointer(pos_b_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		glBindBuffer(GL_ARRAY_BUFFER, cur_frame->normals_vbo);
		glEnableVertexAttribArray(norm_a_location);
		glVertexAttribPointer(norm_a_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		glBindBuffer(GL_ARRAY_BUFFER, next_frame->normals_vbo);
		glEnableVertexAttribArray(norm_b_location);
		glVertexAttribPointer(norm_b_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		
		for(const auto& obj : cur_frame->objects) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->indices_gl_vbo);
			glDrawElements(GL_TRIANGLES, obj->index_count, GL_UNSIGNED_INT, nullptr);
		}
		
		if(hlbvh_state.triangle_vis) {
			mdl->colliding_vertices->acquire_opengl_object(hlbvh_state.cqueue.get());
		}
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glUseProgram(0);
}

bool gl_renderer::compile_shaders() {
	static const string hlbvh_vs_text { R"RAWSTR(
		uniform mat4 mvpm;
		uniform vec4 default_color;
		uniform vec3 light_dir;
		uniform float delta;
		
		in vec3 in_position_a;
		in vec3 in_position_b;
		in vec3 in_normal_a;
		in vec3 in_normal_b;
		
#if COLLIDING_TRIANGLES_VIS
		in uint is_collision;
		noperspective out float collision;
#endif
		
		out vec4 color;
		out vec3 normal;
		
		void main() {
			vec4 pos = vec4(mix(in_position_a, in_position_b, delta), 1.0);
			gl_Position = mvpm * pos;
			
#if COLLIDING_TRIANGLES_VIS
			collision = (is_collision > 0u ? 3.0 : 0.0);
#endif
			color = default_color;
			normal = mix(in_normal_a, in_normal_b, delta);
		}
	)RAWSTR"};
	static const string hlbvh_fs_text { R"RAWSTR(
		uniform vec3 light_dir;
		
#if COLLIDING_TRIANGLES_VIS
		noperspective in float collision;
#endif
		
		in vec4 color;
		in vec3 normal;
		
		out vec4 frag_color;
		
		void main() {
			float intensity = dot(light_dir, normal);
			
			vec4 out_color;
#if COLLIDING_TRIANGLES_VIS
			if (collision > 0.0) {
				out_color = vec4(1.0, 1.0 - clamp(collision * 0.3333, 0.0, 1.0), 0.0, 1.0);
				out_color.xyz *= max(intensity, 0.6);
			}
			else
#endif
			{
				vec4 in_color = color;
				
				if (intensity > 0.95) {
					out_color = vec4(in_color.xyz * 1.2, in_color.w);
				} else if (intensity > 0.25) {
					out_color = vec4(in_color.xyz * intensity, in_color.w);
				} else {
					out_color = vec4(in_color.xyz * 0.1, in_color.w);
				}
			}
			out_color.xyz *= out_color.w;
			
			frag_color = out_color;
		}
	)RAWSTR"};
	
	{
		const auto shd = floor_compile_shader("SHADE",
											  hlbvh_vs_text.c_str(),
											  hlbvh_fs_text.c_str(),
											  150,
											  { { "COLLIDING_TRIANGLES_VIS", hlbvh_state.triangle_vis ? 1 : 0 } });
		if(!shd.first) return false;
		shader_objects.emplace(shd.second.name, shd.second);
	}
	return true;
}
