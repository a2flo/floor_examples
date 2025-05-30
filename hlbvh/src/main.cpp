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
#include <floor/core/timer.hpp>
#include <floor/core/option_handler.hpp>
#include <floor/compute/compute_kernel.hpp>
#include "unified_renderer.hpp"
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
static const double3 cam_speeds { 25.0 /* default */, 150.0 /* faster */, 2.5 /* slower */ };

//! option -> function map
template<> vector<pair<string, hlbvh_opt_handler::option_function>> hlbvh_opt_handler::options {
	{ "--help", [](hlbvh_option_context&, char**&) {
		cout << "command line options:" << endl;
#if defined(__APPLE__)
		cout << "\t--no-metal: disables metal rendering" << endl;
#endif
		cout << "\t--no-vulkan: disables vulkan rendering" << endl;
		cout << "\t--no-unified: do not use the unified renderer (uses the OpenGL renderer instead)" << endl;
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
	{ "--no-metal", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_metal = true;
		cout << "metal disabled" << endl;
	}},
	{ "--no-vulkan", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_vulkan = true;
		cout << "vulkan disabled" << endl;
	}},
	{ "--no-unified", [](hlbvh_option_context&, char**&) {
		hlbvh_state.uni_renderer = false;
		cout << "unified renderer disabled" << endl;
	}},
	{ "--no-triangle-vis", [](hlbvh_option_context&, char**&) {
		hlbvh_state.triangle_vis = false;
		cout << "triangle collision visualization disabled" << endl;
	}},
	{ "--benchmark", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_metal = true; // also disable metal
		hlbvh_state.no_vulkan = true; // also disable vulkan
		hlbvh_state.triangle_vis = false; // triangle visualization is unnecessary here
		hlbvh_state.uni_renderer = false;
		hlbvh_state.benchmark = true;
		cout << "benchmark mode enabled" << endl;
	}},
	{ "--no-fubar", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_fubar = true;
		cout << "FUBAR disabled" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](hlbvh_option_context&, char**&) {} },
	{ "-ApplePersistenceIgnoreState", [](hlbvh_option_context&, char**&) {} },
};

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		hlbvh_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_Q:
				hlbvh_state.done = true;
				break;
			case SDLK_SPACE:
				hlbvh_state.stop ^= true;
				break;
			case SDLK_C:
				hlbvh_state.cam_mode ^= true;
				if(cam != nullptr) {
					cam->set_keyboard_input(hlbvh_state.cam_mode);
					cam->set_wasd_input(hlbvh_state.cam_mode);
					cam->set_mouse_input(hlbvh_state.cam_mode);
				}
				log_msg("switched cam mode");
				break;
			case SDLK_V:
				hlbvh_state.triangle_vis ^= true;
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
			case SDLK_W:
				hlbvh_state.distance = const_math::clamp(hlbvh_state.distance - 5.0f, 1.0f, hlbvh_state.max_distance);
				break;
			case SDLK_S:
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

// embed the compiled hlbvh FUBAR files if they are available
#if defined(__has_embed)
#if __has_embed("../../data/hlbvh.fubar") && __has_embed("../../data/hlbvh_shaders.fubar")
static constexpr const uint8_t hlbvh_fubar[] {
#embed "../../data/hlbvh.fubar"
};
static constexpr const uint8_t hlbvh_shaders_fubar[] {
#embed "../../data/hlbvh_shaders.fubar"
};
#define HAS_EMBEDDED_FUBAR 1
#endif
#endif

int main(int, char* argv[]) {
	// handle options
	hlbvh_option_context option_ctx;
	hlbvh_opt_handler::parse_options(argv + 1, option_ctx);
	if(hlbvh_state.done) return 0;
	
	// disable renderers that aren't available
#if defined(FLOOR_NO_METAL)
	hlbvh_state.no_metal = true;
#endif
#if defined(FLOOR_NO_VULKAN)
	hlbvh_state.no_vulkan = true;
#endif
	
	// init floor
	const auto wanted_renderer = (// if neither Metal nor Vulkan is disabled, use the default
								  (!hlbvh_state.no_metal && !hlbvh_state.no_vulkan) ? floor::RENDERER::DEFAULT :
								  // else: choose a specific one
								  !hlbvh_state.no_vulkan ? floor::RENDERER::VULKAN :
								  !hlbvh_state.no_metal ? floor::RENDERER::METAL :
								  // vulkan/metal are disabled
								  floor::RENDERER::NONE);
	if (!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "hlbvh",
		.console_only = hlbvh_state.benchmark,
		.renderer = wanted_renderer,
		// disable resource tracking and enable non-blocking Vulkan execution
		.context_flags = COMPUTE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING | COMPUTE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING,
	})) {
		return -1;
	}
	
	
	// disable resp. other renderers when using opengl/metal/vulkan
	const auto floor_renderer = floor::get_renderer();
	const bool is_metal = ((floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::METAL ||
							floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::HOST) &&
						   floor_renderer == floor::RENDERER::METAL);
	const bool is_vulkan = ((floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::VULKAN ||
							 floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::CUDA ||
							 floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::HOST) &&
							floor_renderer == floor::RENDERER::VULKAN);
	
	if (floor_renderer == floor::RENDERER::NONE && !hlbvh_state.benchmark) {
		log_error("no renderer was initialized");
		return -2;
	}
	
	if (is_metal) {
		hlbvh_state.no_vulkan = true;
	} else {
		hlbvh_state.no_metal = true;
	}
	
	if (is_vulkan) {
		if (floor_renderer == floor::RENDERER::VULKAN) {
			hlbvh_state.no_metal = true;
		}
	} else {
		hlbvh_state.no_vulkan = true;
	}
	
	log_debug("using $", (hlbvh_state.uni_renderer ? "unified renderer" : "no renderer at all"));
	
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
	cam->set_position(-10.5, 6.0, 2.15);
	cam->set_rotation(28.0, 256.0);
	
	// get the compute and render contexts that has been automatically created (opencl/cuda/metal/vulkan/host, depending on the config)
	hlbvh_state.cctx = floor::get_compute_context();
	hlbvh_state.rctx = floor::get_render_context();
	if (!hlbvh_state.rctx) {
		hlbvh_state.rctx = hlbvh_state.cctx;
	}
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	hlbvh_state.cdev = hlbvh_state.cctx->get_device(compute_device::TYPE::FASTEST);
	hlbvh_state.cqueue = hlbvh_state.cctx->create_queue(*hlbvh_state.cdev);
	if (hlbvh_state.cctx != hlbvh_state.rctx) {
		// render context is not the same as the compute context -> also create dev and queue for the render context
		hlbvh_state.rdev = hlbvh_state.rctx->get_device(compute_device::TYPE::FASTEST);
		hlbvh_state.rqueue = hlbvh_state.rctx->create_queue(*hlbvh_state.rdev);
	} else {
		// otherwise: use the same as the compute context
		hlbvh_state.rdev = hlbvh_state.cdev;
		hlbvh_state.rqueue = hlbvh_state.cqueue;
	}
	
	//
	shared_ptr<compute_program> prog;
	shared_ptr<compute_program> shader_prog;
	
	// if embedded FUBAR data exists + it isn't disabled, try to load this first
#if defined(HAS_EMBEDDED_FUBAR)
	if (!hlbvh_state.no_fubar) {
		// hlbvh kernels/shaders
		const span<const uint8_t> fubar_data { hlbvh_fubar, std::size(hlbvh_fubar) };
		prog = hlbvh_state.cctx->add_universal_binary(fubar_data);
		if (prog) {
			log_msg("using embedded hlbvh FUBAR");
		}
		if (hlbvh_state.uni_renderer) {
			const span<const uint8_t> fubar_shader_data { hlbvh_shaders_fubar, std::size(hlbvh_shaders_fubar) };
			shader_prog = hlbvh_state.rctx->add_universal_binary(fubar_shader_data);
			if (shader_prog) {
				log_msg("using embedded hlbvh shaders FUBAR");
			}
		}
	}
#endif
	
	// compile the program and get the kernel function
#if !defined(FLOOR_IOS)
	if (!prog) {
		const llvm_toolchain::compile_options options {
			.enable_warnings = true,
		};
		prog = hlbvh_state.cctx->add_program_file(floor::data_path("../hlbvh/src/hlbvh.cpp"), options);
	}
#endif
	if (!prog) {
		log_error("program compilation/loading failed");
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
		{ "indirect_radix_sort_count", {} },
		{ "indirect_radix_sort_stream_split", {} },
	};
	for (auto& kernel : hlbvh_state.kernels) {
		kernel.second = prog->get_kernel(kernel.first);
		if (kernel.second == nullptr) {
			log_error("failed to retrieve kernel \"$\" from program", kernel.first);
			return -1;
		}
		hlbvh_state.kernel_max_local_size[kernel.first] = (uint32_t)kernel.second->get_kernel_entry(*hlbvh_state.cdev)->max_total_local_size;
		log_debug("max local size for \"$\": $", kernel.first, hlbvh_state.kernel_max_local_size[kernel.first]);
	}
	
	// init unified renderer (need compiled prog first)
	if (hlbvh_state.uni_renderer) {
		if (!shader_prog) {
			shader_prog = hlbvh_state.rctx->add_program_file(floor::data_path("../hlbvh/src/hlbvh_shaders.cpp"));
			if (!shader_prog) {
				log_error("shader program compilation failed");
				return -1;
			}
		}
		
		// setup renderer
		if (!unified_renderer::init(shader_prog->get_kernel("hlbvh_vertex"),
									shader_prog->get_kernel("hlbvh_fragment"))) {
			log_error("error during unified renderer initialization!");
			return -1;
		}
	}
	
	// load animated models
	vector<unique_ptr<animation>> models;
	models.emplace_back(make_unique<animation>("collision_models/gear/gear_0000", ".obj", 20, false, 0.1f));
	models.emplace_back(make_unique<animation>("collision_models/gear2/gear2_0000", ".obj", 20, false, 0.1f));
	models.emplace_back(make_unique<animation>("collision_models/sinbad/sinbad_0000", ".obj", 20, true));
	models.emplace_back(make_unique<animation>("collision_models/golem/golem_0000", ".obj", 20, false, 0.125f));
	models.emplace_back(make_unique<animation>("collision_models/plane/plane_00000", ".obj", 2));
	
	// create collider
	auto hlbvh_collider = make_unique<collider>();
	
	// main loop
	auto frame_time = core::unix_timestamp_us();
	while (!hlbvh_state.done) {
		floor::get_event()->handle_events();
		
#if !defined(FLOOR_IOS)
		// stop drawing if window is inactive
		if (!(SDL_GetWindowFlags(floor::get_window()) & SDL_WINDOW_INPUT_FOCUS) &&
			!hlbvh_state.benchmark) {
			this_thread::sleep_for(20ms);
			continue;
		}
#endif
		
		// run cam
		if (hlbvh_state.cam_mode) {
			cam->run();
		}
		
		// frame step
		if (!hlbvh_state.stop) {
			for (auto& mdl : models) {
				mdl->do_step();
			}
		}
		
		const auto this_frame_time = core::unix_timestamp_us();
		const auto frame_delta = double(this_frame_time - frame_time) / 1000.0;
#if defined(FLOOR_DEBUG)
		log_warn("###### frame ($ms) #######", frame_delta);
#endif
		frame_time = this_frame_time;
		
		// run the collision
		hlbvh_collider->collide(models);
		
		//
		floor::set_caption("hlbvh | frame-time: " + to_string(frame_delta) + "ms");
		
		if (hlbvh_state.uni_renderer) {
			// Metal/Vulkan rendering
			unified_renderer::render(models, hlbvh_state.cam_mode, *cam.get());
		}
	}
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	cam = nullptr;
	
	// cleanup
	models.clear();
	
	if (hlbvh_state.uni_renderer) {
		unified_renderer::destroy();
	}
	
	hlbvh_collider = nullptr;
	prog = nullptr;
	shader_prog = nullptr;
	hlbvh_state.kernels.clear();
	hlbvh_state.cqueue = nullptr;
	hlbvh_state.cctx = nullptr;
	hlbvh_state.rqueue = nullptr;
	hlbvh_state.rctx = nullptr;
	
	// kthxbye
	log_msg("done!");
	floor::destroy();
	return 0;
}
