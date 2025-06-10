/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include <floor/floor.hpp>
#include <floor/core/option_handler.hpp>
#include <floor/core/core.hpp>

#include <SDL3_image/SDL_image.h>

using namespace fl;
using namespace std;

struct path_tracer_option_context {
	bool done { false };
	bool with_textures { false };
	string additional_options { "" };
};
typedef option_handler<path_tracer_option_context> path_tracer_opt_handler;

static bool done { false };
static uint32_t iteration { 0 };
static shared_ptr<device_buffer> img_buffer;
static shared_ptr<device_queue> dev_queue;

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if (type == EVENT_TYPE::QUIT) {
		done = true;
		return true;
	} else if (type == EVENT_TYPE::KEY_UP) {
		switch (((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_Q:
			case SDLK_ESCAPE:
				done = true;
				break;
			case SDLK_R:
				iteration = 0u;
				img_buffer->zero(*dev_queue);
				break;
			default: break;
		}
		return true;
	}
	return false;
}

//! option -> function map
template<> vector<pair<string, path_tracer_opt_handler::option_function>> path_tracer_opt_handler::options {
	{ "--help", [](path_tracer_option_context& ctx, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--with-textures: enables rendering with simple textures" << endl;
		ctx.done = true;
	}},
	{ "--with-textures", [](path_tracer_option_context& ctx, char**&) {
		ctx.with_textures = true;
		cout << "textures enabled" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](path_tracer_option_context&, char**&) {} },
	{ "-ApplePersistenceIgnoreState", [](path_tracer_option_context&, char**&) {} },
};

static shared_ptr<device_image> load_texture(device_context& ctx, const string& filename) {
	SDL_Surface* surface = IMG_Load(filename.c_str());
	if (surface == nullptr) {
		log_error("error loading texture file \"$\": $!", filename, SDL_GetError());
		return {};
	}
	
	// we only want 8-bit/ch RGB(A) textures that we then convert to single channel 8bpp
	const auto px_format_details = SDL_GetPixelFormatDetails(surface->format);
	if (px_format_details->bits_per_pixel != 24u &&
		px_format_details->bits_per_pixel != 32u) {
		log_error("texture must be 24bpp or 32bpp: \"$\"!", filename);
		SDL_DestroySurface(surface);
		return {};
	}

	// check format, we always want RGB(A)
	// NOTE: SDL uses reverse order ... RGBA is ABGR in SDL
	std::optional<SDL_PixelFormat> conv_format;
	switch (surface->format) {
		default:
			break;
		case SDL_PIXELFORMAT_RGB24:
			conv_format = SDL_PIXELFORMAT_BGR24;
			break;
		case SDL_PIXELFORMAT_XRGB8888:
		case SDL_PIXELFORMAT_RGBX8888:
		case SDL_PIXELFORMAT_XBGR8888:
		case SDL_PIXELFORMAT_BGRX8888:
			conv_format = SDL_PIXELFORMAT_XBGR8888;
			break;
		case SDL_PIXELFORMAT_ARGB8888:
		case SDL_PIXELFORMAT_RGBA8888:
		case SDL_PIXELFORMAT_BGRA8888:
			conv_format = SDL_PIXELFORMAT_ABGR8888;
			break;
	}
	
	if (conv_format) {
		SDL_Surface* new_surface = SDL_ConvertSurface(surface, *conv_format);
		if (new_surface == nullptr) {
			log_error("BGR(A)->RGB(A) surface conversion failed!");
			SDL_DestroySurface(surface);
			return {};
		}
		SDL_DestroySurface(surface);
		surface = new_surface;
	}
	
	const uint4 image_dim { uint32_t(surface->w), uint32_t(surface->h), 0, 0 };
	const auto pixel_count = image_dim.x * image_dim.y;
	auto image_data = make_unique<uint8_t[]>(pixel_count);
	const auto bpp = px_format_details->bytes_per_pixel;
	const auto pitch = (uint32_t)surface->pitch;
	for (uint32_t y = 0; y < image_dim.y; ++y) {
		for (uint32_t x = 0; x < image_dim.x; ++x) {
			image_data[x + y * image_dim.x] = *((uint8_t*)surface->pixels + pitch * y + x * bpp);
		}
	}
	SDL_DestroySurface(surface);
	
	return ctx.create_image(*dev_queue, image_dim,
							IMAGE_TYPE::IMAGE_2D | IMAGE_TYPE::READ | IMAGE_TYPE::R8UI_NORM,
							span { image_data.get(), pixel_count });
}

int main(int, char* argv[]) {
	path_tracer_option_context option_ctx;
	path_tracer_opt_handler::parse_options(argv + 1, option_ctx);
	if (option_ctx.done) return 0;
	
	if (!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "path tracer",
		.renderer = floor::RENDERER::DEFAULT,
		.context_flags = DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING | DEVICE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING,
	})) {
		return -1;
	}
	
	// get the default device context that has been automatically created (CUDA/Host-Compute/Metal/OpenCL/Vulkan, depending on the config or platform default)
	auto ctx = floor::get_device_context();
	
	// create a device queue (aka command queue or stream) for the fastest device in the context
	auto fastest_device = ctx->get_device(device::TYPE::FASTEST);
	dev_queue = ctx->create_queue(*fastest_device);
	
	//
	static const uint2 img_size { 512, 512 }; // fixed for now, b/c random function makes this look horrible at higher res
	static const uint32_t pixel_count { img_size.x * img_size.y };
	floor::set_screen_size(img_size);
	
	// compile the program and get the kernel function
#if !defined(FLOOR_IOS)
	auto path_tracer_prog = ctx->add_program_file(floor::data_path("../path_tracer/src/path_tracer.cpp"),
												  toolchain::compile_options {
		.cli = ("-I\"" + floor::data_path("../path_tracer/src") + "\"" +
				" -DSCREEN_WIDTH=" + to_string(img_size.x) +
				" -DSCREEN_HEIGHT=" + to_string(img_size.y)),
	});
#else
	auto path_tracer_prog = ctx->add_universal_binary(floor::data_path("path_tracer.fubar"));
#endif
	if (path_tracer_prog == nullptr) {
		log_error("program compilation failed");
		return -1;
	}
	auto path_tracer_kernel = path_tracer_prog->get_function(!option_ctx.with_textures ? "path_trace" : "path_trace_textured");
	if (path_tracer_kernel == nullptr) {
		log_error("failed to retrieve kernel from program");
		return -1;
	}
	
	// load textures if specified
	vector<shared_ptr<device_image>> textures;
	if (option_ctx.with_textures) {
		textures = {
			load_texture(*ctx, floor::data_path("white.png")),
			load_texture(*ctx, floor::data_path("textures/cross.png")),
			load_texture(*ctx, floor::data_path("textures/circle.png")),
			load_texture(*ctx, floor::data_path("textures/inv_cross.png")),
			load_texture(*ctx, floor::data_path("textures/stripe.png")),
		};
		for (const auto& tex : textures) {
			if (!tex) {
				return -1;
			}
		}
	}
	
	// create the image buffer on the device
	img_buffer = ctx->create_buffer(*dev_queue, sizeof(float4) * pixel_count);
	
	// init kernel parameters (will update in loop)
	device_queue::execution_parameters_t params {
		// this is a 1D kernel
		.execution_dim = 1u,
		// total amount of work
		.global_work_size = uint1 { pixel_count },
		// work per work-group
		.local_work_size = uint1 { fastest_device->max_total_local_size },
		.args = {
			img_buffer,
			0u, // iteration
			0u, // seed
		},
		// iterations may not overlap, since they access the same img_buffer memory -> block until completed
		.wait_until_completion = true,
	};
	if (option_ctx.with_textures) {
		params.args.emplace_back(textures);
	}
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_internal_event_handler(evt_handler_fnctr, EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP);
	
	static constexpr const uint32_t iteration_count { 16384 };
	for (iteration = 0; iteration < iteration_count; ++iteration) {
		floor::get_event()->handle_events();
		
		params.args[1] = iteration;
		// new seed each iteration, note that this needs to be stored in a referencable value when using params
		const auto seed = core::rand<uint32_t>();
		params.args[2] = seed;
		dev_queue->execute_with_parameters(*path_tracer_kernel, params);

		// draw every 10th frame (except for the first 10 frames)
		if (iteration < 10 || iteration % 10 == 0) {
			// grab the current image buffer data (read-only + blocking) ...
			auto img_data = (float4*)img_buffer->map(*dev_queue, MEMORY_MAP_FLAG::READ | MEMORY_MAP_FLAG::BLOCK);

			// path tracer output needs gamma correction (fixed 2.2), otherwise it'll look too dark
			const auto gamma_correct = [](const float3& color) {
				static constexpr const float gamma { 2.2f };
				const float3 gamma_corrected { (color.powed(1.0f / gamma) * 255.0f).clamp(0.0f, 255.0f) };
				return uchar3 { (uint8_t)gamma_corrected.x, (uint8_t)gamma_corrected.y, (uint8_t)gamma_corrected.z };
			};
			
			// ... and blit it into the window
			const auto wnd = floor::get_window();
			const auto wnd_surface = SDL_GetWindowSurface(wnd);
			const auto scale = uint32_t(SDL_GetWindowDisplayScale(wnd));
			SDL_LockSurface(wnd_surface);
			const uint2 render_dim = img_size.minned(uint2 { floor::get_width(), floor::get_height() });
			const auto px_format_details = SDL_GetPixelFormatDetails(wnd_surface->format);
			for (uint32_t y = 0; y < render_dim.y * scale; ++y) {
				uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels + ((size_t)wnd_surface->pitch / sizeof(uint32_t)) * y;
				for (uint32_t x = 0; x < render_dim.x * scale; ++x) {
					// map and gamma correct each pixel according to the window format
					const auto rgb = gamma_correct(img_data[x / scale + (y / scale) * img_size.x].xyz);
					*px_ptr++ = SDL_MapRGB(px_format_details, nullptr, rgb.x, rgb.y, rgb.z);
				}
			}
			img_buffer->unmap(*dev_queue, img_data);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
		floor::set_caption("frame #" + to_string(iteration + 1));
		
		if (done) {
			break;
		}
	}
	log_msg("done!");
	
	// kthxbye
	textures.clear();
	path_tracer_kernel = nullptr;
	path_tracer_prog = nullptr;
	img_buffer = nullptr;
	dev_queue = nullptr;
	ctx = nullptr;
	floor::destroy();
	return 0;
}
