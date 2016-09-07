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
#include <floor/core/timer.hpp>
#include <floor/core/option_handler.hpp>
#include "gl_renderer.hpp"
#include "metal_renderer.hpp"
#include "hlbvh_state.hpp"
#include "animation.hpp"
#include "collider.hpp"
#include "camera.hpp"

hlbvh_state_struct hlbvh_state;

struct hlbvh_option_context {
	// unused
	string additional_options { "" };
};
typedef option_handler<hlbvh_option_context> hlbvh_opt_handler;

// camera (free cam mode)
static unique_ptr<camera> cam;
// camera speeds (modified by shift/ctrl)
static const float3 cam_speeds { 25.0f /* default */, 150.0f /* faster */, 2.5f /* slower */ };

//! option -> function map
template<> vector<pair<string, hlbvh_opt_handler::option_function>> hlbvh_opt_handler::options {
	{ "--help", [](hlbvh_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--no-opengl: disables opengl rendering" << endl;
#if defined(__APPLE__)
		cout << "\t--no-metal: disables metal rendering" << endl;
#endif
		cout << "\t--benchmark: runs the simulation in benchmark mode, without rendering" << endl;
		cout << "\t--no-triangle-vis: disables triangle collision visualization and uses per-model visualization instead (faster)" << endl;
		hlbvh_state.done = true;
		
		cout << endl;
		cout << "controls:" << endl;
		cout << "\tq: quit" << endl;
		cout << "\tspace: halt" << endl;
		cout << "\tc: switch camera mode between free cam and locked/rotational camera" << endl;
		cout << "\t* locked camera:" << endl;
		cout << "\t\tw/s/right-mouse-drag: move camera forwards/backwards" << endl;
		cout << "\t\tleft-mouse-drag: rotate camera" << endl;
		cout << "\t* free camera:" << endl;
		cout << "\t\tw/a/s/d/arrow-keys: move forward/left/backward/right" << endl;
		cout << "\t\tmove mouse: rotate camera" << endl;
		cout << "\tshift: faster camera movement" << endl;
		cout << "\tctrl: slower camera movement" << endl;
		cout << endl;
		
		// TODO: performance stats
		cout << endl;
	}},
	{ "--no-opengl", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_opengl = true;
		cout << "opengl disabled" << endl;
	}},
	{ "--no-metal", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_metal = true;
		cout << "metal disabled" << endl;
	}},
	{ "--no-triangle-vis", [](hlbvh_option_context&, char**&) {
		hlbvh_state.triangle_vis = false;
		cout << "triangle collision visualization disabled" << endl;
	}},
	{ "--benchmark", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_opengl = true; // also disable opengl
		hlbvh_state.no_metal = true; // also disable metal
		hlbvh_state.triangle_vis = false; // triangle visualization is unnecessary here
		hlbvh_state.benchmark = true;
		cout << "benchmark mode enabled" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](hlbvh_option_context&, char**&) {} },
};

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		hlbvh_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_q:
				hlbvh_state.done = true;
				break;
			case SDLK_SPACE:
				hlbvh_state.stop ^= true;
				break;
			case SDLK_c:
				hlbvh_state.cam_mode ^= true;
				if(cam != nullptr) {
					cam->set_keyboard_input(hlbvh_state.cam_mode);
					cam->set_wasd_input(hlbvh_state.cam_mode);
					cam->set_mouse_input(hlbvh_state.cam_mode);
				}
				log_msg("switched cam mode");
				break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speeds.x);
				break;
			default: break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::KEY_DOWN) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				cam->set_cam_speed(cam_speeds.y);
				break;
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				cam->set_cam_speed(cam_speeds.z);
				break;
			case SDLK_w:
				hlbvh_state.distance = const_math::clamp(hlbvh_state.distance - 5.0f, 1.0f, hlbvh_state.max_distance);
				break;
			case SDLK_s:
				hlbvh_state.distance = const_math::clamp(hlbvh_state.distance + 5.0f, 1.0f, hlbvh_state.max_distance);
				break;
			default: break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::MOUSE_LEFT_DOWN
#if defined(FLOOR_IOS) // NOTE: trackpads also generate finger events -> would lead to double input
			|| type == EVENT_TYPE::FINGER_DOWN
#endif
			) {
		hlbvh_state.enable_cam_rotate = true;
		return true;
	}
	else if(type == EVENT_TYPE::MOUSE_LEFT_UP
#if defined(FLOOR_IOS)
			|| type == EVENT_TYPE::FINGER_UP
#endif
			) {
		hlbvh_state.enable_cam_rotate = false;
		return true;
	}
	
	// origin cam:
	if(!hlbvh_state.cam_mode) {
		if(type == EVENT_TYPE::MOUSE_RIGHT_DOWN) {
			hlbvh_state.enable_cam_move = true;
			return true;
		}
		else if(type == EVENT_TYPE::MOUSE_RIGHT_UP) {
			hlbvh_state.enable_cam_move = false;
			return true;
		}
		else if(type == EVENT_TYPE::MOUSE_MOVE
#if defined(FLOOR_IOS)
				|| type == EVENT_TYPE::FINGER_MOVE
#endif
				) {
			if(!(hlbvh_state.enable_cam_rotate ^ hlbvh_state.enable_cam_move)) {
				return true;
			}
			
#if !defined(FLOOR_IOS)
			const auto& move_evt = (shared_ptr<mouse_move_event>&)obj;
			
			// "normalize"/transform delta position according to screen size
			float2 delta { float2(move_evt->move) / float2 { floor::get_width(), floor::get_height() } };
#else
			const auto& move_evt = (shared_ptr<finger_move_event>&)obj;
			float2 delta { move_evt->normalized_move }; // already normalized
#endif
			
			if(hlbvh_state.enable_cam_rotate) {
				// multiply by desired rotation speed
				static constexpr const float rot_speed { 100.0f };
				delta *= rot_speed;
				
				// multiply existing rotation by newly computed rotation around the y axis
				hlbvh_state.cam_rotation *= (quaternionf::rotation_deg(delta.x, float3 { 0.0f, 1.0f, 0.0f }));
			}
			else {
				// multiply by desired move speed
				static constexpr const float move_speed { 250.0f };
				delta *= move_speed;
				hlbvh_state.distance = const_math::clamp(hlbvh_state.distance + delta.y, 1.0f, hlbvh_state.max_distance);
			}
			return true;
		}
	}
	// free cam:
	else {
		if(type == EVENT_TYPE::MOUSE_RIGHT_UP) {
			if(cam != nullptr) {
				cam->set_mouse_input(cam->get_mouse_input() ^ true);
			}
		}
	}
	return false;
}

int main(int, char* argv[]) {
	// handle options
	hlbvh_option_context option_ctx;
	hlbvh_opt_handler::parse_options(argv + 1, option_ctx);
	if(hlbvh_state.done) return 0;
	
	// init floor
#if !defined(FLOOR_IOS)
	floor::init(argv[0], (const char*)"../../data/", // call path, data path
				hlbvh_state.benchmark, "config.json", // console-mode, config name
				hlbvh_state.benchmark ^ true); // use opengl 3.3+ (core)
#else
	floor::init(argv[0], (const char*)"data/");
#endif
	
	// disable opengl renderer when using metal
	if(!hlbvh_state.no_opengl) {
		hlbvh_state.no_opengl = (floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::METAL);
	}
#if defined(FLOOR_NO_METAL)
	// disable metal renderer if it's not available
	hlbvh_state.no_metal = true;
#else
	if(!hlbvh_state.no_metal) {
		hlbvh_state.no_metal = (floor::get_compute_context()->get_compute_type() != COMPUTE_TYPE::METAL);
	}
#endif
	
	floor::set_caption("hlbvh");
	
	// opengl and floor context handling
	if(hlbvh_state.no_opengl) floor::set_use_gl_context(false);
	floor::acquire_context();
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
												   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN,
												   EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE,
												   EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
												   EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE);
	
	cam = make_unique<camera>();
	cam->set_mouse_input(hlbvh_state.cam_mode);
	cam->set_wasd_input(hlbvh_state.cam_mode);
	cam->set_keyboard_input(hlbvh_state.cam_mode);
	cam->set_rotation_wrapping(true);
	cam->set_position(-10.5f, -6.0f, 2.15f);
	cam->set_rotation(28.0f, 256.0f, 0.0f);
	
	// get the compute context that has been automatically created (opencl/cuda/metal/host, depending on the config)
	hlbvh_state.ctx = floor::get_compute_context();
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	hlbvh_state.dev = hlbvh_state.ctx->get_device(compute_device::TYPE::FASTEST);
	hlbvh_state.dev_queue = hlbvh_state.ctx->create_queue(hlbvh_state.dev);
	
	// compile the program and get the kernel function
#if !defined(FLOOR_IOS)
	const llvm_compute::compile_options options {
		.enable_warnings = true,
	};
	auto prog = hlbvh_state.ctx->add_program_file(floor::data_path("../hlbvh/src/hlbvh.cpp"), options);
#else
	// TODO: ios implementation!
#endif
	if(prog == nullptr) {
		log_error("program compilation failed");
		return -1;
	}
	
	// get all kernels
	hlbvh_state.kernels = {
		{ "build_aabbs", {} },
		{ "collide_root_aabbs", {} },
		{ "compute_morton_codes", {} },
		{ "build_bvh", {} },
		{ "build_bvh_aabbs_leaves", {} },
		{ "build_bvh_aabbs", {} },
		{ "collide_bvhs_no_tri_vis", {} },
		{ "collide_bvhs_tri_vis", {} },
		{ "map_collided_triangles", {} },
		{ "radix_sort_count", {} },
		{ "radix_sort_prefix_sum", {} },
		{ "radix_sort_stream_split", {} },
	};
	for(auto& kernel : hlbvh_state.kernels) {
		kernel.second = prog->get_kernel(kernel.first);
		if(kernel.second == nullptr) {
			log_error("failed to retrieve kernel \"%s\" from program", kernel.first);
			return -1;
		}
		hlbvh_state.kernel_max_local_size[kernel.first] = (uint32_t)kernel.second->get_kernel_entry(hlbvh_state.dev)->max_total_local_size;
		log_debug("max local size for \"%s\": %u", kernel.first, hlbvh_state.kernel_max_local_size[kernel.first]);
	}
	
	// init gl renderer
#if !defined(FLOOR_IOS)
	if(!hlbvh_state.no_opengl) {
		// setup renderer
		if(!gl_renderer::init()) {
			log_error("error during opengl initialization!");
			return -1;
		}
	}
#endif
	// init metal renderer (need compiled prog first)
#if defined(__APPLE__)
	shared_ptr<compute_program> shader_prog;
	if(!hlbvh_state.no_metal && hlbvh_state.no_opengl) {
		shader_prog = hlbvh_state.ctx->add_program_file(floor::data_path("../hlbvh/src/hlbvh_shaders.cpp"),
														"-DCOLLIDING_TRIANGLES_VIS="s + (hlbvh_state.triangle_vis ? "1" : "0"));
		if(shader_prog == nullptr) {
			log_error("shader program compilation failed");
			return -1;
		}
		
		// setup renderer
		if(!metal_renderer::init(shader_prog->get_kernel("hlbvh_vertex"),
								 shader_prog->get_kernel("hlbvh_fragment"))) {
			log_error("error during metal initialization!");
			return -1;
		}
	}
#endif
	
	// load animated models
	vector<unique_ptr<animation>> models;
	models.emplace_back(make_unique<animation>("collision_models/gear/gear_0000", ".obj", 20, false, 0.1f));
	models.emplace_back(make_unique<animation>("collision_models/gear2/gear2_0000", ".obj", 20, false, 0.1f));
	models.emplace_back(make_unique<animation>("collision_models/sinbad/sinbad_0000", ".obj", 20, true));
	models.emplace_back(make_unique<animation>("collision_models/golem/golem_0000", ".obj", 20, false, 0.125f));
	models.emplace_back(make_unique<animation>("collision_models/plane/plane_00000", ".obj", 2));
	
	// create collider
	collider hlbvh_collider;
	
	// init done, release context
	floor::release_context();
	
	// main loop
	while(!hlbvh_state.done) {
		floor::get_event()->handle_events();
		
#if !defined(FLOOR_IOS)
		// stop drawing if window is inactive
		if(!(SDL_GetWindowFlags(floor::get_window()) & SDL_WINDOW_INPUT_FOCUS) &&
		   !hlbvh_state.benchmark) {
			SDL_Delay(20);
			continue;
		}
#endif
		
		// run cam
		if(hlbvh_state.cam_mode) {
			cam->run();
		}
		
		// frame step
		if(!hlbvh_state.stop) {
			for(auto& mdl : models) {
				mdl->do_step();
			}
		}
		
		// run the collision
		const auto& collisions = hlbvh_collider.collide(models);
		
		//
		if(floor::is_new_fps_count()) {
			floor::set_caption("hlbvh | FPS: " + to_string(floor::get_fps()) +
							   " | Pos: " + (-cam->get_position()).to_string() +
							   " | Rot: " + cam->get_rotation().to_string());
		}
		
		// s/w rendering
		if(hlbvh_state.no_opengl && hlbvh_state.no_metal) {
			// nope
		}
		// opengl/metal rendering
		else if((!hlbvh_state.no_opengl || !hlbvh_state.no_metal)) {
			floor::start_draw();
			if(!hlbvh_state.no_opengl) {
				gl_renderer::render(models, collisions, hlbvh_state.cam_mode, *cam.get());
			}
#if defined(__APPLE__)
			else metal_renderer::render(models, collisions, hlbvh_state.cam_mode, *cam.get());
#endif
			floor::stop_draw();
		}
	}
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	cam = nullptr;
	
	if(!hlbvh_state.no_opengl) {
		// need to kill off the shared opengl buffers before floor kills the opengl context, otherwise bad things(tm) will happen
		floor::acquire_context();
		models.clear();
		floor::release_context();
	}
#if defined(__APPLE__)
	if(!hlbvh_state.no_metal && hlbvh_state.no_opengl) {
		metal_renderer::destroy();
	}
#endif
	
	// kthxbye
	log_msg("done!");
	floor::destroy();
	return 0;
}
