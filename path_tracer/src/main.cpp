/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

int main(int, char* argv[]) {
	if(!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "path tracer",
		// NOTE: don't need a specific renderer here, so just use the defaults
	})) {
		return -1;
	}
	
	// get the compute context that has been automatically created (opencl/cuda/metal/host, depending on the config)
	auto compute_ctx = floor::get_compute_context();
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	auto fastest_device = compute_ctx->get_device(compute_device::TYPE::FASTEST);
	auto dev_queue = compute_ctx->create_queue(*fastest_device);
	
	//
	static const uint2 img_size { 512, 512 }; // fixed for now, b/c random function makes this look horrible at higher res
	static const uint32_t pixel_count { img_size.x * img_size.y };
	floor::set_screen_size(img_size);
	
	// compile the program and get the kernel function
#if !defined(FLOOR_IOS)
	auto path_tracer_prog = compute_ctx->add_program_file(floor::data_path("../path_tracer/src/path_tracer.cpp"),
														  "-I" + floor::data_path("../path_tracer/src") +
														  " -DSCREEN_WIDTH=" + to_string(img_size.x) +
														  " -DSCREEN_HEIGHT=" + to_string(img_size.y));
#else
	auto path_tracer_prog = compute_ctx->add_universal_binary(floor::data_path("path_tracer.fubar"));
#endif
	if(path_tracer_prog == nullptr) {
		log_error("program compilation failed");
		return -1;
	}
	auto path_tracer_kernel = path_tracer_prog->get_kernel("path_trace");
	if(path_tracer_kernel == nullptr) {
		log_error("failed to retrieve kernel from program");
		return -1;
	}
	
	// create the image buffer on the device
	auto img_buffer = compute_ctx->create_buffer(*dev_queue, sizeof(float4) * pixel_count);
	
	bool done = false;
	static constexpr const uint32_t iteration_count { 16384 };
	for(uint32_t iteration = 0; iteration < iteration_count; ++iteration) {
		dev_queue->execute(*path_tracer_kernel,
						   // total amount of work:
						   uint1 { pixel_count },
						   // work per work-group:
						   uint1 { fastest_device->max_total_local_size },
						   // kernel arguments:
						   img_buffer, iteration, core::rand<uint32_t>());

		// draw every 10th frame (except for the first 10 frames)
		if(iteration < 10 || iteration % 10 == 0) {
			// grab the current image buffer data (read-only + blocking) ...
			auto img_data = (float4*)img_buffer->map(*dev_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);

			// path tracer output needs gamma correction (fixed 2.2), otherwise it'll look too dark
			const auto gamma_correct = [](const float3& color) {
				static constexpr const float gamma { 2.2f };
				const float3 gamma_corrected { (color.powed(1.0f / gamma) * 255.0f).clamp(0.0f, 255.0f) };
				return uchar3 { (uint8_t)gamma_corrected.x, (uint8_t)gamma_corrected.y, (uint8_t)gamma_corrected.z };
			};
			
			// ... and blit it into the window
			// (crude and I'd usually do this via GL, but this way it's not a requirement)
			const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
			SDL_LockSurface(wnd_surface);
			const uint2 render_dim = img_size.minned(uint2 { floor::get_width(), floor::get_height() });
			for(uint32_t y = 0; y < render_dim.y; ++y) {
				uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels + ((size_t)wnd_surface->pitch / sizeof(uint32_t)) * y;
				uint32_t img_idx = img_size.x * y;
				for(uint32_t x = 0; x < render_dim.x; ++x, ++img_idx) {
					// map and gamma correct each pixel according to the window format
					const auto rgb = gamma_correct(img_data[img_idx].xyz);
					*px_ptr++ = SDL_MapRGB(wnd_surface->format, rgb.x, rgb.y, rgb.z);
				}
			}
			img_buffer->unmap(*dev_queue, img_data);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
		floor::set_caption("frame #" + to_string(iteration + 1));
		
		// handle quit event
		SDL_Event event_handle;
		while(SDL_PollEvent(&event_handle)) {
			if(event_handle.type == SDL_QUIT) {
				done = true;
				break;
			}
			else if(event_handle.type == SDL_KEYDOWN) {
				switch(event_handle.key.keysym.sym) {
					case SDLK_q:
					case SDLK_ESCAPE:
						done = true;
						break;
					default: break;
				}
			}
		}
		if(done) break;
	}
	log_msg("done!");
	
	// kthxbye
	path_tracer_kernel = nullptr;
	path_tracer_prog = nullptr;
	img_buffer = nullptr;
	dev_queue = nullptr;
	compute_ctx = nullptr;
	floor::destroy();
	return 0;
}
