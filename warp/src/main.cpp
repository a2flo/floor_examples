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

#include <floor/floor/floor.hpp>
#include <floor/core/option_handler.hpp>
#include <floor/core/gl_shader.hpp>
#include <libwarp/libwarp.h>
#include "gl_renderer.hpp"
#include "metal_renderer.hpp"
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
static const float3 cam_speeds { 75.0f /* default */, 150.0f /* faster */, 10.0f /* slower */ };

//! option -> function map
template<> vector<pair<string, warp_opt_handler::option_function>> warp_opt_handler::options {
	{ "--help", [](warp_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--scatter: warp in scatter mode" << endl;
		cout << "\t--gather: warp in gather mode" << endl;
		cout << "\t--gather-forward: warp in gather forward-only mode" << endl;
		cout << "\t--frames <count>: input frame rate, amount of frames per second that will actually be rendered (via opengl/metal)" << endl;
		cout << "\t                  NOTE: obviously an upper limit" << endl;
		cout << "\t--target <count>: target frame rate (will use a constant time delta for each compute frame instead of a variable delta)" << endl;
		cout << "\t                  NOTE: if vsync is enabled, this will automatically be set to the appropriate value" << endl;
		cout << "\t--depth-zw: write depth as z/w into a separate color fbo attachment (use this if OpenGL/OpenCL depth buffer sharing is not supported or broken)" << endl;
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
	{ "--depth-zw", [](warp_option_context&, char**&) {
		warp_state.is_zw_depth = true;
		cout << "using separate z/w depth buffer" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](warp_option_context&, char**&) {} },
};
static float3 last_cam_pos;
static void cam_dump() {
	log_undecorated("{ { %ff, %ff, %ff }, { %ff, %ff } },",
					cam->get_position().x, cam->get_position().y, cam->get_position().z,
					cam->get_rotation().x, cam->get_rotation().y);
	last_cam_pos = cam->get_position();
}

static bool compile_program() {
	// reset libwarp programs/kernels
	libwarp_cleanup();
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
				log_debug("eps 1: %f", warp_state.gather_eps_1);
				break;
			case SDLK_u:
				warp_state.gather_eps_1 -= eps1_step_size;
				warp_state.gather_eps_1 = const_math::clamp(warp_state.gather_eps_1, 0.0f, 32.0f);
				log_debug("eps 1: %f", warp_state.gather_eps_1);
				break;
			case SDLK_k:
				warp_state.gather_eps_2 += eps2_step_size;
				warp_state.gather_eps_2 = const_math::clamp(warp_state.gather_eps_2, 0.0f, 32.0f);
				log_debug("eps 2: %f", warp_state.gather_eps_2);
				break;
			case SDLK_j:
				warp_state.gather_eps_2 -= eps2_step_size;
				warp_state.gather_eps_2 = const_math::clamp(warp_state.gather_eps_2, 0.0f, 32.0f);
				log_debug("eps 2: %f", warp_state.gather_eps_2);
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
				log_debug("warping? %b", warp_state.is_warping);
				break;
			case SDLK_2:
				warp_state.is_render_full ^= true;
				log_debug("render full scene? %b", warp_state.is_render_full);
				break;
			case SDLK_3:
				warp_state.is_clear_frame ^= true;
				log_debug("frame clearing? %b", warp_state.is_clear_frame);
				break;
			case SDLK_4:
				warp_state.is_fixup ^= true;
				log_debug("px fixup? %b", warp_state.is_fixup);
				break;
			case SDLK_5:
				warp_state.cur_fbo = 0;
				warp_state.is_bidir_scatter ^= true;
				log_debug("bidirectional scatter? %b", warp_state.is_bidir_scatter);
				break;
			case SDLK_g:
				warp_state.cur_fbo = 0;
				warp_state.is_scatter ^= true;
				log_debug("scatter/gather? %s", (warp_state.is_scatter ? "scatter" : "gather"));
				break;
			case SDLK_TAB:
				warp_state.is_frame_repeat ^= true;
				log_debug("frame repeat? %b", warp_state.is_frame_repeat);
				break;
			case SDLK_p:
				if(warp_state.gather_dbg < gather_max_dbg) ++warp_state.gather_dbg;
				log_debug("dbg: %u", warp_state.gather_dbg);
				break;
			case SDLK_o:
				if(warp_state.gather_dbg > 0) --warp_state.gather_dbg;
				log_debug("dbg: %u", warp_state.gather_dbg);
				break;
			case SDLK_t:
				warp_state.is_debug_delta ^= true;
				log_debug("debug delta? %b", warp_state.is_debug_delta);
				break;
			case SDLK_b:
				warp_state.is_split_view ^= true;
				log_debug("split view? %b", warp_state.is_split_view);
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
	
	// init floor
#if !defined(FLOOR_IOS)
	floor::init(argv[0], (const char*)"../../data/", // call path, data path
				false, "config.json", // console-mode, config name
				true); // use opengl 3.3+ (core)
#else
	floor::init(argv[0], (const char*)"data/");
#endif
	floor::set_caption("warp");
	
	// opengl and floor context handling
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
		
		cam->set_position(-115.0f, -15.0f, 0.0f);
		cam->set_rotation(0.0f, -90.0f, 0.0f);
		last_cam_pos = cam->get_position();
		
		// get the compute context that has been automatically created (opencl/cuda/metal/host)
		warp_state.ctx = floor::get_compute_context();
		
		// create a compute queue (aka command queue or stream) for the fastest device in the context
		warp_state.dev = warp_state.ctx->get_device(compute_device::TYPE::FASTEST);
		warp_state.dev_queue = warp_state.ctx->create_queue(warp_state.dev);
		
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
			log_debug("using target frame rate: %u", warp_state.target_frame_count);
		}
		else log_debug("using a variable target frame rate");
		
		//
		if(!compile_program()) {
			return -1;
		}
		
		// setup renderer
		if(warp_state.ctx->get_compute_type() != COMPUTE_TYPE::METAL) {
			if(warp_state.no_opengl) {
				log_error("opengl renderer required!");
				return -1;
			}
			warp_state.no_metal = true;
			
			if(!gl_renderer::init()) {
				log_error("error during opengl initialization!");
				return -1;
			}
		}
#if defined(__APPLE__)
		else {
			if(warp_state.no_metal) {
				log_error("metal renderer required!");
				return -1;
			}
			warp_state.no_opengl = true;
			warp_state.is_zw_depth = false; // not applicable to metal
			
			if(!metal_renderer::init()) {
				log_error("error during metal initialization!");
				return -1;
			}
		}
#endif
		
		//
		bool model_success { false };
		model = obj_loader::load(floor::data_path("sponza/sponza.obj"), model_success,
								 warp_state.ctx, warp_state.dev);
		if(!model_success) {
			return -1;
		}
	
		// init done, release context
	}
	
	// main loop
	log_debug("first frame");
	while(!warp_state.done) {
		floor::get_event()->handle_events();
		
#if !defined(FLOOR_IOS)
		// stop drawing if window is inactive
		if(!(SDL_GetWindowFlags(floor::get_window()) & SDL_WINDOW_INPUT_FOCUS)) {
			SDL_Delay(20);
			continue;
		}
#endif
		
		if(!warp_state.is_frame_repeat) {
			if(!warp_state.is_auto_cam) cam->run();
			else auto_cam::run(*cam);
		}
		
		if(floor::is_new_fps_count()) {
			floor::set_caption("warp | FPS: " + to_string(floor::get_fps()) +
							   " | Pos: " + (-cam->get_position()).to_string() +
							   " | Rot: " + cam->get_rotation().to_string());
		}
		
		// s/w rendering
		if(warp_state.no_opengl && warp_state.no_metal) {
			// nope
		}
		// opengl/metal rendering
		else if((!warp_state.no_opengl || !warp_state.no_metal) && !warp_state.stop) {
			floor::start_draw();
			if(!warp_state.no_opengl) {
				gl_renderer::render((const gl_obj_model&)*model.get(), *cam.get());
			}
#if defined(__APPLE__)
			else metal_renderer::render((const metal_obj_model&)*model.get(), *cam.get());
#endif
			floor::stop_draw();
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	gl_renderer::destroy();
	cam = nullptr;
	
	// kthxbye
	floor::destroy();
	return 0;
}
