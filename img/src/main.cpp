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

#include <floor/floor/floor.hpp>
#include <floor/core/option_handler.hpp>
#include <floor/core/timer.hpp>
#include <floor/compute/compute_kernel.hpp>
#include "img_kernels.hpp"

struct img_option_context {
	// unused
	string additional_options { "" };
};
typedef option_handler<img_option_context> img_opt_handler;

static bool done { false };
static bool use_half { false };
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
		cout << "\t--dumb: runs the \"dumb\" version of the compute kernel (no caching)" << endl;
		cout << "\t--half: using half precision computations instead of single precision" << endl;
		
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
		
		if ((image_size.x % 32u) != 0 || (image_size.y % 32u) != 0) {
			cerr << "image dims must be a multiple of 32: " << image_size.x << "x" << image_size.y << endl;
			done = true;
			return;
		}
		
		cout << "image size set to: " << image_size << endl;
	}},
	{ "--dumb", [](img_option_context&, char**&) {
		dumb = true;
		cout << "running dumb kernels" << endl;
	}},
	{ "--half", [](img_option_context&, char**&) {
		use_half = true;
		cout << "using half precision for computations" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](img_option_context&, char**&) {} },
	{ "-ApplePersistenceIgnoreState", [](img_option_context&, char**&) {} },
};

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_Q:
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
			case SDLK_W:
				cur_image = (cur_image + 1) % 3;
				break;
			default: break;
		}
		return true;
	}
	return false;
}

enum SINGLE_STAGE_BLUR_KERNEL {
	SINGLE_STAGE_BLUR_1024_32_F32,
	SINGLE_STAGE_BLUR_256_16_F32,
	SINGLE_STAGE_BLUR_64_8_F32,
	SINGLE_STAGE_BLUR_1024_32_F16,
	SINGLE_STAGE_BLUR_256_16_F16,
	SINGLE_STAGE_BLUR_64_8_F16,
	__MAX_SINGLE_STAGE_BLUR_KERNEL
};
static constexpr const array single_stage_blur_kernel_names {
	"image_blur_single_stage_32x32_f32"sv,
	"image_blur_single_stage_16x16_f32"sv,
	"image_blur_single_stage_8x8_f32"sv,
	"image_blur_single_stage_32x32_f16"sv,
	"image_blur_single_stage_16x16_f16"sv,
	"image_blur_single_stage_8x8_f16"sv,
};
static_assert(size(single_stage_blur_kernel_names) == __MAX_SINGLE_STAGE_BLUR_KERNEL);

//! profiling wrapper that either use compute_queue profiling (if available) or otherwise falls back to use floor_timer
class start_stop_profiling {
public:
	start_stop_profiling(const compute_queue& dev_queue_) : dev_queue(dev_queue_), queue_profiling(dev_queue_.has_profiling_support()) {
		if (queue_profiling) {
			dev_queue.start_profiling();
		} else {
			profiling_start = floor_timer::start();
		}
	}
	
	//! stops the timer / profiling and returns the time in milliseconds since the start
	double stop() {
		uint64_t microseconds = 0u;
		if (queue_profiling) {
			microseconds = dev_queue.stop_profiling();
		} else {
			microseconds = floor_timer::stop<chrono::microseconds>(profiling_start);
		}
		return double(microseconds) / 1000.0;
	}
	
protected:
	const compute_queue& dev_queue;
	const bool queue_profiling { false };
	floor_timer::timer_clock_type::time_point profiling_start {};
};

int main(int, char* argv[]) {
	// handle options
	img_option_context option_ctx;
	img_opt_handler::parse_options(argv + 1, option_ctx);
	if (done) return 0;
	
	// init floor
	if (!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "img",
		.renderer = floor::RENDERER::DEFAULT,
		.context_flags = COMPUTE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING | COMPUTE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING,
	})) {
		return -1;
	}
	floor::set_screen_size(image_size);
	
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
	auto dev_queue = compute_ctx->create_queue(*fastest_device);
	
	// compile the program and get the kernel functions
	static_assert(tap_count % 2u == 1u, "tap count must be an odd number!");
#if !defined(FLOOR_IOS)
	auto img_prog = compute_ctx->add_program_file(floor::data_path("../img/src/img_kernels.cpp"),
												  "-I" + floor::data_path("../img/src") +
												  " -DTAP_COUNT=" + to_string(tap_count));
#else
	auto img_prog = compute_ctx->add_universal_binary(floor::data_path("img_kernels.fubar"));
#endif
	if (!img_prog) {
		log_error("program compilation failed");
		return -1;
	}
	array<shared_ptr<compute_kernel>, size(single_stage_blur_kernel_names)> single_stage_blur_kernels;
	for (size_t i = 0; i < size(single_stage_blur_kernel_names); ++i) {
		single_stage_blur_kernels[i] = img_prog->get_kernel(single_stage_blur_kernel_names[i]);
		if (!single_stage_blur_kernels[i]) {
			log_warn("failed to retrieve/compile kernel: $", single_stage_blur_kernel_names[i]);
		}
	}
	auto image_blur_dumb_v = img_prog->get_kernel("image_blur_dumb_vertical_"s + (use_half ? "f16" : "f32"));
	auto image_blur_dumb_h = img_prog->get_kernel("image_blur_dumb_horizontal_"s + (use_half ? "f16" : "f32"));
	if (!image_blur_dumb_v || !image_blur_dumb_h) {
		log_error("failed to retrieve kernel from program");
		return -1;
	}
	
	// create images
	static constexpr const size_t img_count { 3 };
	auto img_data = make_unique<uchar4[]>(image_size.x * image_size.y); // allocated at runtime so it doesn't kill the stack
	const auto img_data_size = image_size.x * image_size.y * sizeof(uchar4);
	array<shared_ptr<compute_image>, img_count> imgs;
	for (size_t img_idx = 0; img_idx < img_count; ++img_idx) {
		if (img_idx == 0) {
			for (uint32_t i = 0, count = image_size.x * image_size.y; i < count; ++i) {
				img_data[i] = {
					uchar4(uint8_t(i * (image_size.x / 2) % 255u),
						   uint8_t((i * 255u) / count),
						   uint8_t(((i % image_size.x) * 255u) / image_size.x),
						   uint8_t(core::rand(0, 255)))
				};
			}
		}
		imgs[img_idx] = compute_ctx->create_image(*dev_queue,
												  // using a uint2 here, although parameter is actually a uint4 to support 3D/cube-maps/arrays/etc.,
												  // conversion happens automatically
												  image_size,
												  // this is a simple 2D image, using 4 channels (CHANNELS_4), unsigned int data (UINT) and normalized 8-bit per channel (FLAG_NORMALIZED + FORMAT_8)
												  // -> IMAGE_2D + convenience alias RGBA8UI_NORM which does the things specified above
												  COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::RGBA8UI_NORM |
												  // first image: read only, second image: write only (also sets NO_SAMPLER flag), third image: read/write
												  (img_idx == 0 ? COMPUTE_IMAGE_TYPE::READ : (img_idx == 1 ? COMPUTE_IMAGE_TYPE::READ_WRITE : COMPUTE_IMAGE_TYPE::READ_WRITE)),
												  // init image with our random data, or nothing
												  span<uint8_t> { img_idx == 0 ? (uint8_t*)&img_data[0] : nullptr, img_idx == 0 ? img_data_size : 0u },
												  // NOTE: COMPUTE_MEMORY_FLAG r/w flags are inferred automatically
												  // host will read and write
												  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
		if (img_idx > 0) {
			imgs[img_idx]->zero(*dev_queue);
		}
	}
	
	// flush/finish everything (init, data copy) before running the benchmark
	dev_queue->finish();
	
	// amount of blur runs we want to perform
	static constexpr const size_t run_count { 20 };
	
	// -> compute blur
	{
		log_debug("running $compute blur ...", (dumb ? "dumb " : ""));
		// NOTE: all kernels are run with "wait_until_completion" set to true, since this provides proper synchronization in the absence
		// of automatic or manual synchronization provided by a backend queue (-> automatic resource tracking or manual dev_queue->finish())
		if (!dumb) {
			// figure out which kernel to use
			compute_kernel* single_stage_blur_kernel = nullptr;
			uint32_t single_stage_blur_local_size = 1024u;
			uint32_t kernel_idx = (use_half ? 3u : 0u);
			for (uint32_t i = 0; i < 3; ++i, single_stage_blur_local_size >>= 2u, ++kernel_idx) {
				if (single_stage_blur_local_size <= fastest_device->max_total_local_size &&
					single_stage_blur_kernels[kernel_idx]) {
					const auto kernel_entry = single_stage_blur_kernels[kernel_idx]->get_kernel_entry(*fastest_device);
					if (!kernel_entry || kernel_entry->max_total_local_size < single_stage_blur_local_size) {
						continue;
					}
					single_stage_blur_kernel = single_stage_blur_kernels[kernel_idx].get();
					break;
				}
			}
			if (!single_stage_blur_kernel) {
				log_error("no single stage blur kernel is supported by the device");
				return -1;
			}
			log_debug("using single-stage blur kernel: $", single_stage_blur_kernel_names[kernel_idx]);
			
			// run single-stage blur
			compute_queue::execution_parameters_t exec_params {
				// run as 1D kernel
				.execution_dim = 1u,
				// total amount of work:
				.global_work_size = { image_size.x * image_size.y, 0u, 0u },
				// work per work-group:
				.local_work_size = { single_stage_blur_local_size, 0u, 0u },
				// kernel arguments:
				.args = {
					imgs[0], imgs[1]
				},
				.wait_until_completion = true,
				.debug_label = "blur_single_stage",
			};
			for (size_t i = 0; i < run_count; ++i) {
				start_stop_profiling prof(*dev_queue);
				dev_queue->execute_with_parameters(*single_stage_blur_kernel, exec_params);
				const auto blur_end = prof.stop();
				log_debug("blur run in $ms", blur_end);
			}
		} else {
			compute_queue::execution_parameters_t exec_params_horizontal {
				// run as 2D kernel
				.execution_dim = 2u,
				// total amount of work:
				.global_work_size = image_size,
				// work per work-group:
				.local_work_size = uint2 { 32, 16 },
				// kernel arguments:
				.args = {
					imgs[0], imgs[2]
				},
				.wait_until_completion = true,
				.debug_label = "blur_horizontal",
			};
			compute_queue::execution_parameters_t exec_params_vertical {
				// run as 2D kernel
				.execution_dim = 2u,
				// total amount of work:
				.global_work_size = image_size,
				// work per work-group:
				.local_work_size = uint2 { 32, 16 },
				// kernel arguments:
				.args = {
					imgs[2], imgs[1]
				},
				.wait_until_completion = true,
				.debug_label = "blur_vertical",
			};
			for (size_t i = 0; i < run_count; ++i) {
				start_stop_profiling prof(*dev_queue);
				dev_queue->execute_with_parameters(*image_blur_dumb_h, exec_params_horizontal);
				dev_queue->execute_with_parameters(*image_blur_dumb_v, exec_params_vertical);
				const auto blur_end = prof.stop();
				log_debug("dumb blur run in $ms", blur_end);
			}
		}
	}
	
	// render output image by default
	cur_image = 1;
	
	// main loop
	while(!done) {
		floor::get_event()->handle_events();
		
		if(floor::is_new_fps_count()) {
			floor::set_caption("img | FPS: " + to_string(floor::get_fps()));
		}
		
		// s/w rendering
		{
			// grab the current image buffer data (read-only + blocking) ...
			auto render_img = (uchar4*)imgs[cur_image]->map(*dev_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
			
			// ... and blit it into the window
			const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
			SDL_LockSurface(wnd_surface);
			const uint2 render_dim = image_size.minned(uint2 { floor::get_width(), floor::get_height() });
			const auto px_format_details = SDL_GetPixelFormatDetails(wnd_surface->format);
			for (uint32_t y = 0; y < render_dim.y; ++y) {
				uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels + ((size_t)wnd_surface->pitch / sizeof(uint32_t)) * y;
				uint32_t img_idx = image_size.x * y;
				for (uint32_t x = 0; x < render_dim.x; ++x, ++img_idx) {
					*px_ptr++ = SDL_MapRGB(px_format_details, nullptr, render_img[img_idx].x, render_img[img_idx].y, render_img[img_idx].z);
				}
			}
			imgs[cur_image]->unmap(*dev_queue, render_img);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	// cleanup
	imgs.fill(nullptr);
	
	image_blur_dumb_v = nullptr;
	image_blur_dumb_h = nullptr;
	img_prog = nullptr;
	dev_queue = nullptr;
	compute_ctx = nullptr;
	
	// kthxbye
	floor::destroy();
	return 0;
}
