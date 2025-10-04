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
#include <floor/core/timer.hpp>
#include <floor/core/option_handler.hpp>
#include <floor/device/device_function.hpp>
#include "unified_renderer.hpp"
#include "hlbvh_state.hpp"
#include "animation.hpp"
#include "collider.hpp"
#include "camera.hpp"
#include "radix_sort.hpp"
using namespace std::chrono_literals;

hlbvh_state_struct hlbvh_state;

struct hlbvh_option_context {
	// unused
	std::string additional_options { "" };
};
typedef option_handler<hlbvh_option_context> hlbvh_opt_handler;

// camera (free cam mode)
static std::unique_ptr<camera> cam;
// camera speeds (modified by shift/ctrl)
static const double3 cam_speeds { 25.0 /* default */, 150.0 /* faster */, 2.5 /* slower */ };

//! option -> function map
template<> std::vector<std::pair<std::string, hlbvh_opt_handler::option_function>> hlbvh_opt_handler::options {
	{ "--help", [](hlbvh_option_context&, char**&) {
		std::cout << "command line options:" << std::endl;
		std::cout << "\t--cuda: set default compute backend to CUDA" << std::endl;
		std::cout << "\t--host: set default compute backend to Host-Compute" << std::endl;
		std::cout << "\t--metal: set default compute backend to Metal" << std::endl;
		std::cout << "\t--opencl: set default compute backend to OpenCL" << std::endl;
		std::cout << "\t--vulkan: set default compute backend to Vulkan" << std::endl;
#if defined(__APPLE__)
		std::cout << "\t--no-metal: disables Metal rendering" << std::endl;
#endif
		std::cout << "\t--no-vulkan: disables Vulkan rendering" << std::endl;
		std::cout << "\t--benchmark: runs the simulation in benchmark mode, without rendering" << std::endl;
		std::cout << "\t--no-triangle-vis: disables triangle collision visualization and uses per-model visualization instead (faster)" << std::endl;
		std::cout << "\t--legacy-radix-sort: force the use of the legacy radix sort" << std::endl;
		std::cout << "\t--no-local-atomics: uses kernels that don't use local memory atomics" << std::endl;
		hlbvh_state.done = true;
		
		std::cout << std::endl;
		std::cout << "controls:" << std::endl;
		std::cout << "\tq: quit" << std::endl;
		std::cout << "\tspace: halt" << std::endl;
		std::cout << "\tc: switch camera mode between free cam and locked/rotational camera" << std::endl;
		std::cout << "\t* locked camera:" << std::endl;
		std::cout << "\t\tw/s/right-mouse-drag: move camera forwards/backwards" << std::endl;
		std::cout << "\t\tleft-mouse-drag: rotate camera" << std::endl;
		std::cout << "\t* free camera:" << std::endl;
		std::cout << "\t\tw/a/s/d/arrow-keys: move forward/left/backward/right" << std::endl;
		std::cout << "\t\tmove mouse: rotate camera" << std::endl;
		std::cout << "\tshift: faster camera movement" << std::endl;
		std::cout << "\tctrl: slower camera movement" << std::endl;
		std::cout << std::endl;
		
		// TODO: performance stats
		std::cout << std::endl;
	}},
	{ "--cuda", [](hlbvh_option_context&, char**&) {
		std::cout << "using CUDA" << std::endl;
		hlbvh_state.default_platform = PLATFORM_TYPE::CUDA;
	}},
	{ "--host", [](hlbvh_option_context&, char**&) {
		std::cout << "using Host-Compute" << std::endl;
		hlbvh_state.default_platform = PLATFORM_TYPE::HOST;
	}},
	{ "--metal", [](hlbvh_option_context&, char**&) {
		std::cout << "using Metal" << std::endl;
		hlbvh_state.default_platform = PLATFORM_TYPE::METAL;
	}},
	{ "--opencl", [](hlbvh_option_context&, char**&) {
		std::cout << "using OpenCL" << std::endl;
		hlbvh_state.default_platform = PLATFORM_TYPE::OPENCL;
	}},
	{ "--vulkan", [](hlbvh_option_context&, char**&) {
		std::cout << "using Vulkan" << std::endl;
		hlbvh_state.default_platform = PLATFORM_TYPE::VULKAN;
	}},
	{ "--no-metal", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_metal = true;
		std::cout << "Metal rendering disabled" << std::endl;
	}},
	{ "--no-vulkan", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_vulkan = true;
		std::cout << "Vulkan rendering disabled" << std::endl;
	}},
	{ "--no-triangle-vis", [](hlbvh_option_context&, char**&) {
		hlbvh_state.triangle_vis = false;
		std::cout << "triangle collision visualization disabled" << std::endl;
	}},
	{ "--legacy-radix-sort", [](hlbvh_option_context&, char**&) {
		hlbvh_state.improved_radix_sort = false;
		std::cout << "improved radix sort disabled" << std::endl;
	}},
	{ "--no-local-atomics", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_local_atomics = true;
		std::cout << "not using kernels with local memory atomics" << std::endl;
	}},
	{ "--benchmark", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_metal = true; // also disable metal
		hlbvh_state.no_vulkan = true; // also disable vulkan
		hlbvh_state.triangle_vis = false; // triangle visualization is unnecessary here
		hlbvh_state.benchmark = true;
		std::cout << "benchmark mode enabled" << std::endl;
	}},
	{ "--no-fubar", [](hlbvh_option_context&, char**&) {
		hlbvh_state.no_fubar = true;
		std::cout << "FUBAR disabled" << std::endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](hlbvh_option_context&, char**&) {} },
	{ "-ApplePersistenceIgnoreState", [](hlbvh_option_context&, char**&) {} },
};

static bool evt_handler(EVENT_TYPE type, std::shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		hlbvh_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((std::shared_ptr<key_up_event>&)obj)->key) {
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
		switch(((std::shared_ptr<key_up_event>&)obj)->key) {
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
			const auto& move_evt = (std::shared_ptr<mouse_move_event>&)obj;
			
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
#if __has_embed("../../data/hlbvh.fubar") && __has_embed("../../data/hlbvh_radix_sort.fubar") && __has_embed("../../data/hlbvh_shaders.fubar")
static constexpr const uint8_t hlbvh_fubar[] {
#embed "../../data/hlbvh.fubar"
};
static constexpr const uint8_t hlbvh_radix_sort_fubar[] {
#embed "../../data/hlbvh_radix_sort.fubar"
};
static constexpr const uint8_t hlbvh_shaders_fubar[] {
#embed "../../data/hlbvh_shaders.fubar"
};
#define HAS_EMBEDDED_FUBAR 1
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
		.default_platform = hlbvh_state.default_platform,
		.renderer = wanted_renderer,
		// disable resource tracking and enable non-blocking Vulkan execution
		.context_flags = DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING | DEVICE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING,
	})) {
		return -1;
	}
	
	
	// disable resp. other renderers when using opengl/metal/vulkan
	const auto floor_renderer = floor::get_renderer();
	const bool is_metal = ((floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::METAL ||
							floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::HOST) &&
						   floor_renderer == floor::RENDERER::METAL);
	const bool is_vulkan = ((floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::VULKAN ||
							 floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::CUDA ||
							 floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::HOST) &&
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
	
	// add event handlers
	event::handler_f evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_event_handler(evt_handler_fnctr,
										  EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN,
										  EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE,
										  EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
										  EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE);
	
	cam = std::make_unique<camera>();
	cam->set_mouse_input(hlbvh_state.cam_mode);
	cam->set_wasd_input(hlbvh_state.cam_mode);
	cam->set_keyboard_input(hlbvh_state.cam_mode);
	cam->set_rotation_wrapping(true);
	cam->set_position(-10.5, 6.0, 2.15);
	cam->set_rotation(28.0, 256.0);
	
	// get the compute and render contexts that has been automatically created (opencl/cuda/metal/vulkan/host, depending on the config)
	hlbvh_state.cctx = floor::get_device_context();
	hlbvh_state.rctx = floor::get_render_context();
	if (!hlbvh_state.rctx) {
		hlbvh_state.rctx = hlbvh_state.cctx;
	}
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	hlbvh_state.cdev = hlbvh_state.cctx->get_device(device::TYPE::FASTEST);
	hlbvh_state.cqueue = hlbvh_state.cctx->create_queue(*hlbvh_state.cdev);
	if (hlbvh_state.cctx != hlbvh_state.rctx) {
		// render context is not the same as the compute context -> also create dev and queue for the render context
		hlbvh_state.rdev = hlbvh_state.rctx->get_device(device::TYPE::FASTEST);
		hlbvh_state.rqueue = hlbvh_state.rctx->create_queue(*hlbvh_state.rdev);
	} else {
		// otherwise: use the same as the compute context
		hlbvh_state.rdev = hlbvh_state.cdev;
		hlbvh_state.rqueue = hlbvh_state.cqueue;
	}
	
	//
	std::shared_ptr<device_program> prog;
	std::shared_ptr<device_program> radix_sort_prog;
	std::shared_ptr<device_program> shader_prog;
	
	// if embedded FUBAR data exists + it isn't disabled, try to load this first
#if defined(HAS_EMBEDDED_FUBAR)
	if (!hlbvh_state.no_fubar) {
		// hlbvh kernels/shaders
		const std::span<const uint8_t> fubar_data { hlbvh_fubar, std::size(hlbvh_fubar) };
		prog = hlbvh_state.cctx->add_universal_binary(fubar_data);
		if (prog) {
			log_msg("using embedded hlbvh FUBAR");
		}
		
		const std::span<const uint8_t> fubar_rs_data { hlbvh_radix_sort_fubar, std::size(hlbvh_radix_sort_fubar) };
		radix_sort_prog = hlbvh_state.cctx->add_universal_binary(fubar_rs_data);
		if (radix_sort_prog) {
			log_msg("using embedded hlbvh radix sort FUBAR");
		}
		
		if (!hlbvh_state.benchmark) {
			const std::span<const uint8_t> fubar_shader_data { hlbvh_shaders_fubar, std::size(hlbvh_shaders_fubar) };
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
		toolchain::compile_options options {
			.enable_warnings = true,
		};
		prog = hlbvh_state.cctx->add_program_file(floor::data_path("../hlbvh/src/hlbvh.cpp"), options);
		
		options.metal.restrictive_vectorization = true;
		radix_sort_prog = hlbvh_state.cctx->add_program_file(floor::data_path("../hlbvh/src/radix_sort.cpp"), options);
	}
#endif
	if (!prog) {
		log_error("program compilation/loading failed");
		return -1;
	}
	
	// get all kernels
	hlbvh_state.kernel_build_aabbs = prog->get_function("build_aabbs").get();
	hlbvh_state.kernel_collide_root_aabbs = prog->get_function("collide_root_aabbs").get();
	hlbvh_state.kernel_compute_morton_codes = prog->get_function("compute_morton_codes").get();
	hlbvh_state.kernel_build_bvh = prog->get_function("build_bvh").get();
	hlbvh_state.kernel_build_bvh_aabbs_leaves = prog->get_function("build_bvh_aabbs_leaves").get();
	hlbvh_state.kernel_build_bvh_aabbs = prog->get_function("build_bvh_aabbs").get();
	hlbvh_state.kernel_collide_bvhs_no_tri_vis = prog->get_function("collide_bvhs_no_tri_vis").get();
	hlbvh_state.kernel_collide_bvhs_tri_vis = prog->get_function("collide_bvhs_tri_vis").get();
	hlbvh_state.kernel_map_collided_triangles = prog->get_function("map_collided_triangles").get();
	hlbvh_state.kernel_indirect_radix_sort_count = prog->get_function("indirect_radix_sort_count").get();
	hlbvh_state.kernel_radix_sort_prefix_sum = prog->get_function("radix_sort_prefix_sum").get();
	hlbvh_state.kernel_indirect_radix_sort_stream_split = prog->get_function("indirect_radix_sort_stream_split").get();
	if (!hlbvh_state.kernel_build_aabbs ||
		!hlbvh_state.kernel_collide_root_aabbs ||
		!hlbvh_state.kernel_compute_morton_codes ||
		!hlbvh_state.kernel_build_bvh ||
		!hlbvh_state.kernel_build_bvh_aabbs_leaves ||
		!hlbvh_state.kernel_build_bvh_aabbs ||
		!hlbvh_state.kernel_collide_bvhs_no_tri_vis ||
		!hlbvh_state.kernel_collide_bvhs_tri_vis ||
		!hlbvh_state.kernel_map_collided_triangles ||
		!hlbvh_state.kernel_indirect_radix_sort_count ||
		!hlbvh_state.kernel_radix_sort_prefix_sum ||
		!hlbvh_state.kernel_indirect_radix_sort_stream_split) {
		log_error("missing HLBVH kernel(s)");
		return -1;
	}
	
	hlbvh_state.max_local_size_build_aabbs = hlbvh_state.kernel_build_aabbs->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_collide_root_aabbs = hlbvh_state.kernel_collide_root_aabbs->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_compute_morton_codes = hlbvh_state.kernel_compute_morton_codes->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_build_bvh = hlbvh_state.kernel_build_bvh->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_build_bvh_aabbs_leaves = hlbvh_state.kernel_build_bvh_aabbs_leaves->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_build_bvh_aabbs = hlbvh_state.kernel_build_bvh_aabbs->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_collide_bvhs_no_tri_vis = hlbvh_state.kernel_collide_bvhs_no_tri_vis->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_collide_bvhs_tri_vis = hlbvh_state.kernel_collide_bvhs_tri_vis->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_map_collided_triangles = hlbvh_state.kernel_map_collided_triangles->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_indirect_radix_sort_count = hlbvh_state.kernel_indirect_radix_sort_count->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_radix_sort_prefix_sum = hlbvh_state.kernel_radix_sort_prefix_sum->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	hlbvh_state.max_local_size_indirect_radix_sort_stream_split = hlbvh_state.kernel_indirect_radix_sort_stream_split->get_function_entry(*hlbvh_state.cdev)->max_total_local_size;
	
	// improved radix sort is optional and dependent on device support
	if (hlbvh_state.cdev->simd_width != 32u ||
		!hlbvh_state.cdev->sub_group_support ||
		!hlbvh_state.cdev->sub_group_ballot_support ||
		!hlbvh_state.cdev->sub_group_shuffle_support ||
		hlbvh_state.cctx->get_platform_type() == PLATFORM_TYPE::OPENCL) {
		hlbvh_state.improved_radix_sort = false;
	}
	if (hlbvh_state.improved_radix_sort) {
		hlbvh_state.kernel_indirect_radix_zero = radix_sort_prog->get_function("indirect_radix_zero").get();
		hlbvh_state.kernel_indirect_radix_upsweep_init = radix_sort_prog->get_function("indirect_radix_upsweep_init").get();
		hlbvh_state.kernel_indirect_radix_upsweep_pass_only = radix_sort_prog->get_function("indirect_radix_upsweep_pass_only").get();
		hlbvh_state.kernel_indirect_radix_upsweep = radix_sort_prog->get_function("indirect_radix_upsweep").get();
		hlbvh_state.kernel_indirect_radix_scan_small = radix_sort_prog->get_function("indirect_radix_scan_small").get();
		hlbvh_state.kernel_indirect_radix_scan = radix_sort_prog->get_function("indirect_radix_scan").get();
		hlbvh_state.kernel_indirect_radix_downsweep_keys = radix_sort_prog->get_function("indirect_radix_downsweep_keys").get();
		hlbvh_state.kernel_indirect_radix_downsweep_kv16 = radix_sort_prog->get_function("indirect_radix_downsweep_kv16").get();
		if (!hlbvh_state.kernel_indirect_radix_zero ||
			!hlbvh_state.kernel_indirect_radix_upsweep_init ||
			!hlbvh_state.kernel_indirect_radix_upsweep_pass_only ||
			!hlbvh_state.kernel_indirect_radix_upsweep ||
			!hlbvh_state.kernel_indirect_radix_scan_small ||
			!hlbvh_state.kernel_indirect_radix_scan ||
			!hlbvh_state.kernel_indirect_radix_downsweep_keys ||
			!hlbvh_state.kernel_indirect_radix_downsweep_kv16) {
			log_warn("missing improved radix sort kernel(s)");
			hlbvh_state.improved_radix_sort = false;
		} else {
			indirect_command_description desc {
				.command_type = indirect_command_description::COMMAND_TYPE::COMPUTE,
				.max_command_count = 4u * 3u + 1, // 4 passes with 3 kernels each + 1 init
				.debug_label = "radix_sort_pipeline"
			};
			desc.compute_buffer_counts_from_functions(*hlbvh_state.cdev, {
				hlbvh_state.kernel_indirect_radix_zero,
				hlbvh_state.kernel_indirect_radix_upsweep_init,
				hlbvh_state.kernel_indirect_radix_upsweep_pass_only,
				hlbvh_state.kernel_indirect_radix_upsweep,
				hlbvh_state.kernel_indirect_radix_scan_small,
				hlbvh_state.kernel_indirect_radix_scan,
				hlbvh_state.kernel_indirect_radix_downsweep_keys,
				hlbvh_state.kernel_indirect_radix_downsweep_kv16,
			});
			hlbvh_state.radix_sort_pipeline = hlbvh_state.cctx->create_indirect_command_pipeline(desc);
			if (!hlbvh_state.radix_sort_pipeline->is_valid()) {
				hlbvh_state.improved_radix_sort = false;
				log_error("failed to create indirect radix sort pipeline");
			} else {
				const std::array<radix_sort::indirect_radix_shift_t, radix_sort::sort_passes> radix_shift_params {{
					{
						.radix_shift = 0u,
					},
					{
						.radix_shift = 8u,
					},
					{
						.radix_shift = 16u,
					},
					{
						.radix_shift = 24u,
					},
				}};
				static_assert(hlbvh_state.radix_shift_param_buffers.size() == radix_sort::sort_passes);
				hlbvh_state.radix_shift_param_buffers = {
					hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, std::span { &radix_shift_params[0], sizeof(radix_sort::indirect_params_t) },
													MEMORY_FLAG::READ | MEMORY_FLAG::HEAP_ALLOCATION),
					hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, std::span { &radix_shift_params[1], sizeof(radix_sort::indirect_params_t) },
													MEMORY_FLAG::READ | MEMORY_FLAG::HEAP_ALLOCATION),
					hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, std::span { &radix_shift_params[2], sizeof(radix_sort::indirect_params_t) },
													MEMORY_FLAG::READ | MEMORY_FLAG::HEAP_ALLOCATION),
					hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, std::span { &radix_shift_params[3], sizeof(radix_sort::indirect_params_t) },
													MEMORY_FLAG::READ | MEMORY_FLAG::HEAP_ALLOCATION),
				};
				
				hlbvh_state.radix_sort_param_buffer = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue, sizeof(radix_sort::indirect_params_t),
																					  MEMORY_FLAG::READ | MEMORY_FLAG::HOST_WRITE |
																					  MEMORY_FLAG::HEAP_ALLOCATION |
																					  MEMORY_FLAG::VULKAN_HOST_COHERENT);
				
				hlbvh_state.radix_sort_global_histogram = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue,
																						  sizeof(uint32_t) * radix_sort::radix * radix_sort::sort_passes,
																						  MEMORY_FLAG::READ_WRITE | MEMORY_FLAG::HEAP_ALLOCATION);
				
				const auto max_group_count = (animation::max_triangle_count + radix_sort::partition_size - 1u) / radix_sort::partition_size;
				hlbvh_state.radix_sort_pass_histogram = hlbvh_state.cctx->create_buffer(*hlbvh_state.cqueue,
																						sizeof(uint32_t) * max_group_count * radix_sort::radix,
																						MEMORY_FLAG::READ_WRITE | MEMORY_FLAG::HEAP_ALLOCATION);
			}
		}
	}
	if (hlbvh_state.improved_radix_sort) {
		log_msg("using improved radix sort");
	} else {
		log_msg("using legacy radix sort");
	}
	
	// init unified renderer (need compiled prog first)
	if (!hlbvh_state.benchmark) {
		if (!shader_prog) {
			shader_prog = hlbvh_state.rctx->add_program_file(floor::data_path("../hlbvh/src/hlbvh_shaders.cpp"));
			if (!shader_prog) {
				log_error("shader program compilation failed");
				return -1;
			}
		}
		
		// setup renderer
		if (!unified_renderer::init(shader_prog->get_function("hlbvh_vertex"),
									shader_prog->get_function("hlbvh_fragment"))) {
			log_error("error during unified renderer initialization!");
			return -1;
		}
	}
	
	// load animated models
	std::vector<std::unique_ptr<animation>> models;
	models.emplace_back(std::make_unique<animation>("collision_models/gear/gear_0000", ".obj", 20, false, 0.1f));
	models.emplace_back(std::make_unique<animation>("collision_models/gear2/gear2_0000", ".obj", 20, false, 0.1f));
	models.emplace_back(std::make_unique<animation>("collision_models/sinbad/sinbad_0000", ".obj", 20, true));
	models.emplace_back(std::make_unique<animation>("collision_models/golem/golem_0000", ".obj", 20, false, 0.125f));
	models.emplace_back(std::make_unique<animation>("collision_models/plane/plane_00000", ".obj", 2));
	
	// create collider
	auto hlbvh_collider = std::make_unique<collider>();
	
	// main loop
	auto frame_time = core::unix_timestamp_us();
	while (!hlbvh_state.done) {
		floor::get_event()->handle_events();
		
#if !defined(FLOOR_IOS)
		// stop drawing if window is inactive
		if (!(SDL_GetWindowFlags(floor::get_window()) & SDL_WINDOW_INPUT_FOCUS) &&
			!hlbvh_state.benchmark) {
			std::this_thread::sleep_for(20ms);
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
		floor::set_caption("hlbvh | frame-time: " + std::to_string(frame_delta) + "ms");
		
		if (!hlbvh_state.benchmark) {
			// Metal/Vulkan rendering
			unified_renderer::render(models, hlbvh_state.cam_mode, *cam.get());
		}
	}
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	cam = nullptr;
	
	// cleanup
	models.clear();
	
	if (!hlbvh_state.benchmark) {
		unified_renderer::destroy();
	}
	
	hlbvh_collider = nullptr;
	prog = nullptr;
	radix_sort_prog = nullptr;
	shader_prog = nullptr;
	hlbvh_state.radix_sort_pipeline = nullptr;
	hlbvh_state.radix_shift_param_buffers.fill(nullptr);
	hlbvh_state.radix_sort_param_buffer = nullptr;
	hlbvh_state.radix_sort_global_histogram = nullptr;
	hlbvh_state.radix_sort_pass_histogram = nullptr;
	hlbvh_state.cqueue = nullptr;
	hlbvh_state.cctx = nullptr;
	hlbvh_state.rqueue = nullptr;
	hlbvh_state.rctx = nullptr;
	
	// kthxbye
	log_msg("done!");
	floor::destroy();
	return 0;
}
