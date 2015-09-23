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
#include <floor/core/option_handler.hpp>
#include <floor/core/timer.hpp>
#include <floor/core/gl_shader.hpp>
#include "gl_blur.hpp"
#include "img_kernels.hpp"

struct img_option_context {
	// unused
	string additional_options { "" };
};
typedef option_handler<img_option_context> img_opt_handler;

static bool done { false };
static bool no_opengl {
#if !defined(FLOOR_IOS)
	false
#else
	true
#endif
};
static bool run_gl_blur { false };
static bool second_cache { true };
static bool dumb {
#if !defined(FLOOR_IOS)
	false
#else
	true
#endif
};
static uint32_t cur_image { 0 };
static uint2 image_size { 1024 };
static constexpr const uint32_t tap_count { TAP_COUNT };

//! option -> function map
template<> vector<pair<string, img_opt_handler::option_function>> img_opt_handler::options {
	{ "--help", [](img_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--dim <width> <height>: image width * height in px (default: " << image_size << ")" << endl;
		cout << "\t--no-opengl: disables opengl rendering and sharing (uses s/w rendering instead)" << endl;
		cout << "\t--gl-blur: runs the opengl/glsl blur shader instead of the compute one" << endl;
		cout << "\t--no-second-cache: disables the use of a second (smaller) local memory cache in the kernel" << endl;
		cout << "\t--dumb: runs the \"dumb\" version of the compute kernel (no caching)" << endl;
		
		cout << endl;
		cout << "controls:" << endl;
		cout << "\tq: quit" << endl;
		cout << "\t1: show original image" << endl;
		cout << "\t2: show blurred image" << endl;
		cout << "\t3: show intermediate image" << endl;
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
	{ "--gl-blur", [](img_option_context&, char**&) {
		run_gl_blur = true;
		cout << "running gl-blur" << endl;
	}},
	{ "--no-second-cache", [](img_option_context&, char**&) {
		second_cache = false;
		cout << "disabled second cache" << endl;
	}},
	{ "--dumb", [](img_option_context&, char**&) {
		dumb = true;
		cout << "running dumb kernels" << endl;
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
			case SDLK_3:
				cur_image = 2;
				break;
			case SDLK_w:
				cur_image = (cur_image + 1) % 3;
				break;
			default: break;
		}
		return true;
	}
	return false;
}

/////////////////
// TODO: creata a common header/source file ...
		
#if !defined(FLOOR_IOS)
static GLuint vbo_fullscreen_triangle { 0 };
static uint32_t global_vao { 0 };
#endif
static unordered_map<string, shared_ptr<floor_shader_object>> shader_objects;
		
#if !defined(FLOOR_IOS)
static void gl_render(shared_ptr<compute_queue> dev_queue floor_unused, shared_ptr<compute_image> img) {
	// draws ogl stuff
	// don't need to clear, drawing the complete screen
	glViewport(0, 0, (GLsizei)floor::get_width(), (GLsizei)floor::get_height());
	
	//img->release_opengl_object(dev_queue);
	
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
	
	//img->acquire_opengl_object(dev_queue);
}

static bool compile_shaders() {
	static const char img_vs_text[] { u8R"RAWSTR(#version 150 core
		in vec2 in_vertex;
		out vec2 tex_coord;
		void main() {
			tex_coord = in_vertex.xy * 0.5 + 0.5;
			gl_Position = vec4(in_vertex.xy, 0.0, 1.0);
		})RAWSTR" };
	static const char img_fs_text[] { u8R"RAWSTR(#version 150 core
		uniform sampler2D tex;
		in vec2 tex_coord;
		out vec4 frag_color;
		void main() {
			frag_color = texture(tex, vec2(tex_coord.x, 1.0 - tex_coord.y));
		}
	)RAWSTR" };
	
	{
		auto shd = make_shared<floor_shader_object>("IMG_DRAW");
		if(!floor_compile_shader(*shd.get(), &img_vs_text[0], &img_fs_text[0])) return false;
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
	
	//
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glDisable(GL_BLEND);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glFrontFace(GL_CW);
	
	return compile_shaders();
}
#endif

/////////////////
int main(int, char* argv[]) {
	// handle options
	img_option_context option_ctx;
	img_opt_handler::parse_options(argv + 1, option_ctx);
	if(done) return 0;
	
	// init floor
#if !defined(FLOOR_IOS)
	floor::init(argv[0], (const char*)"../../data/", // call path, data path
				false, "config.json", // console-mode, config name
				true); // use opengl 3.3+ (core)
#else
	floor::init(argv[0], (const char*)"data/");
#endif
	floor::set_caption("img");
	
	// disable opengl when using metal
	if(!no_opengl) {
		no_opengl = (floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::METAL);
	}
	
	// opengl and floor context handling
	if(no_opengl) floor::set_use_gl_context(false);
	floor::acquire_context();
	
	if(!no_opengl) {
#if !defined(FLOOR_IOS)
		if(!gl_init()) return -1;
		if(run_gl_blur && !gl_blur::init(image_size, tap_count)) return -1;
#endif
	}
	
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
	static_assert(tap_count % 2u == 1u, "tap count must be an odd number!");
	static constexpr const uint32_t overlap { tap_count / 2u };
	static constexpr const uint32_t inner_tile_size { 16u }; // -> effective tile size
	static constexpr const uint32_t tile_size { inner_tile_size + overlap * 2u };
	static const uint2 img_global_size { (image_size / inner_tile_size) * tile_size };
	log_debug("running blur kernel on an %v image, with a tap count of %u, inner tile size of %v and work-group tile size of %v -> global work size: %v -> %u texture fetches",
			  image_size, tap_count, inner_tile_size, tile_size, img_global_size, img_global_size.x * img_global_size.y);
#if !defined(FLOOR_IOS)
	auto img_prog = compute_ctx->add_program_file(floor::data_path("../img/src/img_kernels.cpp"),
												  "-I" + floor::data_path("../img/src") +
												  " -DTAP_COUNT=" + to_string(tap_count) +
												  " -DTILE_SIZE=" + to_string(tile_size) +
												  " -DINNER_TILE_SIZE=" + to_string(inner_tile_size) +
												  (second_cache ? " -DSECOND_CACHE=1" : ""));
#else
	// for now: use a precompiled metal lib instead of compiling at runtime
	const vector<llvm_compute::kernel_info> kernel_infos {
		// non-functional right now
		/*{
			"image_blur_single_stage",
			{
				llvm_compute::kernel_info::kernel_arg_info { .size = 0, llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE },
				llvm_compute::kernel_info::kernel_arg_info { .size = 0, llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE },
			}
		},*/
		{
			"image_blur_dumb_horizontal",
			{
				llvm_compute::kernel_info::kernel_arg_info { .size = 0, llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE },
				llvm_compute::kernel_info::kernel_arg_info { .size = 0, llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE },
			}
		},
		{
			"image_blur_dumb_vertical",
			{
				llvm_compute::kernel_info::kernel_arg_info { .size = 0, llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE },
				llvm_compute::kernel_info::kernel_arg_info { .size = 0, llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE },
			}
		},
	};
	auto img_prog = compute_ctx->add_precompiled_program_file(floor::data_path("img_kernels.metallib"), kernel_infos);
#endif
	if(img_prog == nullptr) {
		log_error("program compilation failed");
		return -1;
	}
	auto image_blur = img_prog->get_kernel("image_blur_single_stage");
	auto image_blur_dumb_v = img_prog->get_kernel("image_blur_dumb_vertical");
	auto image_blur_dumb_h = img_prog->get_kernel("image_blur_dumb_horizontal");
	if(
#if !defined(FLOOR_IOS)
	   image_blur == nullptr ||
#endif
	   image_blur_dumb_v == nullptr ||
	   image_blur_dumb_h == nullptr) {
		log_error("failed to retrieve kernel from program");
		return -1;
	}
	
	// create images
	static constexpr const size_t img_count { 3 };
	auto img_data = make_unique<uchar4[]>(image_size.x * image_size.y); // allocated at runtime so it doesn't kill the stack
	array<shared_ptr<compute_image>, img_count> imgs;
	for(size_t img_idx = 0; img_idx < img_count; ++img_idx) {
		if(img_idx == 0) {
			for(uint32_t i = 0, count = image_size.x * image_size.y; i < count; ++i) {
				//img_data[i] = { uchar3::random(), 255u };
				/*img_data[i] = {
					uchar3(uint8_t(i * (image_size.x / 2) % 255u),
						   uint8_t((i * 255u) / count),
						   uint8_t(((i % image_size.x) * 255u) / image_size.x)),
					255u
				};*/
				img_data[i] = {
					uchar4(uint8_t(i * (image_size.x / 2) % 255u),
						   uint8_t((i * 255u) / count),
						   uint8_t(((i % image_size.x) * 255u) / image_size.x),
						   uint8_t(core::rand(0, 255)))
				};
			}
		}
		imgs[img_idx] = compute_ctx->create_image(fastest_device,
												  // using a uint2 here, although parameter is actually a uint4 to support 3D/cube-maps/arrays/etc.,
												  // conversion happens automatically
												  image_size,
												  // this is a simple 2D image, using 4 channels (CHANNELS_4), unsigned int data (UINT) and normalized 8-bit per channel (FLAG_NORMALIZED + FORMAT_8)
												  // -> IMAGE_2D + convenience alias RGBA8UI_NORM which does the things specified above (or just RGBA8 for opengl compat)
												  COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI_NORM |
												  // first image: read only, second image: write only (also sets NO_SAMPLER flag), third image: read/write
												  (img_idx == 0 ? COMPUTE_IMAGE_TYPE::READ : (img_idx == 1 ? COMPUTE_IMAGE_TYPE::READ_WRITE : COMPUTE_IMAGE_TYPE::READ_WRITE)),
												  // init image with our random data, or nothing
												  (img_idx == 0 ? &img_data[0] : nullptr),
												  // NOTE: COMPUTE_MEMORY_FLAG r/w flags are inferred automatically
												  // w/ opengl: host will only write, w/o opengl: host will read and write
												  (no_opengl ? COMPUTE_MEMORY_FLAG::HOST_READ_WRITE : COMPUTE_MEMORY_FLAG::HOST_WRITE) |
												  // we want to render the images, so enable opengl sharing
												  (no_opengl ? COMPUTE_MEMORY_FLAG::NONE : COMPUTE_MEMORY_FLAG::OPENGL_SHARING),
												  // when using opengl sharing: appropriate texture target must be set
												  GL_TEXTURE_2D);
		if(img_idx > 0) imgs[img_idx]->zero(dev_queue);
	}
	
	// flush/finish everything (init, data copy) before running the benchmark
	glFlush(); glFinish();
	dev_queue->finish();
	
	//
	static constexpr const size_t run_count { 20 };
	
	// -> compute blur
	if(!run_gl_blur) {
		log_debug("running %scompute blur ...", (dumb ? "dumb " : ""));
		for(size_t i = 0; i < run_count; ++i) {
			if(!dumb) {
				const auto blur_start = floor_timer2::start();
				dev_queue->execute(image_blur,
								   // total amount of work:
								   img_global_size,
								   // work per work-group:
								   uint2 { tile_size, tile_size },
								   // kernel arguments:
								   imgs[0], imgs[1]);
				dev_queue->finish();
				const auto blur_end = floor_timer2::stop<chrono::microseconds>(blur_start);
				log_debug("blur run in %fms", double(blur_end) / 1000.0);
			}
			else {
				const auto blur_start = floor_timer2::start();
				dev_queue->execute(image_blur_dumb_h,
								   // total amount of work:
								   image_size,
								   // work per work-group:
								   uint2 { tile_size * 4, 2 },
								   // kernel arguments:
								   imgs[0], imgs[2]);
				dev_queue->execute(image_blur_dumb_v,
								   // total amount of work:
								   image_size,
								   // work per work-group:
								   uint2 { 2, tile_size * 4 },
								   // kernel arguments:
								   imgs[2], imgs[1]);
				dev_queue->finish();
				const auto blur_end = floor_timer2::stop<chrono::microseconds>(blur_start);
				log_debug("dumb blur run in %fms", double(blur_end) / 1000.0);
			}
		}
	}
	
	// acquire for opengl use
	if(!no_opengl) {
		for(size_t img_idx = 0; img_idx < img_count; ++img_idx) {
			imgs[img_idx]->release_opengl_object(dev_queue);
		}
	}
	
#if !defined(FLOOR_IOS)
	// -> opengl/glsl blur
	if(run_gl_blur) {
		// flush/finish again after acquire (just to be sure _everything_ has completed)
		dev_queue->finish();
		glFlush(); glFinish();
		
		log_debug("running gl blur ...");
		for(size_t i = 0; i < run_count; ++i) {
			const auto gl_blur_start = floor_timer2::start();
			gl_blur::blur(imgs[0]->get_opengl_object(), imgs[1]->get_opengl_object(), imgs[2]->get_opengl_object(), vbo_fullscreen_triangle);
			glFlush(); glFinish();
			const auto gl_blur_end = floor_timer2::stop<chrono::microseconds>(gl_blur_start);
			log_debug("blur run in %fms", double(gl_blur_end) / 1000.0);
		}
	}
#endif
	
	// render output image by default
	cur_image = 1;
	
	// init done, release context
	floor::release_context();
	
	// main loop
	while(!done) {
		floor::get_event()->handle_events();
		
		if(floor::is_new_fps_count()) {
			floor::set_caption("img | FPS: " + to_string(floor::get_fps()));
		}
		
		// s/w rendering
		if(no_opengl) {
			// grab the current image buffer data (read-only + blocking) ...
			auto render_img = (uchar4*)imgs[cur_image]->map(dev_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
			
			// ... and blit it into the window
			const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
			SDL_LockSurface(wnd_surface);
			const uint2 render_dim = image_size.minned(uint2 { floor::get_width(), floor::get_height() });
			for(uint32_t y = 0; y < render_dim.y; ++y) {
				uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels + ((size_t)wnd_surface->pitch / sizeof(uint32_t)) * y;
				uint32_t img_idx = image_size.x * y;
				for(uint32_t x = 0; x < render_dim.x; ++x, ++img_idx) {
					*px_ptr++ = SDL_MapRGB(wnd_surface->format, render_img[img_idx].x, render_img[img_idx].y, render_img[img_idx].z);
				}
			}
			imgs[cur_image]->unmap(dev_queue, render_img);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
		// opengl rendering
		else if(!no_opengl) {
			floor::start_draw();
#if !defined(FLOOR_IOS)
			gl_render(dev_queue, imgs[cur_image]);
#endif
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
