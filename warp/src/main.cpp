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
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <libwarp/libwarp.h>
#include "unified_renderer.hpp"
#include "obj_loader.hpp"
#include "auto_cam.hpp"
#include "warp_state.hpp"
warp_state_struct warp_state;

struct warp_option_context {
	// unused
	string additional_options { "" };
};
typedef option_handler<warp_option_context> warp_opt_handler;

static unique_ptr<camera> cam;
static const double3 cam_speeds { 75.0 /* default */, 150.0 /* faster */, 10.0 /* slower */ };

static shared_ptr<unified_renderer> uni_renderer;

//! option -> function map
template<> vector<pair<string, warp_opt_handler::option_function>> warp_opt_handler::options {
	{ "--help", [](warp_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--scatter: warp in scatter mode" << endl;
		cout << "\t--gather: warp in gather mode" << endl;
		cout << "\t--gather-forward: warp in gather forward-only mode" << endl;
		cout << "\t--no-warp: disables warping at the start (enable at run-time by pressing '1')" << endl;
		cout << "\t--frames <count>: input frame rate, amount of frames per second that will actually be rendered (via metal/vulkan)" << endl;
		cout << "\t                  NOTE: obviously an upper limit" << endl;
		cout << "\t--target <count>: target frame rate (will use a constant time delta for each compute frame instead of a variable delta)" << endl;
		cout << "\t                  NOTE: if vsync is enabled, this will automatically be set to the appropriate value" << endl;
		cout << "\t--always-render: always perform full scene rendering (warping is disabled), not that --frames/--target will be ignored if enabled" << endl;
		cout << "\t--no-arg-buffer: disables use of argument buffers in the unified renderer (also disables tessellation and indirect)" << endl;
		cout << "\t--no-tessellation: disables tessellation in the unified renderer" << endl;
		cout << "\t--no-indirect: disables indirect render/compute commands/pipelines in the unified renderer" << endl;
		warp_state.done = true;
	}},
	{ "--frames", [](warp_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --frames!" << endl;
			warp_state.done = true;
			return;
		}
		warp_state.render_frame_count = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		cout << "render frame count set to: " << warp_state.render_frame_count << endl;
	}},
	{ "--target", [](warp_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --target!" << endl;
			warp_state.done = true;
			return;
		}
		warp_state.target_frame_count = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		cout << "target frame count set to: " << warp_state.target_frame_count << endl;
	}},
	{ "--scatter", [](warp_option_context&, char**&) {
		warp_state.is_scatter = true;
		warp_state.is_gather_forward = false;
		cout << "using scatter" << endl;
	}},
	{ "--gather", [](warp_option_context&, char**&) {
		warp_state.is_scatter = false;
		warp_state.is_gather_forward = false;
		cout << "using gather" << endl;
	}},
	{ "--gather-forward", [](warp_option_context&, char**&) {
		warp_state.is_scatter = false;
		warp_state.is_gather_forward = true;
		cout << "using gather-forward" << endl;
	}},
	{ "--no-warp", [](warp_option_context&, char**&) {
		warp_state.is_warping = false;
		cout << "warping disabled" << endl;
	}},
	{ "--always-render", [](warp_option_context&, char**&) {
		warp_state.is_always_render = true;
		cout << "always rendering (warping fully disabled)" << endl;
	}},
	{ "--no-metal", [](warp_option_context&, char**&) {
		warp_state.no_metal = true;
		cout << "metal disabled" << endl;
	}},
	{ "--no-vulkan", [](warp_option_context&, char**&) {
		warp_state.no_vulkan = true;
		cout << "vulkan disabled" << endl;
	}},
	{ "--no-arg-buffer", [](warp_option_context&, char**&) {
		warp_state.use_argument_buffer = false;
		warp_state.use_tessellation = false;
		warp_state.use_indirect_commands = false;
		cout << "arg-buffer + tessellation + indirect disabled" << endl;
	}},
	{ "--no-tessellation", [](warp_option_context&, char**&) {
		warp_state.use_tessellation = false;
		cout << "tessellation disabled" << endl;
	}},
	{ "--no-indirect", [](warp_option_context&, char**&) {
		warp_state.use_indirect_commands = false;
		cout << "indirect disabled" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](warp_option_context&, char**&) {} },
	{ "-ApplePersistenceIgnoreState", [](warp_option_context&, char**&) {} },
};
static float3 last_cam_pos;
static void cam_dump() {
	log_undecorated("{ { $f, $f, $f }, { $f, $f } },",
					cam->get_position().x, cam->get_position().y, cam->get_position().z,
					cam->get_rotation().x, cam->get_rotation().y);
	last_cam_pos = cam->get_position();
}

static bool compile_program() {
	// reset libwarp programs/kernels
	libwarp_cleanup();
	
	if (uni_renderer) {
		uni_renderer->rebuild_renderer();
	}
	
	return true;
}

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	static constexpr const float eps1_step_size { 0.0005f };
	static constexpr const float eps2_step_size { 0.001f };
	static constexpr const uint32_t gather_max_dbg { 8 };
	
	if(type == EVENT_TYPE::QUIT) {
		warp_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_DOWN) {
		switch(static_pointer_cast<key_down_event>(obj)->key) {
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				cam->set_cam_speed(cam_speeds.y);
				break;
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speeds.z);
				break;
			case SDLK_I:
				warp_state.gather_eps_1 += eps1_step_size;
				warp_state.gather_eps_1 = const_math::clamp(warp_state.gather_eps_1, 0.0f, 32.0f);
				log_debug("eps 1: $", warp_state.gather_eps_1);
				break;
			case SDLK_U:
				warp_state.gather_eps_1 -= eps1_step_size;
				warp_state.gather_eps_1 = const_math::clamp(warp_state.gather_eps_1, 0.0f, 32.0f);
				log_debug("eps 1: $", warp_state.gather_eps_1);
				break;
			case SDLK_K:
				warp_state.gather_eps_2 += eps2_step_size;
				warp_state.gather_eps_2 = const_math::clamp(warp_state.gather_eps_2, 0.0f, 32.0f);
				log_debug("eps 2: $", warp_state.gather_eps_2);
				break;
			case SDLK_J:
				warp_state.gather_eps_2 -= eps2_step_size;
				warp_state.gather_eps_2 = const_math::clamp(warp_state.gather_eps_2, 0.0f, 32.0f);
				log_debug("eps 2: $", warp_state.gather_eps_2);
				break;
			default: break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(static_pointer_cast<key_up_event>(obj)->key) {
			case SDLK_Q:
			case SDLK_ESCAPE:
				warp_state.done = true;
				break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speeds.x);
				break;
			case SDLK_X:
				cam_dump();
				break;
			case SDLK_C:
				compile_program();
				break;
			case SDLK_SPACE:
				warp_state.stop ^= true;
				break;
			case SDLK_V:
				warp_state.is_auto_cam ^= true;
				break;
			case SDLK_R:
				warp_state.is_warping = true;
				warp_state.is_render_full = true;
				warp_state.is_clear_frame = true;
				warp_state.is_fixup = true;
				break;
			case SDLK_1:
				warp_state.is_warping ^= true;
				log_debug("warping? $", warp_state.is_warping);
				break;
			case SDLK_2:
				warp_state.is_render_full ^= true;
				log_debug("render full scene? $", warp_state.is_render_full);
				break;
			case SDLK_3:
				warp_state.is_clear_frame ^= true;
				log_debug("frame clearing? $", warp_state.is_clear_frame);
				break;
			case SDLK_4:
				warp_state.is_fixup ^= true;
				log_debug("px fixup? $", warp_state.is_fixup);
				break;
			case SDLK_5:
				warp_state.is_bidir_scatter ^= true;
				log_debug("bidirectional scatter? $", warp_state.is_bidir_scatter);
				break;
			case SDLK_G:
				warp_state.is_scatter ^= true;
				log_debug("scatter/gather? $", (warp_state.is_scatter ? "scatter" : "gather"));
				break;
			case SDLK_TAB:
				warp_state.is_frame_repeat ^= true;
				log_debug("frame repeat? $", warp_state.is_frame_repeat);
				break;
			case SDLK_P:
				if(warp_state.gather_dbg < gather_max_dbg) ++warp_state.gather_dbg;
				log_debug("dbg: $", warp_state.gather_dbg);
				break;
			case SDLK_O:
				if(warp_state.gather_dbg > 0) --warp_state.gather_dbg;
				log_debug("dbg: $", warp_state.gather_dbg);
				break;
			case SDLK_T:
				warp_state.is_debug_delta ^= true;
				log_debug("debug delta? $", warp_state.is_debug_delta);
				break;
			case SDLK_B:
				warp_state.is_split_view ^= true;
				log_debug("split view? $", warp_state.is_split_view);
				break;
			case SDLK_0: {
				static const uint32_t max_disp_mode = (warp_state.use_tessellation ? 3u : 2u);
				warp_state.displacement_mode = (warp_state.displacement_mode + 1u) % max_disp_mode;
				break;
			}
			case SDLK_KP_0:
				uni_renderer->set_debug_blit_mode(0);
				break;
			case SDLK_KP_1:
				uni_renderer->set_debug_blit_mode(1);
				break;
			case SDLK_KP_2:
				uni_renderer->set_debug_blit_mode(2);
				break;
			case SDLK_KP_3:
				uni_renderer->set_debug_blit_mode(3);
				break;
			case SDLK_KP_4:
				uni_renderer->set_debug_blit_mode(4);
				break;
			case SDLK_KP_5:
				uni_renderer->set_debug_blit_mode(5);
				break;
			default: break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::MOUSE_RIGHT_UP) {
		if(cam != nullptr) {
			cam->set_mouse_input(cam->get_mouse_input() ^ true);
		}
	}
	else if(type == EVENT_TYPE::WINDOW_RESIZE) {
		compile_program();
		return true;
	}
	return false;
}

int main(int, char* argv[]) {
	// handle options
	warp_option_context option_ctx;
	warp_opt_handler::parse_options(argv + 1, option_ctx);
	if(warp_state.done) return 0;
	
	// disable renderers that aren't available
#if defined(FLOOR_NO_METAL)
	warp_state.no_metal = true;
#endif
#if defined(FLOOR_NO_VULKAN)
	warp_state.no_vulkan = true;
#endif
	
	// init floor
	const auto wanted_renderer = (// if neither Metal nor Vulkan is disabled, use the default
								  (!warp_state.no_metal && !warp_state.no_vulkan) ? floor::RENDERER::DEFAULT :
								  // else: choose a specific one
								  !warp_state.no_vulkan ? floor::RENDERER::VULKAN :
								  !warp_state.no_metal ? floor::RENDERER::METAL :
								  // vulkan/metal are disabled
								  floor::RENDERER::NONE);
	if(!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "warp",
		.renderer = wanted_renderer,
		// disable resource tracking and enable non-blocking Vulkan execution
		.context_flags = COMPUTE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING | COMPUTE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING,
	})) {
		return -1;
	}
	
	// disable resp. other renderers when using metal/vulkan
	const auto floor_renderer = floor::get_renderer();
	const bool is_metal = ((floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::METAL ||
							floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::HOST) &&
						   floor_renderer == floor::RENDERER::METAL);
	const bool is_vulkan = ((floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::VULKAN ||
							 floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::CUDA ||
							 floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::HOST) &&
							floor_renderer == floor::RENDERER::VULKAN);
	
	if (floor_renderer == floor::RENDERER::NONE) {
		log_error("no renderer was initialized");
		return -2;
	}
	
	if (is_metal) {
		warp_state.no_vulkan = true;
	} else {
		warp_state.no_metal = true;
	}
	
	if (is_vulkan) {
		if (floor_renderer == floor::RENDERER::VULKAN) {
			warp_state.no_metal = true;
		}
	} else {
		warp_state.no_vulkan = true;
	}
	
	if (!is_metal && !is_vulkan) {
		log_error("neither Metal nor Vulkan is available (or disabled)");
		return -3;
	}
	
	event::handler evt_handler_fnctr(&evt_handler);
	shared_ptr<obj_model> model;
	{
		// add event handlers
		floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
													   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN,
													   EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE,
													   EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
													   EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE,
													   EVENT_TYPE::WINDOW_RESIZE);
		
		cam = make_unique<camera>();
		
		if (!warp_state.is_auto_cam) {
			cam->set_mouse_input(true);
			cam->set_wasd_input(true);
			cam->set_rotation_wrapping(false);
		} else {
			cam->set_mouse_input(false);
			cam->set_wasd_input(false);
		}
		
		cam->set_position(-115.0, 15.0, 0.0);
		cam->set_rotation(0.0, -90.0);
		last_cam_pos = cam->get_position();
		
		// get the compute and render contexts that has been automatically created (opencl/cuda/metal/vulkan/host)
		warp_state.cctx = floor::get_compute_context();
		warp_state.rctx = floor::get_render_context();
		if (!warp_state.rctx) {
			warp_state.rctx = warp_state.cctx;
		}
		
		// create a compute queue (aka command queue or stream) for the fastest device in the context
		warp_state.cdev = warp_state.cctx->get_device(compute_device::TYPE::FASTEST);
		warp_state.cqueue = warp_state.cctx->create_queue(*warp_state.cdev);
		if (warp_state.cctx != warp_state.rctx) {
			// render context is not the same as the compute context -> also create dev and queue for the render context
			warp_state.rdev = warp_state.rctx->get_device(compute_device::TYPE::FASTEST);
			warp_state.rqueue = warp_state.rctx->create_queue(*warp_state.rdev);
		} else {
			// otherwise: use the same as the compute context
			warp_state.rdev = warp_state.cdev;
			warp_state.rqueue = warp_state.cqueue;
		}
		
		// disable argument_buffer use when argument buffers with images are not supported by the device
		warp_state.use_argument_buffer &= warp_state.rdev->argument_buffer_image_support;
		// tessellation requires argument_buffer support
		warp_state.use_tessellation &= warp_state.rdev->tessellation_support & warp_state.use_argument_buffer;
		
		// if there isn't full argument buffer support or there isn't full indirect command support, disable indirect command use
		if (!warp_state.use_argument_buffer ||
			!warp_state.rdev->indirect_command_support ||
			!warp_state.rdev->indirect_render_command_support ||
			!warp_state.rdev->indirect_compute_command_support) {
			warp_state.use_indirect_commands = false;
		}
		
		// tessellation requires argument buffer support
		if (warp_state.use_tessellation && !warp_state.use_argument_buffer) {
			log_error("argument buffer support is required when using tessellation");
			return -1;
		}
		
		log_debug("arg-buffers: $, tessellation: $, indirect: $",
				  warp_state.use_argument_buffer, warp_state.use_tessellation, warp_state.use_indirect_commands);
		
		// if vsync is enabled and the target frame rate isn't set,
		// compute the appropriate value according to the render/input frame rate and display refresh rate
		if ((floor::get_vsync() || warp_state.rctx->get_compute_type() == COMPUTE_TYPE::METAL) &&
			warp_state.target_frame_count == 0) {
			warp_state.target_frame_count = floor::get_window_refresh_rate();
			if (warp_state.target_frame_count == 0) {
				warp_state.target_frame_count = 60;
			}
		}
		if (warp_state.target_frame_count > 0) {
			log_debug("using target frame rate: $", warp_state.target_frame_count);
		} else {
			log_debug("using a variable target frame rate");
		}
		
		//
		if (!compile_program()) {
			return -1;
		}
		
		// init renderers (need compiled prog first)
		uni_renderer = make_shared<unified_renderer>();
		if (!uni_renderer->init()) {
			log_error("error during unified renderer initialization!");
			return -1;
		}
		
		// init displacement mode depending on tessellation support
		warp_state.displacement_mode = (warp_state.use_tessellation ? 2u : 1u);
		
		//
		bool model_success { false };
		model = obj_loader::load(floor::data_path("sponza/sponza.obj"), model_success,
								 *warp_state.rctx, *warp_state.rqueue);
		if(!model_success) {
			return -1;
		}
		
		uni_renderer->post_init(*warp_state.rqueue, *(floor_obj_model*)model.get(), *cam);
		
		// init done
	}
	
	// main loop
	log_debug("first frame");
	while (!warp_state.done) {
		if (uni_renderer) {
			// throttle main loop when rendering and render queueing is off-loaded
			this_thread::sleep_for(500us);
			{
				static auto last_sec_frame = chrono::steady_clock::now();
				const auto cur_time = chrono::steady_clock::now();
				if (cur_time - last_sec_frame > 1s) {
					// update stats
					last_sec_frame = cur_time;
				}
			}
		}
		
		floor::get_event()->handle_events();
		
#if !defined(FLOOR_IOS)
		// stop drawing if window is inactive
		if (!(SDL_GetWindowFlags(floor::get_window()) & SDL_WINDOW_INPUT_FOCUS)) {
			this_thread::sleep_for(20ms);
			continue;
		}
#endif
		
		if (!warp_state.is_frame_repeat && !warp_state.stop) {
			if(!warp_state.is_auto_cam) cam->run();
			else auto_cam::run(*cam);
		}
		
		if (uni_renderer) {
			if (uni_renderer->is_new_fps_count()) {
				const auto& cam_state = cam->get_current_state();
				floor::set_caption("warp | FPS: " + to_string(uni_renderer->get_fps()) +
								   " | Pos: " + cam_state.position.to_string() +
								   " | Rot: " + cam_state.rotation.to_string());
			}
		} else {
			if (floor::is_new_fps_count()) {
				floor::set_caption("warp | FPS: " + to_string(floor::get_fps()) +
								   " | Pos: " + cam->get_position().to_string() +
								   " | Rot: " + cam->get_rotation().to_string());
			}
		}
		
		if (!warp_state.stop) {
			uni_renderer->render();
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	uni_renderer = nullptr;
	libwarp_destroy();
	cam = nullptr;
	model = nullptr;
	warp_state.rqueue = nullptr;
	warp_state.rctx = nullptr;
	warp_state.cqueue = nullptr;
	warp_state.cctx = nullptr;
	
	// kthxbye
	floor::destroy();
	return 0;
}
