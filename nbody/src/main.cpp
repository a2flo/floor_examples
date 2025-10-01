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
#include <floor/device/device_function.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/device/host/host_context.hpp>
#include <floor/device/indirect_command.hpp>
#include <floor/vr/vr_context.hpp>
#include <chrono>
#include "unified_renderer.hpp"
#include "nbody_state.hpp"
#include <SDL3/SDL_main.h>
using namespace std;

nbody_state_struct nbody_state;

struct nbody_option_context {
	// unused
	string additional_options { "" };
};
typedef option_handler<nbody_option_context> nbody_opt_handler;

// all initial nbody system setups
enum class NBODY_SETUP : uint32_t {
	PSEUDO_GALAXY,
	CUBE,
	ON_SPHERE,
	IN_SPHERE,
	RING,
	STAR_COLLAPSE,
	__MAX_NBODY_SETUP
};
static NBODY_SETUP nbody_setup { NBODY_SETUP::ON_SPHERE };
static constexpr const array<const char*, (size_t)NBODY_SETUP::__MAX_NBODY_SETUP> nbody_setup_desc {{
	"pseudo-galaxy: spawns bodies in a round pseudo galaxy with body height ~= log(total-radius - body-distance + 1)",
	"cube: spawns bodies randomly inside a cube (uniform distribution)",
	"on-sphere: spawns bodies randomly on a sphere (uniform distribution)",
	"in-sphere: spawns bodies randomly inside a sphere (uniform distribution)",
	"ring: spawns bodies in a ring (uniform distribution)",
	"star-collapse: pseudo star collapse (multiple spherical shells of varying mass)",
}};

// device compute/command queue
static shared_ptr<device_queue> dev_queue;
static shared_ptr<device_queue> render_dev_queue;
// amount of nbody position buffers (need at least 2)
static constexpr const size_t pos_buffer_count { 3 };
// nbody position buffers
static array<shared_ptr<device_buffer>, pos_buffer_count> position_buffers;
// nbody velocity buffer
static shared_ptr<device_buffer> velocity_buffer;
// iterates over [0, pos_buffer_count - 1] (-> currently active position buffer)
static size_t buffer_flip_flop { 0 };
// current iteration number (used to track/compute gflops, resets every 100 iterations)
static size_t iteration { 0 };
// used to track/compute gflops, resets every 100 iterations
static double sim_time_sum { 0.0 };
// initializes (or resets) the current nbody system
static void init_system();
// exports the current nbody system to a file
static void export_nbody_system();
// set to true when the nbody system should be exported after the next run
static atomic<bool> export_next_run { false };
// the amount of nbody compute iterations that will be performed for the benchmark
static constexpr const uint32_t benchmark_iterations { 100 };
// when using indirect command pipelines: this contains the full 100 iterations of the nbody benchmark
static unique_ptr<indirect_command_pipeline> indirect_benchmark_pipeline;

//! option -> function map
template<> vector<pair<string, nbody_opt_handler::option_function>> nbody_opt_handler::options {
	{ "--help", [](nbody_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--count <count>: specify the amount of bodies (default: " << nbody_state.body_count << ")" << endl;
		cout << "\t--tile-size <count>: sets the tile size / work-group size, thus also the amount of used local memory and unrolling (default: " << nbody_state.tile_size << ")" << endl;
		cout << "\t--time-step <step-size>: sets the time step size (default: " << nbody_state.time_step << ")" << endl;
		cout << "\t--mass <min> <max>: sets the random mass interval (default: " << nbody_state.mass_minmax_default << ")" << endl;
		cout << "\t--softening <softening>: sets the simulation softening (default: " << nbody_state.softening << ")" << endl;
		cout << "\t--damping <damping>: sets the simulation damping (default: " << nbody_state.damping << ")" << endl;
#if defined(__APPLE__)
		cout << "\t--no-metal: disables metal rendering (uses s/w rendering instead)" << endl;
#endif
		cout << "\t--no-vulkan: disables vulkan rendering (uses s/w rendering instead)" << endl;
		cout << "\t--benchmark: runs the simulation in benchmark mode, without rendering" << endl;
		cout << "\t--type <type>: sets the initial nbody setup (default: on-sphere)" << endl;
		for(const auto& desc : nbody_setup_desc) {
			cout << "\t\t" << desc << endl;
		}
		cout << "\t--render-size <work-items>: sets the amount of work-items/work-group when using s/w rendering" << endl;
		cout << "\t--no-msaa: disable 4xMSAA rendering" << endl;
		cout << "\t--no-indirect: disables indirect command pipeline usage" << endl;
		cout << "\t--no-fubar: don't use the compiled FUBAR file/data if it exists (integrated or on disk)" << endl;
		nbody_state.done = true;
		
		cout << endl;
		cout << "controls:" << endl;
		cout << "\tq: quit" << endl;
		cout << "\tspace: halt simulation" << endl;
		cout << "\tr: reset simulation" << endl;
		cout << "\tw/s/right-mouse-drag: move camera forwards/backwards" << endl;
		cout << "\tleft-mouse-drag: rotate camera" << endl;
		cout << "\tt: toggle rendering between alpha/transparent and solid non-transparent particles" << endl;
		cout << "\t1-5: reset simulation and set used nbody setup (";
		for(uint32_t num = 1; num <= (uint32_t)NBODY_SETUP::__MAX_NBODY_SETUP; ++num) {
			const string desc_str = nbody_setup_desc[num - 1];
			cout << num << " = " << desc_str.substr(0, desc_str.find(':'));
			if(num < (uint32_t)NBODY_SETUP::__MAX_NBODY_SETUP) cout << ", ";
		}
		cout << ")" << endl;
		cout << endl;
		
		// performance stats
		cout << "expected performance (with --benchmark):" << endl;
		cout << "\tRTX 3090:     ~21200 gflops (--count 335872 --tile-size 512)" << endl;
		cout << "\tRX 6900 XT:   ~14760 gflops (--count 327680 --tile-size 32)" << endl;
		cout << "\tRTX 2080 Ti:  ~11190 gflops (--count 278528 --tile-size 256)" << endl;
		cout << "\tP6000:        ~ 8400 gflops (--count 262144 --tile-size 512)" << endl;
		cout << "\tGP100:        ~ 7600 gflops (--count 262144 --tile-size 512)" << endl;
		cout << "\tM1 Max:       ~ 5070 gflops (--count 262144 --tile-size 256)" << endl;
		cout << "\tRX 590:       ~ 4120 gflops (--count 221184 --tile-size 64)" << endl;
		cout << "\tRX 580:       ~ 3225 gflops (--count 221184 --tile-size 64)" << endl;
		cout << "\tGTX 970:      ~ 2770 gflops (--count 131072 --tile-size 256)" << endl;
		cout << "\tGTX 780:      ~ 2350 gflops (--count 131072 --tile-size 512)" << endl;
		cout << "\tR9 285:       ~ 1865 gflops (--count 131072 --tile-size 64)" << endl;
		cout << "\tGTX 1050 Ti:  ~ 1690 gflops (--count 262144 --tile-size 256)" << endl;
		cout << "\tRyzen 7950X3D:~ 1210 gflops (--count 65536 --tile-size 512)" << endl;
		cout << "\tA17 Pro:      ~ 1100 gflops (--count 131072 --tile-size 256)" << endl;
		cout << "\ti9-7980XE:    ~ 1060 gflops (--count 73728 --tile-size 64)" << endl;
		cout << "\tVan Gogh GPU: ~ 1000 gflops (--count 262144 --tile-size 256)" << endl;
		cout << "\tGTX 750:      ~  840 gflops (--count 65536 --tile-size 256)" << endl;
		cout << "\tRyzen 7700X:  ~  495 gflops (--count 65536 --tile-size 512)" << endl;
		cout << "\tGT 650M:      ~  385 gflops (--count 65536 --tile-size 512)" << endl;
		cout << "\tiPad A12:     ~  320 gflops (--count 32768 --tile-size 512)" << endl;
		cout << "\tHD 530:       ~  242 gflops (--count 65536 --tile-size 128)" << endl;
		cout << "\tHD 4600:      ~  235 gflops (--count 65536 --tile-size 80)" << endl;
		cout << "\tM1 (8P2E):    ~  228 gflops (--count 102400 --tile-size 1024)" << endl;
		cout << "\ti7-6700:      ~  195 gflops (--count 32768 --tile-size 1024)" << endl;
		cout << "\tHD 4000:      ~  165 gflops (--count 32768 --tile-size 128)" << endl;
		cout << "\tiPhone A10:   ~  149 gflops (--count 32768 --tile-size 512)" << endl;
		cout << "\ti7-5820K:     ~  105 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\ti7-4770:      ~   80 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\tVan Gogh CPU: ~   52 gflops (--count 65536 --tile-size 1024)" << endl;
		cout << "\ti7-3615QM:    ~   38 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\ti7-950:       ~   29 gflops (--count 32768 --tile-size 4)" << endl;
		cout << "\tiPhone A8:    ~   28 gflops (--count 16384 --tile-size 512)" << endl;
		cout << "\tiPad A7:      ~   20 gflops (--count 16384 --tile-size 512)" << endl;
		cout << endl;
	}},
	{ "--count", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --count!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.body_count = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		cout << "body count set to: " << nbody_state.body_count << endl;
	}},
	{ "--tile-size", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --tile-size!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.tile_size = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		cout << "tile size set to: " << nbody_state.tile_size << endl;
	}},
	{ "--time-step", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --time-step!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.time_step = strtof(*arg_ptr, nullptr);
		cout << "time step set to: " << nbody_state.time_step << endl;
	}},
	{ "--mass", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --mass!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.mass_minmax_default.x = strtof(*arg_ptr, nullptr);
		
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after second --mass parameter!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.mass_minmax_default.y = strtof(*arg_ptr, nullptr);
		cout << "mass set to: " << nbody_state.mass_minmax_default << endl;
	}},
	{ "--softening", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --softening!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.softening = strtof(*arg_ptr, nullptr);
		cout << "softening set to: " << nbody_state.softening << endl;
	}},
	{ "--damping", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --damping!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.damping = strtof(*arg_ptr, nullptr);
		cout << "damping set to: " << nbody_state.damping << endl;
	}},
	{ "--no-metal", [](nbody_option_context&, char**&) {
		nbody_state.no_metal = true;
		cout << "metal disabled" << endl;
	}},
	{ "--no-vulkan", [](nbody_option_context&, char**&) {
		nbody_state.no_vulkan = true;
		cout << "vulkan disabled" << endl;
	}},
	{ "--benchmark", [](nbody_option_context&, char**&) {
		nbody_state.no_metal = true; // also disable metal
		nbody_state.no_vulkan = true; // also disable vulkan
		nbody_state.benchmark = true;
		cout << "benchmark mode enabled" << endl;
	}},
	{ "--type", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --type!" << endl;
			nbody_state.done = true;
			return;
		}
		
		const string arg_str = *arg_ptr;
		uint32_t cur_setup { 0 };
		for(const auto& desc : nbody_setup_desc) {
			const string desc_str = desc;
			const auto desc_type = desc_str.substr(0, desc_str.find(':'));
			if(arg_str == desc_type) {
				nbody_setup = (NBODY_SETUP)cur_setup;
				cout << "nbody setup set to: " << desc_type << endl;
				return;
			}
			++cur_setup;
		}
		cerr << "unknown nbody setup: " << arg_str << endl;
	}},
	{ "--render-size", [](nbody_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --render-size!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.render_size = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		cout << "render size set to: " << nbody_state.render_size << endl;
	}},
	{ "--no-msaa", [](nbody_option_context&, char**&) {
		nbody_state.msaa = false;
		cout << "MSAA rendering disabled" << endl;
	}},
	{ "--no-indirect", [](nbody_option_context&, char**&) {
		nbody_state.no_indirect = true;
		cout << "indirect command pipelines disabled" << endl;
	}},
	{ "--no-fubar", [](nbody_option_context&, char**&) {
		nbody_state.no_fubar = true;
		cout << "FUBAR disabled" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](nbody_option_context&, char**&) {} },
	{ "-ApplePersistenceIgnoreState", [](nbody_option_context&, char**&) {} },
};

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if (type == EVENT_TYPE::QUIT) {
		nbody_state.done = true;
		return true;
	} else if (type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_Q:
				nbody_state.done = true;
				break;
			case SDLK_SPACE:
				nbody_state.stop ^= true;
				break;
			case SDLK_T:
				nbody_state.alpha_mask ^= true;
				break;
			case SDLK_R:
				init_system();
				break;
			case SDLK_1:
				nbody_setup = NBODY_SETUP::PSEUDO_GALAXY;
				init_system();
				break;
			case SDLK_2:
				nbody_setup = NBODY_SETUP::CUBE;
				init_system();
				break;
			case SDLK_3:
				nbody_setup = NBODY_SETUP::ON_SPHERE;
				init_system();
				break;
			case SDLK_4:
				nbody_setup = NBODY_SETUP::IN_SPHERE;
				init_system();
				break;
			case SDLK_5:
				nbody_setup = NBODY_SETUP::RING;
				init_system();
				break;
			case SDLK_6:
				nbody_setup = NBODY_SETUP::STAR_COLLAPSE;
				init_system();
				break;
			case SDLK_E:
				export_next_run = true;
				break;
			default: break;
		}
		return true;
	} else if (type == EVENT_TYPE::KEY_DOWN) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_W:
				nbody_state.distance = const_math::clamp(nbody_state.distance - 5.0f, 1.0f, nbody_state.max_distance);
				break;
			case SDLK_S:
				nbody_state.distance = const_math::clamp(nbody_state.distance + 5.0f, 1.0f, nbody_state.max_distance);
				break;
			default: break;
		}
		return true;
	} else if (type == EVENT_TYPE::MOUSE_LEFT_DOWN
#if defined(FLOOR_IOS) // NOTE: trackpads also generate finger events -> would lead to double input
			   || type == EVENT_TYPE::FINGER_DOWN
#endif
			  ) {
		nbody_state.enable_cam_rotate = true;
		return true;
	} else if (type == EVENT_TYPE::MOUSE_LEFT_UP
#if defined(FLOOR_IOS)
			   || type == EVENT_TYPE::FINGER_UP
#endif
			  ) {
		nbody_state.enable_cam_rotate = false;
		return true;
	} else if(type == EVENT_TYPE::MOUSE_RIGHT_DOWN) {
		nbody_state.enable_cam_move = true;
		return true;
	} else if(type == EVENT_TYPE::MOUSE_RIGHT_UP) {
		nbody_state.enable_cam_move = false;
		return true;
	} else if(type == EVENT_TYPE::MOUSE_MOVE
#if defined(FLOOR_IOS)
			   || type == EVENT_TYPE::FINGER_MOVE
#endif
			  ) {
		if(!(nbody_state.enable_cam_rotate ^ nbody_state.enable_cam_move)) {
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
		
		if(nbody_state.enable_cam_rotate) {
			// multiply by desired rotation speed
			static constexpr const float rot_speed { 100.0f };
			delta *= rot_speed;
			
			// multiply existing rotation by newly computed rotation around the x and y axis
			nbody_state.cam_rotation = (quaternionf::rotation_deg(delta.x, float3 { 0.0f, 1.0f, 0.0f }) *
										quaternionf::rotation_deg(delta.y, float3 { 1.0f, 0.0f, 0.0f })) * nbody_state.cam_rotation;
		} else {
			// multiply by desired move speed
			static constexpr const float move_speed { 250.0f };
			delta *= move_speed;
			nbody_state.distance = const_math::clamp(nbody_state.distance + delta.y, 1.0f, nbody_state.max_distance);
		}
		return true;
	} else if (type == EVENT_TYPE::VR_APP_MENU_PRESS) {
		// rotate setup
		const auto& press_evt = (shared_ptr<vr_app_menu_press_event>&)obj;
		if (press_evt->state) {
			nbody_setup = (NBODY_SETUP) ((uint32_t(nbody_setup) + 1u) % uint32_t(NBODY_SETUP::__MAX_NBODY_SETUP));
			init_system();
		}
		return true;
	} else if (type == EVENT_TYPE::VR_MAIN_PRESS) {
		// pause
		const auto& press_evt = (shared_ptr<vr_main_press_event>&)obj;
		if (press_evt->state) {
			nbody_state.stop ^= true;
		}
		return true;
	} else if (type == EVENT_TYPE::VR_GRIP_PRESS) {
		// reset
		const auto& press_evt = (shared_ptr<vr_grip_press_event>&)obj;
		if (press_evt->state) {
			init_system();
		}
		return true;
	} else if (type == EVENT_TYPE::VR_GRIP_FORCE) {
		// reset
		const auto& grip_force_evt = (shared_ptr<vr_grip_force_event>&)obj;
		if (grip_force_evt->force > 0.98f) {
			init_system();
		}
		return true;
	} else if (type == EVENT_TYPE::VR_GRIP_PULL) {
		// nop
		[[maybe_unused]] const auto& grip_pull_evt = (shared_ptr<vr_grip_pull_event>&)obj;
		return true;
	} else if (type == EVENT_TYPE::VR_THUMBSTICK_PRESS) {
		// quit if both thumbstick buttons are pressed
		const auto& press_evt = (shared_ptr<vr_grip_press_event>&)obj;
		static bool left_thumbstick_pressed = false;
		static bool right_thumbstick_pressed = false;
		if (press_evt->is_left_controller()) {
			left_thumbstick_pressed = press_evt->state;
		} else {
			right_thumbstick_pressed = press_evt->state;
		}
		if (left_thumbstick_pressed && right_thumbstick_pressed) {
			nbody_state.done = true;
		}
		return true;
	} else if (type == EVENT_TYPE::VR_TRIGGER_PULL) {
		// slow-mo
		const auto& pull_evt = (shared_ptr<vr_trigger_pull_event>&)obj;
		static const auto default_time_step = nbody_state.time_step;
		nbody_state.time_step = default_time_step * (1.0f - pull_evt->pull);
		return true;
	} else if (type == EVENT_TYPE::VR_THUMBSTICK_MOVE) {
		// rotate cam
		const auto& move_evt = (shared_ptr<vr_thumbstick_move_event>&)obj;
		static constexpr const float rot_speed { 2.0f };
		// multiply existing rotation by newly computed rotation around the x and y axis
		nbody_state.cam_rotation *= (quaternionf::rotation_deg(move_evt->position.x * rot_speed, float3 { 0.0f, 1.0f, 0.0f }) *
									 quaternionf::rotation_deg(-move_evt->position.y * rot_speed, float3 { 1.0f, 0.0f, 0.0f }));
		return true;
	} else if (type == EVENT_TYPE::VR_TRACKPAD_FORCE) {
		// reset
		[[maybe_unused]] const auto& trackpad_force_evt = (shared_ptr<vr_trackpad_force_event>&)obj;
		return true;
	} else if (type == EVENT_TYPE::VR_TRACKPAD_MOVE) {
		// move cam
		const auto& move_evt = (shared_ptr<vr_trackpad_move_event>&)obj;
		static constexpr const float move_speed { 2.0f };
		nbody_state.distance = const_math::clamp(nbody_state.distance - move_evt->position.y * move_speed, 1.0f, nbody_state.max_distance);
		return true;
	} else if (type == EVENT_TYPE::VR_TRACKPAD_PRESS) {
		[[maybe_unused]] const auto& press_evt = (shared_ptr<vr_trackpad_press_event>&)obj;
		return true;
	} else if (type == EVENT_TYPE::VR_GRIP_TOUCH) {
		[[maybe_unused]] const auto& touch_evt = (shared_ptr<vr_grip_touch_event>&)obj;
		return true;
	}
	return false;
}

void init_system() {
	// finish up old execution before we start with anything new
	dev_queue->finish();
	
	auto positions = (float4*)position_buffers[0]->map(*dev_queue, MEMORY_MAP_FLAG::WRITE_INVALIDATE | MEMORY_MAP_FLAG::BLOCK);
	auto velocities = (float3*)velocity_buffer->map(*dev_queue, MEMORY_MAP_FLAG::WRITE_INVALIDATE | MEMORY_MAP_FLAG::BLOCK);
	if(nbody_setup != NBODY_SETUP::STAR_COLLAPSE) {
		nbody_state.mass_minmax = nbody_state.mass_minmax_default;
	}
	else {
		nbody_state.mass_minmax = float2 { 1.008f, 55.845f };
	}
	for(size_t i = 0; i < nbody_state.body_count; ++i) {
		constexpr const float sphere_scale { 10.0f };
		switch(nbody_setup) {
			case NBODY_SETUP::PSEUDO_GALAXY: {
				static constexpr const float disc_radius { 12.0f };
				static constexpr const float height_dev { 3.0f };
				const float rnd_dir { core::rand(const_math::PI_MUL_2<float>) };
				const float rnd_radius { core::rand(disc_radius) };
				positions[i].x = rnd_radius * sinf(rnd_dir);
				positions[i].z = rnd_radius * cosf(rnd_dir);
				const float ln_height = log(1.0f + const_math::PI_MUL_2<float> * (1.0f - (rnd_radius / disc_radius))); // ~[0, 2]
				positions[i].y = core::rand(height_dev) * (ln_height * 0.5f) * (core::rand(0, 15) % 2 == 0 ? -1.0f : 1.0f);
				//const float2 vel = float2(positions[i].x, positions[i].z).perpendicular() * (rnd_radius * rnd_radius) * 0.001f;
				const float2 vel = float2(positions[i].x, positions[i].z).perpendicular() * (rnd_radius * rnd_radius) * 0.1f;
				velocities[i].x = vel.x;
				velocities[i].y = core::rand(-0.0001f, 0.0001f);
				velocities[i].z = vel.y;
			} break;
			case NBODY_SETUP::CUBE:
				positions[i].xyz = float3 { float3::random(-10.0f, 10.0f) };
				velocities[i] = float3::random(-10.0f, 10.0f);
				break;
			case NBODY_SETUP::ON_SPHERE: {
				const auto theta = core::rand(const_math::PI_MUL_2<float>);
				const auto xi_y = core::rand(-1.0f, 1.0f);
				const auto sqrt_term = sqrt(1.0f - xi_y * xi_y);
				positions[i].xyz = {
					cos(theta) * sphere_scale * sqrt_term,
					sin(theta) * sphere_scale * sqrt_term,
					sphere_scale * xi_y
				};
				velocities[i] = -positions[i].xyz * 0.1f;
			} break;
			case NBODY_SETUP::IN_SPHERE: {
				const auto theta = core::rand(const_math::PI_MUL_2<float>);
				const auto xi_y = core::rand(-1.0f, 1.0f);
				const auto sqrt_term = sqrt(1.0f - xi_y * xi_y);
				const auto rad = core::rand(-1.0f, 1.0f);
				const auto rnd_rad = sphere_scale * sqrt(1.0f - rad * rad) * (rad < 0.0f ? -1.0f : 1.0f);
				positions[i].xyz = {
					cos(theta) * rnd_rad * sqrt_term,
					sin(theta) * rnd_rad * sqrt_term,
					rnd_rad * xi_y
				};
				velocities[i] = -positions[i].xyz * 0.1f;
			} break;
			case NBODY_SETUP::RING: {
				const auto theta = core::rand(const_math::PI_MUL_2<float>);
				positions[i].xyz = float3 { cos(theta) * sphere_scale, sin(theta) * sphere_scale, 0.0f };
				velocities[i] = -positions[i].xyz * 0.1f;
			} break;
			case NBODY_SETUP::STAR_COLLAPSE: {
				static constexpr const size_t shell_count { 7 };
				static constexpr const float rad_per_shell { 1.0f / float(shell_count) };
				static constexpr const array<float, shell_count> shell_masses {{
					1.008f, 4.002602f, 12.011f, 20.1797f, 15.999f, 28.085f, 55.845f,
				}};
				const auto shell = core::rand(shell_count - 1);
				positions[i].w = shell_masses[shell];
				
				const auto theta = core::rand(const_math::PI_MUL_2<float>);
				const auto xi_y = core::rand(-1.0f, 1.0f);
				const auto sqrt_term = sqrt(1.0f - xi_y * xi_y);
				const auto rad = (core::rand(rad_per_shell * float(shell), rad_per_shell * float(shell + 1)) *
								  (core::rand(0, 15) % 2 == 0 ? 1.0f : -1.0f));
				const auto rnd_rad = sphere_scale * sqrt(1.0f - rad * rad) * (rad < 0.0f ? -1.0f : 1.0f);
				positions[i].xyz = {
					cos(theta) * rnd_rad * sqrt_term,
					sin(theta) * rnd_rad * sqrt_term,
					rnd_rad * xi_y
				};
				velocities[i] = -positions[i].xyz * 5.0f;
			} break;
			default: floor_unreachable();
		}
		
		if(nbody_setup != NBODY_SETUP::STAR_COLLAPSE) {
			positions[i].w = core::rand(nbody_state.mass_minmax.x, nbody_state.mass_minmax.y);
		}
	}
	position_buffers[0]->unmap(*dev_queue, positions);
	velocity_buffer->unmap(*dev_queue, velocities);
	
	// reset everything
	buffer_flip_flop = 0;
	iteration = 0;
	sim_time_sum = 0.0L;
}

void export_nbody_system() {
	log_msg("exporting nbody system ...");
	
	dev_queue->finish();
	
	const auto& cur_pos_buffer = position_buffers[buffer_flip_flop];
	auto positions = make_unique<float4[]>(nbody_state.body_count);
	std::span positions_span { positions.get(), nbody_state.body_count };
	cur_pos_buffer->read(*dev_queue, positions_span.data(), positions_span.size_bytes());
	if (!file_io::buffer_to_file("nbody_positions_" + to_string(nbody_state.body_count) + ".bin",
								 (const char*)positions_span.data(), positions_span.size_bytes())) {
		log_error("failed to export nbody system");
	} else {
		log_msg("exported nbody system!");
	}
}

// embed the compiled nbody FUBAR file if it is available
#if __has_embed("../../data/nbody.fubar")
static constexpr const uint8_t nbody_fubar[] {
#embed "../../data/nbody.fubar"
};
#define HAS_EMBEDDED_FUBAR 1
#endif

int main(int, char* argv[]) {
	nbody_option_context option_ctx;
	nbody_opt_handler::parse_options(argv + 1, option_ctx);
	if(nbody_state.done) return 0;
	
	// disable renderers that aren't available
#if defined(FLOOR_NO_METAL)
	nbody_state.no_metal = true;
#endif
#if defined(FLOOR_NO_VULKAN)
	nbody_state.no_vulkan = true;
#endif
	
	// init floor
	if (!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "nbody",
		.console_only = nbody_state.benchmark,
		.renderer = (// no renderer when running in console-only mode
					 nbody_state.benchmark ? floor::RENDERER::NONE :
					 // if Metal/Vulkan aren't disabled, use the default renderer for the platform
					 (!nbody_state.no_metal && !nbody_state.no_vulkan) ? floor::RENDERER::DEFAULT :
					 // else: choose a specific one
#if !defined(__APPLE__) && !defined(FLOOR_NO_VULKAN)
					 !nbody_state.no_vulkan ? floor::RENDERER::VULKAN :
#endif
#if defined(__APPLE__) && !defined(FLOOR_NO_METAL)
					 !nbody_state.no_metal ? floor::RENDERER::METAL :
#endif
					 // Vulkan/Metal are disabled
					 floor::RENDERER::NONE),
		.context_flags = DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING | DEVICE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING,
	})) {
		return -1;
	}
	
	// disable resp. other renderers when using Metal/Vulkan
	const auto floor_renderer = floor::get_renderer();
	const bool is_metal = ((floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::METAL ||
							floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::HOST) &&
						   floor_renderer == floor::RENDERER::METAL);
	const bool is_vulkan = ((floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::VULKAN ||
							 floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::CUDA ||
							 floor::get_device_context()->get_platform_type() == PLATFORM_TYPE::HOST) &&
							floor_renderer == floor::RENDERER::VULKAN);
	
	if (is_metal) {
		nbody_state.no_vulkan = true;
	} else {
		nbody_state.no_metal = true;
	}
	
	if (is_vulkan) {
		if (floor_renderer == floor::RENDERER::VULKAN) {
			nbody_state.no_metal = true;
		}
	} else {
		nbody_state.no_vulkan = true;
	}
	
	log_debug("using $", (floor_renderer != floor::RENDERER::NONE ? "unified renderer" : "s/w rasterizer"));

	// get the compute and render context that have been automatically created (opencl/cuda/metal/vulkan/host)
	// NOTE: for Metal and Vulkan these are the same
	auto compute_ctx = floor::get_device_context();
	auto render_ctx = floor::get_render_context();

	// create a compute queue (aka command queue or stream) for the fastest device in the context
	auto compute_dev = compute_ctx->get_device(device::TYPE::FASTEST);
	dev_queue = compute_ctx->create_queue(*compute_dev);

	// also retrieve the fastest device from the render context,
	// if it's not the same device (== same context), also create a queue for it (else, use the same queue)
	const device* render_dev { nullptr };
	if (render_ctx) {
		render_dev = render_ctx->get_device(device::TYPE::FASTEST);
		render_dev_queue = (render_dev != compute_dev ? render_ctx->create_queue(*render_dev) : dev_queue);
	}

	// parameter sanity check
	if (nbody_state.tile_size > compute_dev->max_total_local_size) {
		nbody_state.tile_size = (uint32_t)compute_dev->max_total_local_size;
		log_error("tile size too large, > max possible work-group size! - setting tile size to $ now", nbody_state.tile_size);
	}
	if ((nbody_state.body_count % nbody_state.tile_size) != 0u) {
		nbody_state.body_count = ((nbody_state.body_count / nbody_state.tile_size) + 1u) * nbody_state.tile_size;
		log_error("body count not a multiple of tile size! - setting body count to $ now", nbody_state.body_count);
	}
	if (compute_ctx->get_platform_type() == PLATFORM_TYPE::HOST &&
		!((const host_context*)compute_ctx.get())->has_host_device_support() && nbody_state.tile_size != NBODY_TILE_SIZE) {
		log_error("compiled NBODY_TILE_SIZE ($) must match run-time tile-size when using host compute! - using it now",
				  NBODY_TILE_SIZE);
		nbody_state.tile_size = NBODY_TILE_SIZE;
	}

	shared_ptr<device_program> nbody_prog;
	shared_ptr<device_program> nbody_render_prog;
	shared_ptr<device_function> nbody_compute;
	shared_ptr<device_function> nbody_compute_fixed_delta;
	shared_ptr<device_function> nbody_raster;
	
	// if embedded FUBAR data exists + it isn't disabled, try to load this first
#if defined(HAS_EMBEDDED_FUBAR)
	if (!nbody_state.no_fubar) {
		// nbody kernels/shaders
		const span<const uint8_t> fubar_data { nbody_fubar, std::size(nbody_fubar) };
		nbody_prog = compute_ctx->add_universal_binary(fubar_data);
		nbody_render_prog =
			(compute_ctx != render_ctx && render_ctx ? render_ctx->add_universal_binary(fubar_data) : nbody_prog);
		if (nbody_prog && nbody_render_prog) {
			log_msg("using embedded nbody FUBAR");
		}
	}
#endif

	// otherwise: compile the program
	if (!nbody_prog) {
#if !defined(FLOOR_IOS)
		toolchain::compile_options options {
			.cli = ("-I" + floor::data_path("../nbody/src") + " -DNBODY_TILE_SIZE=" + to_string(nbody_state.tile_size) +
					" -DNBODY_SOFTENING=" + to_string(nbody_state.softening) + "f" +
					" -DNBODY_DAMPING=" + to_string(nbody_state.damping) + "f"),
			// override max registers that can be used, this is beneficial here as it yields about +10% of performance
			.cuda.max_registers = 36,
		};
		nbody_prog = compute_ctx->add_program_file(floor::data_path("../nbody/src/nbody.cpp"), options);
		nbody_render_prog = (compute_ctx != render_ctx && render_ctx
								 ? render_ctx->add_program_file(floor::data_path("../nbody/src/nbody.cpp"), options)
								 : nbody_prog);
#else
		nbody_prog = compute_ctx->add_universal_binary(floor::data_path("nbody.fubar"));
		nbody_render_prog =
			(compute_ctx != render_ctx && render_ctx ? render_ctx->add_universal_binary(floor::data_path("nbody.fubar"))
													 : nbody_prog);
#endif
	}
	if (!nbody_prog || !nbody_render_prog) {
		log_error("program compilation failed");
		return -1;
	}

	// get the kernel functions
	nbody_compute = nbody_prog->get_function("nbody_compute");
	nbody_compute_fixed_delta = nbody_prog->get_function("nbody_compute_fixed_delta");
	nbody_raster = nbody_prog->get_function("nbody_raster");
	if (nbody_compute == nullptr || nbody_compute_fixed_delta == nullptr || nbody_raster == nullptr) {
		log_error("failed to retrieve kernel(s) from program");
		return -1;
	}

	// init unified/metal/vulkan renderers (need compiled prog first)
	if (floor_renderer != floor::RENDERER::NONE) {
		// setup renderer
		if (!unified_renderer::init(
				*render_ctx, *render_dev_queue, nbody_state.msaa, *nbody_render_prog->get_function("lighting_vertex"),
				*nbody_render_prog->get_function("lighting_fragment"), nbody_render_prog->get_function("blit_vs").get(),
				nbody_render_prog->get_function("blit_fs").get(), nbody_render_prog->get_function("blit_fs_layered").get())) {
			log_error("error during unified renderer initialization!");
			return -1;
		}
	}

	// create nbody position and velocity buffers
	MEMORY_FLAG graphics_sharing_flags = MEMORY_FLAG::NONE;
	if (compute_ctx != render_ctx && render_ctx != nullptr) {
		// enable automatic buffer sync between the render and compute backends
		graphics_sharing_flags = (MEMORY_FLAG::SHARING_SYNC |
								  // render backend only reads data
								  MEMORY_FLAG::SHARING_RENDER_READ |
								  // compute backend only writes data
								  MEMORY_FLAG::SHARING_COMPUTE_WRITE);
		if (!nbody_state.no_vulkan) {
			// we're using a Vulkan renderer -> enable Vulkan buffer sharing
			graphics_sharing_flags |= MEMORY_FLAG::VULKAN_SHARING;
		} else if (!nbody_state.no_metal) {
			// we're using a Metal renderer -> enable Metal buffer sharing
			graphics_sharing_flags |= MEMORY_FLAG::METAL_SHARING;
		}
	}
	for (size_t i = 0; i < pos_buffer_count; ++i) {
		position_buffers[i] = compute_ctx->create_buffer(*dev_queue, sizeof(float4) * nbody_state.body_count,
														 ( // will be reading and writing from the kernel
															 MEMORY_FLAG::READ_WRITE
															 // host will only write data
															 | MEMORY_FLAG::HOST_WRITE
															 // graphics sharing flags are set above
															 | graphics_sharing_flags));
	}
	velocity_buffer = compute_ctx->create_buffer(*dev_queue, sizeof(float3) * nbody_state.body_count,
												 MEMORY_FLAG::READ_WRITE | MEMORY_FLAG::HOST_WRITE);

	// image buffers (for s/w rendering only)
	const uint2 img_size { floor::get_physical_screen_size() };
	size_t img_buffer_flip_flop { 0 };
	array<shared_ptr<device_buffer>, 2> img_buffers;
	if (floor_renderer == floor::RENDERER::NONE && !nbody_state.benchmark) {
		img_buffers = { {
			compute_ctx->create_buffer(*dev_queue, sizeof(uint32_t) * img_size.x * img_size.y,
									   MEMORY_FLAG::READ_WRITE | MEMORY_FLAG::HOST_READ),
			compute_ctx->create_buffer(*dev_queue, sizeof(uint32_t) * img_size.x * img_size.y,
									   MEMORY_FLAG::READ_WRITE | MEMORY_FLAG::HOST_READ),
		} };
		img_buffers[0]->zero(*dev_queue);
		img_buffers[1]->zero(*dev_queue);
	}

	// init nbody system
	init_system();

	// create the indirect command pipeline for benchmarking
	if (nbody_state.benchmark && !nbody_state.no_indirect) {
		do {
			indirect_command_description desc{ .command_type = indirect_command_description::COMMAND_TYPE::COMPUTE,
											   .max_command_count = benchmark_iterations,
											   .debug_label = "indirect_benchmark_pipeline" };
			desc.compute_buffer_counts_from_functions(*compute_dev, { nbody_compute_fixed_delta.get() });
			indirect_benchmark_pipeline = compute_ctx->create_indirect_command_pipeline(desc);
			if (!indirect_benchmark_pipeline->is_valid()) {
				log_error("failed to create indirect_benchmark_pipeline");
				// -> fall back to not using an indirect command pipeline
				nbody_state.no_indirect = true;
				indirect_benchmark_pipeline = nullptr;
				break;
			}

			// encode indirect pipeline
			static_assert(pos_buffer_count >= 2, "need at least 2 position buffers");
			for (uint32_t i = 0; i < benchmark_iterations; ++i) {
				// will only flip/flop between two position buffers here
				indirect_benchmark_pipeline->add_compute_command(*compute_dev, *nbody_compute_fixed_delta)
					.set_arguments(position_buffers[i % 2u], position_buffers[(i + 1u) % 2u], velocity_buffer)
					.execute(nbody_state.body_count, nbody_state.tile_size)
					.barrier();
			}
			indirect_benchmark_pipeline->complete();
		} while (false);
	}

	// add event handlers
	event::handler_f evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_event_handler(
		evt_handler_fnctr, EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN, EVENT_TYPE::MOUSE_LEFT_DOWN,
		EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE, EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
		EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE, EVENT_TYPE::VR_THUMBSTICK_MOVE,
		EVENT_TYPE::VR_TRACKPAD_MOVE, EVENT_TYPE::VR_TRACKPAD_FORCE, EVENT_TYPE::VR_TRACKPAD_PRESS,
		EVENT_TYPE::VR_APP_MENU_PRESS, EVENT_TYPE::VR_MAIN_PRESS, EVENT_TYPE::VR_GRIP_PRESS, EVENT_TYPE::VR_GRIP_FORCE,
		EVENT_TYPE::VR_GRIP_PULL, EVENT_TYPE::VR_GRIP_TOUCH, EVENT_TYPE::VR_TRIGGER_PULL, EVENT_TYPE::VR_THUMBSTICK_PRESS);
	
	// keeps track of the time between two simulation steps -> time delta in kernel
	auto time_keeper = chrono::high_resolution_clock::now();
	// is there a price for the most nested namespace/typedef?
	static const double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
	// main loop
	while (!nbody_state.done) {
		floor::get_event()->handle_events();
		
		// simple iteration time -> gflops mapping function (note that flops per interaction is a fixed number)
		const auto compute_gflops = [](const double& iter_time_in_ms, const bool use_fma) {
			if(!use_fma) {
				const size_t flops_per_body { 19 };
				const size_t flops_per_iter { size_t(nbody_state.body_count) * size_t(nbody_state.body_count) * flops_per_body };
				return ((1000.0 / iter_time_in_ms) * (double)flops_per_iter) / 1'000'000'000.0;
			}
			else {
				// NOTE: GPUs and recent CPUs support fma instructions, thus performing 2 floating point operations
				// in 1 cycle instead of 2 -> to account for that, compute some kind of "actual ops done" metric
				const size_t flops_per_body_fma { 13 };
				const size_t flops_per_iter_fma { size_t(nbody_state.body_count) * size_t(nbody_state.body_count) * flops_per_body_fma };
				return ((1000.0 / iter_time_in_ms) * (double)flops_per_iter_fma) / 1'000'000'000.0;
			}
		};
		
		// there is no proper dependency tracking yet, so always need to manually finish right now
		if(is_vulkan || is_metal) dev_queue->finish();
		
		// flip/flop buffer indices
		const size_t cur_buffer = buffer_flip_flop;
		const size_t next_buffer = (buffer_flip_flop + 1) % pos_buffer_count;
		if (!nbody_state.stop) {
			//log_debug("delta: $ms /// $ gflops", 1000.0f * float(((double)delta.count()) / time_den),
			//		  compute_gflops(1000.0 * (((double)delta.count()) / time_den), false));
			
			if (nbody_state.benchmark && indirect_benchmark_pipeline) {
				// if we using an indirect command pipeline: run it here once and complete the benchmark
				iteration = benchmark_iterations - 1u; // -> signal as complete
				
				// ensure any previous work is complete, then start the timing
				dev_queue->finish();
				time_keeper = chrono::high_resolution_clock::now();
				
				const device_queue::indirect_execution_parameters_t exec_params {
					.wait_until_completion = true,
					.debug_label = "nbody_benchmark",
				};
				dev_queue->execute_indirect(*indirect_benchmark_pipeline, exec_params);
			} else {
				// direct, one kernel execution per iteration:
				dev_queue->execute(*nbody_compute,
								   // total amount of work:
								   uint1 { nbody_state.body_count },
								   // work per work-group:
								   uint1 { nbody_state.tile_size },
								   // kernel arguments:
								   /* in_positions: */		position_buffers[cur_buffer],
								   /* out_positions: */		position_buffers[next_buffer],
								   /* velocities: */		velocity_buffer,
								   /* delta: */				/*float(((double)delta.count()) / time_den)*/
															// NOTE: could use a time-step scaler instead, but fixed size seems more reasonable
															nbody_state.time_step);
				buffer_flip_flop = next_buffer;
				dev_queue->finish(); // ensure all is complete
			}
			
			// time keeping
			auto now = chrono::high_resolution_clock::now();
			auto delta = now - time_keeper;
			time_keeper = now;
			
			// in ms
			sim_time_sum += ((double)delta.count()) / (time_den / 1000.0);
			
			if (iteration == benchmark_iterations - 1u) {
				log_debug("avg of $ iterations: $ms ### $ gflops",
						  benchmark_iterations, sim_time_sum / double(benchmark_iterations),
						  compute_gflops(sim_time_sum / double(benchmark_iterations), false));
				floor::set_caption("nbody / " + to_string(nbody_state.body_count) + " bodies / " +
								   to_string(compute_gflops(sim_time_sum / double(benchmark_iterations), false)) + " gflops");
				iteration = 0;
				sim_time_sum = 0.0L;
				
				// benchmark is done after 100 iterations
				if (nbody_state.benchmark) {
					nbody_state.done = true;
				}
			} else {
				++iteration;
			}
		}
		//break;
		
		// export if this was triggered
		if (export_next_run.exchange(false)) {
			export_nbody_system();
		}
		
		// s/w rendering
		if (floor_renderer == floor::RENDERER::NONE && !nbody_state.benchmark) {
			struct {
				const matrix4f mview;
				const uint2 img_size;
				const float2 mass_minmax;
				const uint32_t body_count;
			} raster_params {
				.mview = nbody_state.cam_rotation.to_matrix4() * matrix4f::translation(0.0f, 0.0f, -nbody_state.distance),
				.img_size = img_size,
				.mass_minmax = nbody_state.mass_minmax,
				.body_count = nbody_state.body_count,
			};
			img_buffer_flip_flop = 1 - img_buffer_flip_flop;
			dev_queue->execute(*nbody_raster,
							   // total amount of work:
							   uint1 { img_size.x * img_size.y },
							   // work per work-group:
							   uint1 {
								   nbody_state.render_size == 0 ?
								   nbody_raster->get_function_entry(*compute_dev)->max_total_local_size : nbody_state.render_size
							   },
							   // kernel arguments:
							   /* in_positions: */		position_buffers[buffer_flip_flop],
							   /* img: */				img_buffers[img_buffer_flip_flop],
							   /* img_old: */			img_buffers[1 - img_buffer_flip_flop],
							   /* raster_params: */		raster_params);
			
			if(is_vulkan || is_metal) dev_queue->finish();
			
			// grab the current image buffer data (read-only + blocking) ...
			auto img_data = (uchar4*)img_buffers[img_buffer_flip_flop]->map(*dev_queue, MEMORY_MAP_FLAG::READ | MEMORY_MAP_FLAG::BLOCK);
			
			// ... and blit it into the window
			const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
			SDL_LockSurface(wnd_surface);
			const uint2 surface_dim = { uint32_t(wnd_surface->w), uint32_t(wnd_surface->h) }; // TODO: figure out how to coerce sdl to create a 2x surface
			const uint2 render_dim = img_size.minned(floor::get_physical_screen_size());
			const uint2 scale = render_dim / surface_dim;
			const auto px_format_details = SDL_GetPixelFormatDetails(wnd_surface->format);
			for (uint32_t y = 0; y < surface_dim.y; ++y) {
				uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels + ((size_t)wnd_surface->pitch / sizeof(uint32_t)) * y;
				uint32_t img_idx = img_size.x * y * scale.y;
				for (uint32_t x = 0; x < surface_dim.x; ++x, img_idx += scale.x) {
					*px_ptr++ = SDL_MapRGB(px_format_details, nullptr, img_data[img_idx].x, img_data[img_idx].y, img_data[img_idx].z);
				}
			}
			img_buffers[img_buffer_flip_flop]->unmap(*dev_queue, img_data);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
		// Metal/Vulkan rendering
		else if (floor_renderer != floor::RENDERER::NONE && (!nbody_state.no_metal || !nbody_state.no_vulkan)) {
			unified_renderer::render(*render_ctx, *render_dev_queue, *position_buffers[cur_buffer]);
			floor::end_frame();
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	// cleanup
	dev_queue->finish();
	if (indirect_benchmark_pipeline) {
		indirect_benchmark_pipeline = nullptr;
	}
	for(size_t i = 0; i < pos_buffer_count; ++i) {
		position_buffers[i] = nullptr;
	}
	velocity_buffer = nullptr;
	for(auto img_buffer : img_buffers) {
		img_buffer = nullptr;
	}
	if (floor_renderer != floor::RENDERER::NONE) {
		unified_renderer::destroy(*render_ctx);
	}
	nbody_prog = nullptr;
	nbody_render_prog = nullptr;
	nbody_compute = nullptr;
	nbody_compute_fixed_delta = nullptr;
	nbody_raster = nullptr;
	img_buffers = {};
	dev_queue = nullptr;
	render_dev_queue = nullptr;
	compute_ctx = nullptr;
	render_ctx = nullptr;
	
	// kthxbye
	floor::destroy();
	return 0;
}
