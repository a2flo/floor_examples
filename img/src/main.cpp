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

#include <floor/floor/floor.hpp>
#include <floor/ios/ios_helper.hpp>
#include <floor/core/option_handler.hpp>
#include <floor/core/timer.hpp>

struct img_option_context {
	// unused
	string additional_options { "" };
};
typedef option_handler<img_option_context> img_opt_handler;

static bool done { false };
static bool no_opengl { false };
static uint32_t cur_image { 0 };
static uint2 image_size { 1024 };

//! option -> function map
template<> unordered_map<string, img_opt_handler::option_function> img_opt_handler::options {
	{ "--help", [](img_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--dim <width> <height>: image width * height in px (default: " << image_size << ")" << endl;
		cout << "\t--no-opengl: disables opengl rendering and sharing (uses s/w rendering instead)" << endl;
		
		cout << endl;
		cout << "controls:" << endl;
		cout << "\tq: quit" << endl;
		cout << endl;
		done = true;
	}},
	{ "--dim", [](img_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --dim!" << endl;
			done = true;
			return;
		}
		image_size.x = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after second --dim parameter!" << endl;
			done = true;
			return;
		}
		image_size.y = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		cout << "image size set to: " << image_size << endl;
	}},
	{ "--no-opengl", [](img_option_context&, char**&) {
		no_opengl = true;
		cout << "opengl disabled" << endl;
	}},
};

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_q:
				done = true;
				break;
			case SDLK_1:
				cur_image = 0;
				break;
			case SDLK_2:
				cur_image = 1;
				break;
			case SDLK_w:
				cur_image = 1 - cur_image;
				break;
			default: break;
		}
		return true;
	}
	return false;
}

/////////////////
// TODO: creata a common header/source file ...
		
static GLuint vbo_fullscreen_triangle { 0 };
static uint32_t global_vao { 0 };

struct img_shader_object {
	struct internal_shader_object {
		unsigned int program { 0 };
		unsigned int vertex_shader { 0 };
		unsigned int fragment_shader { 0 };
		
		struct shader_variable {
			size_t location;
			size_t size;
			size_t type;
		};
		unordered_map<string, shader_variable> uniforms;
		unordered_map<string, shader_variable> attributes;
		unordered_map<string, size_t> samplers;
	};
	const string name;
	internal_shader_object program;
	
	img_shader_object(const string& shd_name) : name(shd_name) {}
	~img_shader_object() {}
};
static unordered_map<string, shared_ptr<img_shader_object>> shader_objects;

#define IMG_GL_SHADER_SAMPLER_TYPES(F) \
F(GL_SAMPLER_1D) \
F(GL_SAMPLER_2D) \
F(GL_SAMPLER_3D) \
F(GL_SAMPLER_CUBE) \
F(GL_SAMPLER_1D_SHADOW) \
F(GL_SAMPLER_2D_SHADOW) \
F(GL_SAMPLER_1D_ARRAY) \
F(GL_SAMPLER_2D_ARRAY) \
F(GL_SAMPLER_1D_ARRAY_SHADOW) \
F(GL_SAMPLER_2D_ARRAY_SHADOW) \
F(GL_SAMPLER_CUBE_SHADOW) \
F(GL_SAMPLER_BUFFER) \
F(GL_SAMPLER_2D_RECT) \
F(GL_SAMPLER_2D_RECT_SHADOW) \
F(GL_INT_SAMPLER_1D) \
F(GL_INT_SAMPLER_2D) \
F(GL_INT_SAMPLER_3D) \
F(GL_INT_SAMPLER_1D_ARRAY) \
F(GL_INT_SAMPLER_2D_ARRAY) \
F(GL_INT_SAMPLER_2D_RECT) \
F(GL_INT_SAMPLER_BUFFER) \
F(GL_INT_SAMPLER_CUBE) \
F(GL_UNSIGNED_INT_SAMPLER_1D) \
F(GL_UNSIGNED_INT_SAMPLER_2D) \
F(GL_UNSIGNED_INT_SAMPLER_3D) \
F(GL_UNSIGNED_INT_SAMPLER_1D_ARRAY) \
F(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY) \
F(GL_UNSIGNED_INT_SAMPLER_2D_RECT) \
F(GL_UNSIGNED_INT_SAMPLER_BUFFER) \
F(GL_UNSIGNED_INT_SAMPLER_CUBE) \
F(GL_SAMPLER_2D_MULTISAMPLE) \
F(GL_INT_SAMPLER_2D_MULTISAMPLE) \
F(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE) \
F(GL_SAMPLER_2D_MULTISAMPLE_ARRAY) \
F(GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY) \
F(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)

#define __IMG_DECLARE_GL_TYPE_CHECK(type) case type: return true;
static bool is_gl_sampler_type(const GLenum& type) {
	switch(type) {
		IMG_GL_SHADER_SAMPLER_TYPES(__IMG_DECLARE_GL_TYPE_CHECK);
		default: break;
	}
	return false;
}

#define SHADER_LOG_SIZE 32768
static bool compile_shader(img_shader_object& shd, const char* vs_text, const char* fs_text) {
	// success flag (if it's 1 (true), we successfully created a shader object)
	GLint success = 0;
	GLchar info_log[SHADER_LOG_SIZE+1];
	memset(&info_log[0], 0, SHADER_LOG_SIZE+1);
	
	// add a new program object to this shader
	img_shader_object::internal_shader_object& shd_obj = shd.program;
	
	// create the vertex shader object
	shd_obj.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shd_obj.vertex_shader, 1, (GLchar const**)&vs_text, nullptr);
	glCompileShader(shd_obj.vertex_shader);
	glGetShaderiv(shd_obj.vertex_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.vertex_shader, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in vertex shader \"%s\" compilation:\n%s", shd.name, info_log);
		return false;
	}
	
	// create the fragment shader object
	shd_obj.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shd_obj.fragment_shader, 1, (GLchar const**)&fs_text, nullptr);
	glCompileShader(shd_obj.fragment_shader);
	glGetShaderiv(shd_obj.fragment_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.fragment_shader, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in fragment shader \"%s\" compilation:\n%s", shd.name, info_log);
		return false;
	}
	
	// create the program object
	shd_obj.program = glCreateProgram();
	// attach the vertex and fragment shader progam to it
	glAttachShader(shd_obj.program, shd_obj.vertex_shader);
	glAttachShader(shd_obj.program, shd_obj.fragment_shader);
	
	// now link the program object
	glLinkProgram(shd_obj.program);
	glGetProgramiv(shd_obj.program, GL_LINK_STATUS, &success);
	if(!success) {
		glGetProgramInfoLog(shd_obj.program, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in program \"%s\" linkage!\nInfo log: %s", shd.name, info_log);
		return false;
	}
	glUseProgram(shd_obj.program);
	
	// grab number and names of all attributes and uniforms and get their locations (needs to be done before validation, b/c we have to set sampler locations)
	GLint attr_count = 0, uni_count = 0, max_attr_len = 0, max_uni_len = 0;
	GLint var_location = 0;
	GLint var_size = 0;
	GLenum var_type = 0;
	
	glGetProgramiv(shd_obj.program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attr_len);
	glGetProgramiv(shd_obj.program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uni_len);
	glGetProgramiv(shd_obj.program, GL_ACTIVE_ATTRIBUTES, &attr_count);
	glGetProgramiv(shd_obj.program, GL_ACTIVE_UNIFORMS, &uni_count);
	max_attr_len+=2;
	max_uni_len+=2;
	
	// note: this may report weird attribute/uniform names (and locations), if uniforms/attributes are optimized away by the compiler
	GLchar* attr_name = new GLchar[(size_t)max_attr_len];
	for(GLint attr = 0; attr < attr_count; attr++) {
		memset(attr_name, 0, (size_t)max_attr_len);
		GLsizei written_size = 0;
		glGetActiveAttrib(shd_obj.program, (GLuint)attr, max_attr_len-1, &written_size, &var_size, &var_type, attr_name);
		var_location = glGetAttribLocation(shd_obj.program, attr_name);
		if(var_location < 0) {
			continue;
		}
		string attribute_name(attr_name, (size_t)written_size);
		if(attribute_name.find("[") != string::npos) attribute_name = attribute_name.substr(0, attribute_name.find("["));
		shd_obj.attributes.emplace(attribute_name,
								   img_shader_object::internal_shader_object::shader_variable {
									   (size_t)var_location,
									   (size_t)var_size,
									   var_type });
	}
	delete [] attr_name;
	
	GLchar* uni_name = new GLchar[(size_t)max_uni_len];
	for(GLint uniform = 0; uniform < uni_count; uniform++) {
		memset(uni_name, 0, (size_t)max_uni_len);
		GLsizei written_size = 0;
		glGetActiveUniform(shd_obj.program, (GLuint)uniform, max_uni_len-1, &written_size, &var_size, &var_type, uni_name);
		var_location = glGetUniformLocation(shd_obj.program, uni_name);
		if(var_location < 0) {
			continue;
		}
		string uniform_name(uni_name, (size_t)written_size);
		if(uniform_name.find("[") != string::npos) uniform_name = uniform_name.substr(0, uniform_name.find("["));
		shd_obj.uniforms.emplace(uniform_name,
								 img_shader_object::internal_shader_object::shader_variable {
									 (size_t)var_location,
									 (size_t)var_size,
									 var_type });
		
		// if the uniform is a sampler, add it to the sampler mapping (with increasing id/num)
		// also: use shader_gl3 here, because we can't use shader_base directly w/o instantiating it
		if(is_gl_sampler_type(var_type)) {
			shd_obj.samplers.emplace(uniform_name, shd_obj.samplers.size());
			
			// while we are at it, also set the sampler location to a dummy value (this has to be done to satisfy program validation)
			glUniform1i(var_location, (GLint)shd_obj.samplers.size()-1);
		}
	}
	delete [] uni_name;
	
	// validate the program object
	glValidateProgram(shd_obj.program);
	glGetProgramiv(shd_obj.program, GL_VALIDATE_STATUS, &success);
	if(!success) {
		glGetProgramInfoLog(shd_obj.program, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in program \"%s\" validation!\nInfo log: %s", shd.name, info_log);
		return false;
	}
	else {
		glGetProgramInfoLog(shd_obj.program, SHADER_LOG_SIZE, nullptr, info_log);
		
		// check if shader will run in software (if so, print out a debug message)
		if(strstr((const char*)info_log, (const char*)"software") != nullptr) {
			log_debug("program \"%s\" validation: %s", shd.name, info_log);
		}
	}
	
	//
	glUseProgram(0);
	return true;
}

static void gl_render(shared_ptr<compute_queue> dev_queue floor_unused, shared_ptr<compute_image> img) {
	// draws ogl stuff
	// don't need to clear, drawing the complete screen
	
	//img->acquire_opengl_object(dev_queue);
	
	const auto shd = shader_objects["IMG_DRAW"];
	glUseProgram(shd->program.program);
	
	glUniform1i((GLint)shd->program.uniforms["tex"].location, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, img->get_opengl_object());
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo_fullscreen_triangle);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glUseProgram(0);
	
	//img->release_opengl_object(dev_queue);
}

static bool compile_shaders() {
	static const char img_vs_text[] { u8R"RAWSTR(#version 150 core
		in vec2 in_vertex;
		out vec2 tex_coord;
		void main() {
			tex_coord = in_vertex.xy * 0.5 + 0.5;
			gl_Position = vec4(in_vertex.xy, 0.0, 1.0);
		}
	)RAWSTR"};
	static const string img_fs_text { u8R"RAWSTR(#version 150 core
		uniform sampler2D tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			frag_color = texture(tex, tex_coord);
		}
	)RAWSTR"};
	
	{
		auto shd = make_shared<img_shader_object>("IMG_DRAW");
		if(!compile_shader(*shd.get(), &img_vs_text[0], &img_fs_text[0])) return false;
		shader_objects.emplace(shd->name, shd);
	}
	return true;
}

static bool gl_init() {
	floor::init_gl();
	glGenVertexArrays(1, &global_vao);
	glBindVertexArray(global_vao);
	
	// create fullscreen triangle/quad vbo
	static constexpr const float fullscreen_triangle[6] { 1.0f, 1.0f, 1.0f, -3.0f, -3.0f, 1.0f };
	glGenBuffers(1, &vbo_fullscreen_triangle);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_fullscreen_triangle);
	glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float2), fullscreen_triangle, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glViewport(0, 0, (GLsizei)floor::get_width(), (GLsizei)floor::get_height());
	
	//
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glFrontFace(GL_CW);
	
	return compile_shaders();
}

/////////////////
int main(int, char* argv[]) {
	// handle options
	img_option_context option_ctx;
	img_opt_handler::parse_options(argv + 1, option_ctx);
	if(done) return 0;
	
	// init floor
#if !defined(FLOOR_IOS)
	floor::init(argv[0], (const char*)"../../data/", // call path, data path
				false, "config.xml", // console-mode, config name
				true); // use opengl 3.2+ core
#else
	floor::init(argv[0], (const char*)"data/");
#endif
	floor::set_caption("img");
	
	// opengl and floor context handling
	if(no_opengl) floor::set_use_gl_context(false);
	floor::acquire_context();
	
	if(!no_opengl) gl_init();
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
												   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN,
												   EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE,
												   EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
												   EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE);
	
	// get the compute context that has been automatically created (opencl/cuda/metal/host)
	auto compute_ctx = floor::get_compute_context();
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	auto fastest_device = compute_ctx->get_device(compute_device::TYPE::FASTEST);
	auto dev_queue = compute_ctx->create_queue(fastest_device);
	
	// compile the program and get the kernel functions
	static constexpr const uint32_t tap_count { 17u };
	static_assert(tap_count % 2u == 1u, "tap count must be an odd number!");
	static constexpr const uint32_t overlap { tap_count / 2u };
	static constexpr const uint32_t inner_tile_size { 16u }; // -> effective tile size
	static constexpr const uint32_t tile_size { inner_tile_size + overlap * 2u };
	static const uint2 global_size { (image_size / inner_tile_size) * tile_size };
	log_debug("running blur kernel on an %v image, with a tap count of %u, inner tile size of %v and work-group tile size of %v -> global work size: %v -> %u texture fetches",
			  image_size, tap_count, inner_tile_size, tile_size, global_size, global_size.x * global_size.y);
#if !defined(FLOOR_IOS)
	auto img_prog = compute_ctx->add_program_file(floor::data_path("../img/src/img_kernels.cpp"),
												  "-I" + floor::data_path("../img/src") +
												  " -DTAP_COUNT=" + to_string(tap_count) +
												  " -DTILE_SIZE=" + to_string(tile_size) +
												  " -DINNER_TILE_SIZE=" + to_string(inner_tile_size) +
												  // TODO: add option
												  " -DDOUBLE_CACHE=1");
#else
	// for now: use a precompiled metal lib instead of compiling at runtime
	const vector<llvm_compute::kernel_info> kernel_infos {
		{
			"image_blur",
			{ 8, 8 },
			{
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
			}
		},
	};
	auto img_prog = compute_ctx->add_precompiled_program_file(floor::data_path("img.metallib"), kernel_infos);
#endif
	if(img_prog == nullptr) {
		log_error("program compilation failed");
		return -1;
	}
	auto image_blur = img_prog->get_kernel_fuzzy("image_blur");
	if(image_blur == nullptr) {
		log_error("failed to retrieve kernel from program");
		return -1;
	}
	
	// create images
	static constexpr const size_t img_count { 2 };
	auto img_data = make_unique<uchar4[]>(image_size.x * image_size.y); // allocated at runtime so it doesn't kill the stack
	array<shared_ptr<compute_image>, img_count> imgs;
	for(size_t img_idx = 0; img_idx < img_count; ++img_idx) {
		if(img_idx == 0) {
			for(uint32_t i = 0, count = image_size.x * image_size.y; i < count; ++i) {
				//img_data[i] = { uchar3::random(), 255u };
				img_data[i] = {
					uchar3(uint8_t(i * (image_size.x / 2) % 255u),
						   uint8_t((i * 255u) / count),
						   uint8_t(((i % image_size.x) * 255u) / image_size.x)),
					255u
				};
			}
		}
		imgs[img_idx] = compute_ctx->create_image(fastest_device,
												  // using a uint2 here, although parameter is actually a uint4 to support 3D/cube-maps/arrays/etc.,
												  // conversion happens automatically
												  image_size,
												  // this is a simple 2D image
												  COMPUTE_IMAGE_TYPE::IMAGE_2D |
												  // each channel is an unsigned 8-bit value
												  COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_8 |
												  // use 4 channels (could also use ::RGBA)
												  COMPUTE_IMAGE_TYPE::CHANNELS_4 |
												  // first image: read only, second image: write only (also sets NO_SAMPLER flag)
												  (img_idx == 0 ? COMPUTE_IMAGE_TYPE::READ : COMPUTE_IMAGE_TYPE::WRITE),
												  // init image with our random data
												  (img_idx == 0 ? &img_data[0] : nullptr),
												  // kernel will read from the first image and write to the second
												  (img_idx == 0 ? COMPUTE_MEMORY_FLAG::READ : COMPUTE_MEMORY_FLAG::WRITE) |
												  // w/ opengl: host will only write, w/o opengl: host will read and write
												  (no_opengl ? COMPUTE_MEMORY_FLAG::HOST_READ_WRITE : COMPUTE_MEMORY_FLAG::HOST_WRITE) |
												  // we want to render the images, so enable opengl sharing
												  (no_opengl ? COMPUTE_MEMORY_FLAG::NONE : COMPUTE_MEMORY_FLAG::OPENGL_SHARING),
												  // when using opengl sharing: appropriate texture target must be set
												  GL_TEXTURE_2D);
	}
	
	glFlush(); glFinish();
	dev_queue->finish();
	const auto blur_start = floor_timer2::start();
	dev_queue->execute(image_blur,
					   // total amount of work:
					   size2 { global_size },
					   // work per work-group:
					   size2 { tile_size, tile_size },
					   // kernel arguments:
					   imgs[0], imgs[1]);
	dev_queue->finish();
	const auto blur_end = floor_timer2::stop<chrono::microseconds>(blur_start);
	log_debug("blur run in %fms", double(blur_end) / 1000.0);
	cur_image = 1;
	
	// acquire for opengl use
	for(size_t img_idx = 0; img_idx < img_count; ++img_idx) {
		imgs[img_idx]->acquire_opengl_object(dev_queue);
	}
	
	// init done, release context
	floor::release_context();
	
	// main loop
	while(!done) {
		floor::get_event()->handle_events();
		
		if(floor::is_new_fps_count()) {
			floor::set_caption("img | FPS: " + to_string(floor::get_fps()));
		}
		
		// TODO: s/w rendering
		if(no_opengl) {
			// grab the current image buffer data (read-only + blocking) ...
			auto render_img = (uchar4*)imgs[cur_image]->map(dev_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
			
			// ... and blit it into the window
			const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
			SDL_LockSurface(wnd_surface);
			const auto pitch_offset = ((size_t)wnd_surface->pitch / sizeof(uint32_t)) - (size_t)image_size.x;
			uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels;
			for(uint32_t y = 0, img_idx = 0; y < image_size.y; ++y) {
				for(uint32_t x = 0; x < image_size.x; ++x, ++img_idx) {
					*px_ptr++ = SDL_MapRGB(wnd_surface->format, render_img[img_idx].x, render_img[img_idx].y, render_img[img_idx].z);
				}
				px_ptr += pitch_offset;
			}
			imgs[cur_image]->unmap(dev_queue, render_img);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
		// opengl rendering
		else if(!no_opengl) {
			floor::start_draw();
			gl_render(dev_queue, imgs[cur_image]);
			floor::stop_draw();
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	if(!no_opengl) {
		// need to kill off the shared opengl buffers before floor kills the opengl context, otherwise bad things(tm) will happen
		floor::acquire_context();
		for(size_t img_idx = 0; img_idx < img_count; ++img_idx) {
			imgs[img_idx] = nullptr;
		}
		floor::release_context();
	}
	
	// kthxbye
	floor::destroy();
	return 0;
}
