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
#include <floor/core/gl_shader.hpp>
#include "gl_renderer.hpp"
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
static const float3 cam_speeds { 1.0f /* default */, 2.0f /* faster */, 0.1f /* slower */ };

//! option -> function map
template<> unordered_map<string, warp_opt_handler::option_function> warp_opt_handler::options {
	{ "--help", [](warp_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--TODO: TODO" << endl;
		warp_state.done = true;
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
	// compile the program and get the kernel functions
#if !defined(FLOOR_IOS)
	auto new_warp_prog = warp_state.ctx->add_program_file(floor::data_path("../warp/src/warp.cpp"),
														  "-I" + floor::data_path("../warp/src") +
														  " -DSCREEN_WIDTH=" + to_string(floor::get_width()) +
														  " -DSCREEN_HEIGHT=" + to_string(floor::get_height()) +
														  " -DSCREEN_FOV=" + to_string(warp_state.fov));
#else
	// TODO
#endif
	if(new_warp_prog == nullptr) {
		log_error("program compilation failed");
		return false;
	}
	auto new_warp_kernel = new_warp_prog->get_kernel("warp_scatter_simple");
	auto new_clear_kernel = new_warp_prog->get_kernel("img_clear");
	auto new_fixup_kernel = new_warp_prog->get_kernel("single_px_fixup");
	if(new_warp_kernel == nullptr || new_clear_kernel == nullptr || new_fixup_kernel == nullptr) {
		log_error("failed to retrieve kernel from program");
		return false;
	}
	
	// all okay
	warp_state.warp_prog = new_warp_prog;
	warp_state.warp_kernel = new_warp_kernel;
	warp_state.clear_kernel = new_clear_kernel;
	warp_state.fixup_kernel = new_fixup_kernel;
	return true;
}

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		warp_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_DOWN) {
		switch(((shared_ptr<key_down_event>&)obj)->key) {
			case SDLK_LSHIFT:
				cam->set_cam_speed(cam_speeds.y);
				break;
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speeds.z);
				break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_q:
			case SDLK_ESCAPE:
				warp_state.done = true;
				break;
			case SDLK_LSHIFT:
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
			case SDLK_f:
				warp_state.is_single_frame ^= true;
				warp_state.is_auto_cam = !warp_state.is_single_frame && warp_state.is_auto_cam;
				cam->set_single_frame(warp_state.is_single_frame);
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
				true); // use opengl 3.2+ core
#else
	floor::init(argv[0], (const char*)"data/");
#endif
	floor::set_caption("warp");
	
	// opengl and floor context handling
	floor::acquire_context();
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
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
	
	if(!compile_program()) {
		return -1;
	}
	
	// setup renderer
	if(!gl_renderer::init()) {
		log_error("error during opengl initialization!");
		return -1;
	}
	
	//
	bool model_success { false };
	auto model = obj_loader::load(floor::data_path("sponza/sponza.obj"), model_success);
	if(!model_success) {
		return -1;
	}
	
	// init done, release context
	floor::release_context();
	
	// main loop
	log_debug("first frame");
	while(!warp_state.done) {
		floor::get_event()->handle_events();
		
		if(!warp_state.is_auto_cam) cam->run();
		else auto_cam::run(*cam);
		
		if(floor::is_new_fps_count()) {
			floor::set_caption("warp | FPS: " + to_string(floor::get_fps()) +
							   " | Pos: " + (-cam->get_position()).to_string() +
							   " | Rot: " + cam->get_rotation().to_string());
		}
		
		// s/w rendering
		if(warp_state.no_opengl) {
			// nope
		}
		// opengl rendering
		else if(!warp_state.no_opengl && !warp_state.stop) {
			floor::start_draw();
			gl_renderer::render(model, *cam.get());
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
