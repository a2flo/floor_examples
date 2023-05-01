/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
#include <floor/core/gl_shader.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <libwarp/libwarp.h>
#include "gl_renderer.hpp"
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
		cout << "\t--frames <count>: input frame rate, amount of frames per second that will actually be rendered (via opengl/metal/vulkan)" << endl;
		cout << "\t                  NOTE: obviously an upper limit" << endl;
		cout << "\t--target <count>: target frame rate (will use a constant time delta for each compute frame instead of a variable delta)" << endl;
		cout << "\t                  NOTE: if vsync is enabled, this will automatically be set to the appropriate value" << endl;
		cout << "\t--depth-zw: write depth as z/w into a separate color fbo attachment (use this if OpenGL/OpenCL depth buffer sharing is not supported or broken)" << endl;
		cout << "\t--always-render: always perform full scene rendering (warping is disabled), not that --frames/--target will be ignored if enabled" << endl;
		cout << "\t--no-unified: do not use the unified renderer (uses the OpenGL renderer instead)" << endl;
		cout << "\t--no-tessellation: disables tessellation in the unified renderer" << endl;
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
	{ "--depth-zw", [](warp_option_context&, char**&) {
		warp_state.is_zw_depth = true;
		cout << "using separate z/w depth buffer" << endl;
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
	{ "--no-unified", [](warp_option_context&, char**&) {
		warp_state.unified_renderer = false;
		cout << "unified renderer disabled" << endl;
	}},
	{ "--no-tessellation", [](warp_option_context&, char**&) {
		warp_state.use_tessellation = false;
		cout << "tessellation disabled" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](warp_option_context&, char**&) {} },
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
	
	if (warp_state.unified_renderer && uni_renderer) {
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
			case SDLK_i:
				warp_state.gather_eps_1 += eps1_step_size;
				warp_state.gather_eps_1 = const_math::clamp(warp_state.gather_eps_1, 0.0f, 32.0f);
				log_debug("eps 1: $", warp_state.gather_eps_1);
				break;
			case SDLK_u:
				warp_state.gather_eps_1 -= eps1_step_size;
				warp_state.gather_eps_1 = const_math::clamp(warp_state.gather_eps_1, 0.0f, 32.0f);
				log_debug("eps 1: $", warp_state.gather_eps_1);
				break;
			case SDLK_k:
				warp_state.gather_eps_2 += eps2_step_size;
				warp_state.gather_eps_2 = const_math::clamp(warp_state.gather_eps_2, 0.0f, 32.0f);
				log_debug("eps 2: $", warp_state.gather_eps_2);
				break;
			case SDLK_j:
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
			case SDLK_q:
			case SDLK_ESCAPE:
				warp_state.done = true;
				break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speeds.x);
				break;
			case SDLK_x:
				cam_dump();
				break;
			case SDLK_c:
				compile_program();
				break;
			case SDLK_SPACE:
				warp_state.stop ^= true;
				break;
			case SDLK_v:
				warp_state.is_auto_cam ^= true;
				break;
			case SDLK_r:
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
				warp_state.legacy_cur_fbo = 0;
				warp_state.is_bidir_scatter ^= true;
				log_debug("bidirectional scatter? $", warp_state.is_bidir_scatter);
				break;
			case SDLK_g:
				warp_state.legacy_cur_fbo = 0;
				warp_state.is_scatter ^= true;
				log_debug("scatter/gather? $", (warp_state.is_scatter ? "scatter" : "gather"));
				break;
			case SDLK_TAB:
				warp_state.is_frame_repeat ^= true;
				log_debug("frame repeat? $", warp_state.is_frame_repeat);
				break;
			case SDLK_p:
				if(warp_state.gather_dbg < gather_max_dbg) ++warp_state.gather_dbg;
				log_debug("dbg: $", warp_state.gather_dbg);
				break;
			case SDLK_o:
				if(warp_state.gather_dbg > 0) --warp_state.gather_dbg;
				log_debug("dbg: $", warp_state.gather_dbg);
				break;
			case SDLK_t:
				warp_state.is_debug_delta ^= true;
				log_debug("debug delta? $", warp_state.is_debug_delta);
				break;
			case SDLK_b:
				warp_state.is_split_view ^= true;
				log_debug("split view? $", warp_state.is_split_view);
				break;
			case SDLK_0: {
				static const uint32_t max_disp_mode = (warp_state.use_tessellation ? 3u : 2u);
				warp_state.displacement_mode = (warp_state.displacement_mode + 1u) % max_disp_mode;
				break;
			}
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
	const auto wanted_renderer = (// if neither opengl or vulkan is disabled, use the default
								  // (this will choose vulkan if the config compute backend is vulkan)
								  (!warp_state.no_opengl && !warp_state.no_vulkan && warp_state.unified_renderer) ? floor::RENDERER::DEFAULT :
								  // else: choose a specific one
								  !warp_state.no_vulkan && warp_state.unified_renderer ? floor::RENDERER::VULKAN :
								  !warp_state.no_metal && warp_state.unified_renderer ? floor::RENDERER::METAL :
								  !warp_state.no_opengl ? floor::RENDERER::OPENGL :
								  // opengl/vulkan/metal are disabled
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
		// disable resource tracking for Metal
		.context_flags = ((warp_state.unified_renderer && wanted_renderer != floor::RENDERER::OPENGL ?
						   COMPUTE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING : COMPUTE_CONTEXT_FLAGS::NONE) |
						  COMPUTE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING)
	})) {
		return -1;
	}
	
	// disable resp. other renderers when using opengl/metal/vulkan
	const bool is_metal = (floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::METAL);
	const bool is_vulkan = (floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::VULKAN);
	const auto floor_renderer = floor::get_renderer();
	
	if(floor_renderer == floor::RENDERER::NONE) {
		log_error("no renderer was initialized");
		return -2;
	}
	
	if (is_metal) {
		warp_state.no_opengl = true;
		warp_state.no_vulkan = true;
	} else {
		warp_state.no_metal = true;
	}
	
	if (is_vulkan) {
		if (floor_renderer == floor::RENDERER::VULKAN) {
			warp_state.no_opengl = true;
			warp_state.no_metal = true;
		}
		// can't use OpenGL with Vulkan
		else {
			warp_state.no_opengl = true;
		}
	} else {
		warp_state.no_vulkan = true;
	}
	
	if (!is_metal && !is_vulkan) {
		// neither Metal nor Vulkan is available (or disabled)
		warp_state.unified_renderer = false;
	}
	
	log_debug("using $", (warp_state.unified_renderer ? "unified renderer" :
						  !warp_state.no_opengl ? "opengl renderer" : "no renderer at all"));
	
	// floor context handling
	struct floor_ctx_guard {
		floor_ctx_guard() {
			floor::acquire_context();
		}
		~floor_ctx_guard() {
			floor::release_context();
		}
	};
	event::handler evt_handler_fnctr(&evt_handler);
	shared_ptr<obj_model> model;
	{
		floor_ctx_guard grd;
		
		// add event handlers
		floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
													   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN,
													   EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE,
													   EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
													   EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE,
													   EVENT_TYPE::WINDOW_RESIZE);
		
		cam = make_unique<camera>();
		
		if(!warp_state.is_auto_cam) {
			cam->set_mouse_input(true);
			cam->set_wasd_input(true);
			cam->set_rotation_wrapping(false);
		}
		else {
			cam->set_mouse_input(false);
			cam->set_wasd_input(false);
		}
		
		cam->set_position(-115.0, 15.0, 0.0);
		cam->set_rotation(0.0, -90.0);
		last_cam_pos = cam->get_position();
		
		// get the compute context that has been automatically created (opencl/cuda/metal/vulkan/host)
		warp_state.ctx = floor::get_compute_context();
		log_warn("ctx flags: $", warp_state.ctx->get_context_flags());
		
		// create a compute queue (aka command queue or stream) for the fastest device in the context
		warp_state.dev = warp_state.ctx->get_device(compute_device::TYPE::FASTEST);
		warp_state.main_dev_queue = warp_state.ctx->create_queue(*warp_state.dev);
		
		// enable argument_buffer use when argument buffers with images are supported by the device
		if (warp_state.unified_renderer) {
			warp_state.use_argument_buffer = warp_state.dev->argument_buffer_image_support;
#if 1
			warp_state.use_tessellation &= warp_state.dev->tessellation_support;
#endif
			
			// if there is full argument buffer support and there is full indirect command support, enable indirect command use
			if (warp_state.use_argument_buffer &&
				warp_state.dev->indirect_command_support &&
				warp_state.dev->indirect_render_command_support &&
				warp_state.dev->indirect_compute_command_support) {
#if 1
				warp_state.use_indirect_commands = true;
#endif
			}
		}
		
		// tessellation requires argument buffer support
		if (warp_state.use_tessellation && !warp_state.use_argument_buffer) {
			log_error("argument buffer support is required when using tessellation");
			return -1;
		}
		
		// if vsync is enabled (or metal is being used, which is always using vsync), and the target frame rate isn't set,
		// compute the appropriate value according to the render/input frame rate and display refresh rate
		if((floor::get_vsync() || warp_state.ctx->get_compute_type() == COMPUTE_TYPE::METAL) &&
		   warp_state.target_frame_count == 0) {
			warp_state.target_frame_count = floor::get_window_refresh_rate();
			if(warp_state.target_frame_count == 0) {
				warp_state.target_frame_count = 60;
			}
		}
		if(warp_state.target_frame_count > 0) {
			log_debug("using target frame rate: $", warp_state.target_frame_count);
		}
		else log_debug("using a variable target frame rate");
		
		//
		if(!compile_program()) {
			return -1;
		}
		
		// init renderers (need compiled prog first)
		if (warp_state.unified_renderer) {
			warp_state.is_zw_depth = false; // not applicable to unified renderer
			uni_renderer = make_shared<unified_renderer>();
			if (!uni_renderer->init()) {
				log_error("error during unified renderer initialization!");
				return -1;
			}
			
			// init displacement mode depending on tessellation support
			warp_state.displacement_mode = (warp_state.use_tessellation ? 2u : 1u);
		} else {
			if(!warp_state.no_opengl) {
				if(!gl_renderer::init()) {
					log_error("error during opengl initialization!");
					return -1;
				}
			}
		}
		
		//
		bool model_success { false };
		model = obj_loader::load(floor::data_path("sponza/sponza.obj"), model_success,
								 *warp_state.ctx, *warp_state.main_dev_queue);
		if(!model_success) {
			return -1;
		}
		
		uni_renderer->post_init(*warp_state.main_dev_queue, *(floor_obj_model*)model.get(), *cam);
	
		// init done, release context
	}
	
	// main loop
	log_debug("first frame");
	while (!warp_state.done) {
		if (uni_renderer) {
			// throttle main loop when rendering and render queueing is off-loaded
			this_thread::sleep_for(500us);
			{
				static auto last_sec_frame = chrono::steady_clock::now();
				//static uint32_t event_loop_fps_counter = 0u;
				const auto cur_time = chrono::steady_clock::now();
				if (cur_time - last_sec_frame > 1s) {
					// update stats
					//log_msg("event loop FPS: $", event_loop_fps_counter);
					last_sec_frame = cur_time;
					//event_loop_fps_counter = 0;
				}
				//++event_loop_fps_counter;
			}
		}
		
		floor::get_event()->handle_events();
		
#if !defined(FLOOR_IOS) && 0
		// stop drawing if window is inactive
		if(!(SDL_GetWindowFlags(floor::get_window()) & SDL_WINDOW_INPUT_FOCUS)) {
			SDL_Delay(20);
			continue;
		}
#endif
		
		if (!warp_state.is_frame_repeat) {
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
		
		// s/w rendering
		if(warp_state.no_opengl && warp_state.no_metal && warp_state.no_vulkan) {
			// nope
		}
		// opengl/metal/vulkan rendering
		else if((!warp_state.no_opengl || !warp_state.no_metal || !warp_state.no_vulkan) && !warp_state.stop) { // TODO: !!! cleanup !!!
			if (uni_renderer) {
				uni_renderer->render();
			} else if (!warp_state.no_opengl) {
				floor::start_frame();
				gl_renderer::render((const gl_obj_model&)*model.get(), *cam.get());
				floor::end_frame();
			}
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	gl_renderer::destroy();
	uni_renderer = nullptr;
	cam = nullptr;
	model = nullptr;
	warp_state.main_dev_queue = nullptr;
	warp_state.ctx = nullptr;
	
	// kthxbye
	floor::destroy();
	return 0;
}
