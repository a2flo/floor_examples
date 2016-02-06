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

#include "gl_blur.hpp"

#if !defined(FLOOR_IOS)

static floor_shader_object h_blur_shd, v_blur_shd;
static GLuint rtt_fbo { 0u };

bool gl_blur::init(const uint2& dim, const uint32_t& tap_count) {
	if(tap_count != 15 && tap_count != 17) {
		log_error("tap count != 15 or 17 currently not supported!");
		return false;
	}
	if(dim.x != dim.y) {
		log_error("image width must be identical to image height for now!");
		return false;
	}
	
	const long double px_size = 1.0L / (long double)dim.x;
	stringstream offsets;
	offsets.precision(12);
	const int32_t half_width = tap_count / 2;
	for(int i = -half_width; i <= half_width; ++i) {
		offsets << (((long double)i) * px_size);
		if(i != half_width) offsets << ", ";
	}
	log_debug("offsets: %s", offsets.str());
	
	static const char blur_vs_text[] { u8R"RAWSTR(
		in vec2 in_vertex;
		out vec2 tex_coord;
		void main() {
			tex_coord = in_vertex.xy * 0.5 + 0.5;
			gl_Position = vec4(in_vertex.xy, 0.0, 1.0);
		})RAWSTR" };
	static const string blur_fs_text { u8R"RAWSTR(
		uniform sampler2D tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			// NOTE: fixed for 15-tap or 17-tap
			const float offsets[TAP_COUNT] = float[TAP_COUNT]()RAWSTR"
			+ offsets.str() +
			u8R"RAWSTR();
#if TAP_COUNT == 17
			const float coeffs[17] = float[17](0.00437766, 0.00984974, 0.0196995, 0.0351776,
											   0.0562842, 0.0809086, 0.104705, 0.122156,
											   0.128585,
											   0.122156, 0.104705, 0.0809086, 0.0562842,
											   0.0351776, 0.0196995, 0.00984974, 0.00437766);
#elif TAP_COUNT == 15
			const float coeffs[15] = float[15](0.00441089272499, 0.0115785934031, 0.0257302075624,
											   0.0488873943686, 0.0799975544214, 0.113329872489, 0.139482915401,
											   0.149445980787,
											   0.139482915401, 0.113329872489, 0.0799975544214, 0.0488873943686,
											   0.0257302075624, 0.0115785934031, 0.00441089272499);
#endif
			vec4 color = vec4(0.0);
			for(int i = 0; i < TAP_COUNT; ++i) {
				color += coeffs[i] * texture(tex, tex_coord +
#if defined(BLUR_HORIZONTAL)
											 vec2(offsets[i], 0.0)
#elif defined(BLUR_VERTICAL)
											 vec2(0.0, offsets[i])
#endif
											 );
			}
			frag_color = color;
		}
	)RAWSTR" };

	const auto hshd = floor_compile_shader("BLUR_HORIZONTAL", blur_vs_text, blur_fs_text.c_str(), 150,
										   { { "BLUR_HORIZONTAL", 1 }, { "TAP_COUNT", tap_count } });
	if(!hshd.first) return false;
	h_blur_shd = hshd.second;
	
	const auto vshd = floor_compile_shader("BLUR_VERTICAL", blur_vs_text, blur_fs_text.c_str(), 150,
										   { { "BLUR_VERTICAL", 1 }, { "TAP_COUNT", tap_count } });
	if(!vshd.first) return false;
	v_blur_shd = vshd.second;
	
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
		glUseProgram(h_blur_shd.program.program);
		
		// bind/set source texture
		glUniform1i(h_blur_shd.program.uniforms["tex"].location, 0);
		glBindTexture(GL_TEXTURE_2D, tex_src);
		
		// and go
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	
	// attach final destination texture
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_dst, 0);
	{
		glUseProgram(v_blur_shd.program.program);
		
		// bind/set source texture
		glUniform1i(v_blur_shd.program.uniforms["tex"].location, 0);
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
		
#endif
