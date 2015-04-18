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

#include "gl_blur.hpp"

static shared_ptr<img_shader_object> h_blur_shd, v_blur_shd;
static GLuint rtt_fbo { 0u };

bool gl_blur::init(const uint2& dim, const uint32_t& tap_count) {
	if(tap_count != 17) {
		log_error("tap count != 17 currently not supported!");
		return false;
	}
	if(dim.x != dim.y) {
		log_error("image width must be identical to image height for now!");
		return false;
	}
	
	const long double px_size = 1.0L / (long double)dim.x;
	stringstream offsets;
	offsets.precision(12);
	for(int i = -8; i <= 8; ++i) {
		offsets << (((long double)i) * px_size);
		if(i != 8) offsets << ", ";
	}
	log_debug("offsets: %s", offsets.str());
	
	static const char blur_vs_text[] { u8R"RAWSTR(#version 150 core
		in vec2 in_vertex;
		out vec2 tex_coord;
		void main() {
			tex_coord = in_vertex.xy * 0.5 + 0.5;
			gl_Position = vec4(in_vertex.xy, 0.0, 1.0);
		})RAWSTR" };
	static const string blur_h_fs_text { u8R"RAWSTR(#version 150 core
		uniform sampler2D tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			// NOTE: fixed for 17-tap
			const float offsets[17] = float[17]()RAWSTR"
			+ offsets.str() +
			u8R"RAWSTR();
			const float coeffs[17] = float[17](1.525878906e-05, 0.000244140625, 0.001831054688, 0.008544921875,
											   0.02777099609, 0.06665039062, 0.1221923828, 0.1745605469,
											   0.1963806152,
											   0.1745605469, 0.1221923828, 0.06665039062, 0.02777099609,
											   0.008544921875, 0.001831054688, 0.000244140625, 1.525878906e-05);
			vec4 color = vec4(0.0);
			for(int i = 0; i < 17; ++i) {
				color += coeffs[i] * texture(tex, tex_coord + vec2(offsets[i], 0.0));
			}
			frag_color = color;
		}
	)RAWSTR" };
	static const string blur_v_fs_text { u8R"RAWSTR(#version 150 core
		uniform sampler2D tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			const float offsets[17] = float[17]()RAWSTR"
			+ offsets.str() +
			u8R"RAWSTR();
			const float coeffs[17] = float[17](1.525878906e-05, 0.000244140625, 0.001831054688, 0.008544921875,
											   0.02777099609, 0.06665039062, 0.1221923828, 0.1745605469,
											   0.1963806152,
											   0.1745605469, 0.1221923828, 0.06665039062, 0.02777099609,
											   0.008544921875, 0.001831054688, 0.000244140625, 1.525878906e-05);
			vec4 color = vec4(0.0);
			for(int i = 0; i < 17; ++i) {
				color += coeffs[i] * texture(tex, tex_coord + vec2(0.0, offsets[i]));
			}
			frag_color = color;
		}
	)RAWSTR" };

	h_blur_shd = make_shared<img_shader_object>("BLUR_HORIZONTAL");
	if(!compile_shader(*h_blur_shd.get(), &blur_vs_text[0], blur_h_fs_text.c_str())) return false;
	
	v_blur_shd = make_shared<img_shader_object>("BLUR_VERTICAL");
	if(!compile_shader(*v_blur_shd.get(), &blur_vs_text[0], blur_v_fs_text.c_str())) return false;

	if(h_blur_shd == nullptr || v_blur_shd == nullptr) {
		return false;
	}
	
	// create fbo
	glGenFramebuffers(1, &rtt_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, rtt_fbo);
	
	// set correct render size
	glViewport(0, 0, GLsizei(dim.x), GLsizei(dim.y));
	
	return true;
}

void gl_blur::blur(const GLuint& tex_src, const GLuint& tex_dst, const GLuint& tex_tmp,
				   const GLuint& vbo_fullscreen_triangle) {
	// bind our rtt framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, rtt_fbo);
	
	// attach tmp texture as the first destination texture (what we will render into in the first pass)
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_tmp, 0);
	
	// will be rendering a fullscreen triangle
	glBindBuffer(GL_ARRAY_BUFFER, vbo_fullscreen_triangle);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(0);
	
	// blur / horizontal first, as this is apparently faster in opengl/glsl
	{
		glUseProgram(h_blur_shd->program.program);
		
		// bind/set source texture
		glUniform1i((GLint)h_blur_shd->program.uniforms["tex"].location, 0);
		glBindTexture(GL_TEXTURE_2D, tex_src);
		
		// and go
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	
	// attach final destination texture
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_dst, 0);
	{
		glUseProgram(v_blur_shd->program.program);
		
		// bind/set source texture
		glUniform1i((GLint)v_blur_shd->program.uniforms["tex"].location, 0);
		glBindTexture(GL_TEXTURE_2D, tex_tmp);
		
		// and go
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	
	// clean up
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, FLOOR_DEFAULT_FRAMEBUFFER);
}
