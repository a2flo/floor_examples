/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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
#include "gl_renderer.hpp"
#include "metal_renderer.hpp"
#include "vulkan_renderer.hpp"
#include "nbody_state.hpp"
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
static shared_ptr<compute_queue> dev_queue;
// amount of nbody position buffers (need at least 2)
static constexpr const size_t pos_buffer_count { 3 };
// nbody position buffers
static array<shared_ptr<compute_buffer>, pos_buffer_count> position_buffers;
// nbody velocity buffer
static shared_ptr<compute_buffer> velocity_buffer;
// iterates over [0, pos_buffer_count - 1] (-> currently active position buffer)
static size_t buffer_flip_flop { 0 };
// current iteration number (used to track/compute gflops, resets every 100 iterations)
static size_t iteration { 0 };
// used to track/compute gflops, resets every 100 iterations
static double sim_time_sum { 0.0 };
// initializes (or resets) the current nbody system
static void init_system();

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
		cout << "\t--no-opengl: disables opengl rendering (uses s/w rendering instead)" << endl;
#if defined(__APPLE__)
		cout << "\t--no-metal: disables metal rendering (uses s/w rendering instead if --no-opengl as well)" << endl;
#endif
		cout << "\t--no-vulkan: disables vulkan rendering (uses s/w rendering instead)" << endl;
		cout << "\t--benchmark: runs the simulation in benchmark mode, without rendering" << endl;
		cout << "\t--type <type>: sets the initial nbody setup (default: on-sphere)" << endl;
		cout << "\t--render-size <work-items>: sets the amount of work-items/work-group when using s/w rendering" << endl;
		for(const auto& desc : nbody_setup_desc) {
			cout << "\t\t" << desc << endl;
		}
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
		cout << "expected performace (with --benchmark):" << endl;
		cout << "\tRTX 2080 Ti:  ~10970 gflops (--count 278528 --tile-size 256)" << endl;
		cout << "\tP6000:        ~8400 gflops (--count 262144 --tile-size 512)" << endl;
		cout << "\tGP100:        ~7600 gflops (--count 262144 --tile-size 512)" << endl;
		cout << "\tGTX 970:      ~2770 gflops (--count 131072 --tile-size 256)" << endl;
		cout << "\tGTX 780:      ~2350 gflops (--count 131072 --tile-size 512)" << endl;
		cout << "\tGTX 1050 Ti:  ~1675 gflops (--count 262144 --tile-size 256)" << endl;
		cout << "\tR9 285:       ~1500 gflops (--count 131072 --tile-size 1024)" << endl;
		cout << "\tGTX 750:      ~840 gflops (--count 65536 --tile-size 256)" << endl;
		cout << "\ti9-7980XE:    ~825 gflops (--count 73728 --tile-size 64)" << endl;
		cout << "\tGT 650M:      ~385 gflops (--count 65536 --tile-size 512)" << endl;
		cout << "\tHD 530:       ~242 gflops (--count 65536 --tile-size 128)" << endl;
		cout << "\tHD 4600:      ~235 gflops (--count 65536 --tile-size 80)" << endl;
		cout << "\ti7-6700:      ~195 gflops (--count 32768 --tile-size 1024)" << endl;
		cout << "\tHD 4000:      ~165 gflops (--count 32768 --tile-size 128)" << endl;
		cout << "\tiPhone A10:   ~131 gflops (--count 32768 --tile-size 512)" << endl;
		cout << "\ti7-5820K:     ~105 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\ti7-4770:      ~80 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\ti7-3615QM:    ~38 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\ti7-950:       ~29 gflops (--count 32768 --tile-size 4)" << endl;
		cout << "\tiPhone A8:    ~28 gflops (--count 16384 --tile-size 512)" << endl;
		cout << "\tiPad A7:      ~20 gflops (--count 16384 --tile-size 512)" << endl;
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
	{ "--no-opengl", [](nbody_option_context&, char**&) {
		nbody_state.no_opengl = true;
		cout << "opengl disabled" << endl;
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
		nbody_state.no_opengl = true; // also disable opengl
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
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](nbody_option_context&, char**&) {} },
};

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		nbody_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_q:
				nbody_state.done = true;
				break;
			case SDLK_SPACE:
				nbody_state.stop ^= true;
				break;
			case SDLK_t:
				nbody_state.alpha_mask ^= true;
				break;
			case SDLK_r:
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
			default: break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::KEY_DOWN) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_w:
				nbody_state.distance = const_math::clamp(nbody_state.distance - 5.0f, 1.0f, nbody_state.max_distance);
				break;
			case SDLK_s:
				nbody_state.distance = const_math::clamp(nbody_state.distance + 5.0f, 1.0f, nbody_state.max_distance);
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
		nbody_state.enable_cam_rotate = true;
		return true;
	}
	else if(type == EVENT_TYPE::MOUSE_LEFT_UP
#if defined(FLOOR_IOS)
			|| type == EVENT_TYPE::FINGER_UP
#endif
			) {
		nbody_state.enable_cam_rotate = false;
		return true;
	}
	else if(type == EVENT_TYPE::MOUSE_RIGHT_DOWN) {
		nbody_state.enable_cam_move = true;
		return true;
	}
	else if(type == EVENT_TYPE::MOUSE_RIGHT_UP) {
		nbody_state.enable_cam_move = false;
		return true;
	}
	else if(type == EVENT_TYPE::MOUSE_MOVE
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
			nbody_state.cam_rotation *= (quaternionf::rotation_deg(delta.x, float3 { 0.0f, 1.0f, 0.0f }) *
										 quaternionf::rotation_deg(delta.y, float3 { 1.0f, 0.0f, 0.0f }));
		}
		else {
			// multiply by desired move speed
			static constexpr const float move_speed { 250.0f };
			delta *= move_speed;
			nbody_state.distance = const_math::clamp(nbody_state.distance + delta.y, 1.0f, nbody_state.max_distance);
		}
		return true;
	}
	return false;
}

void init_system() {
	// finish up old execution before we start with anything new
	dev_queue->finish();
	
	auto positions = (float4*)position_buffers[0]->map(dev_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
	auto velocities = (float3*)velocity_buffer->map(dev_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
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
	position_buffers[0]->unmap(dev_queue, positions);
	velocity_buffer->unmap(dev_queue, velocities);
	
	// reset everything
	buffer_flip_flop = 0;
	iteration = 0;
	sim_time_sum = 0.0L;
}

int main(int, char* argv[]) {
	// handle options
	nbody_option_context option_ctx;
	nbody_opt_handler::parse_options(argv + 1, option_ctx);
	if(nbody_state.done) return 0;
	
	// use less bodies on iOS
#if defined(FLOOR_IOS)
	nbody_state.body_count = 8192;
#endif
	
	// disable renderers that aren't available
#if defined(FLOOR_NO_METAL)
	nbody_state.no_metal = true;
#endif
#if defined(FLOOR_NO_VULKAN)
	nbody_state.no_vulkan = true;
#endif
	
	// init floor
	if(!floor::init(floor::init_state {
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
					 // if neither opengl or vulkan is disabled, use the default
					 // (this will choose vulkan if the config compute backend is vulkan)
					 (!nbody_state.no_opengl && !nbody_state.no_vulkan) ? floor::RENDERER::DEFAULT :
					 // else: choose a specific one
					 !nbody_state.no_vulkan ? floor::RENDERER::VULKAN :
					 !nbody_state.no_metal ? floor::RENDERER::METAL :
					 !nbody_state.no_opengl ? floor::RENDERER::OPENGL :
					 // opengl/vulkan/metal are disabled
					 floor::RENDERER::NONE),
	})) {
		return -1;
	}
	
	// disable resp. other renderers when using opengl/metal/vulkan
	const bool is_metal = (floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::METAL);
	const bool is_vulkan = (floor::get_compute_context()->get_compute_type() == COMPUTE_TYPE::VULKAN);
	const auto floor_renderer = floor::get_renderer();
	
	if(is_metal) {
		nbody_state.no_opengl = true;
		nbody_state.no_vulkan = true;
	}
	else {
		nbody_state.no_metal = true;
	}
	
	if(is_vulkan) {
		if(floor_renderer == floor::RENDERER::VULKAN) {
			nbody_state.no_opengl = true;
			nbody_state.no_metal = true;
		}
		// can't use OpenGL with Vulkan
		else {
			nbody_state.no_opengl = true;
		}
	}
	else {
		nbody_state.no_vulkan = true;
	}
	
	log_debug("using %s",
			  (!nbody_state.no_opengl ? "opengl renderer" :
			   !nbody_state.no_metal ? "metal renderer" :
			   !nbody_state.no_vulkan ? "vulkan renderer" : "s/w rasterizer"));
	
	// floor context handling
	floor::acquire_context();
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
												   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN,
												   EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE,
												   EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
												   EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE);
	
	// get the compute context that has been automatically created (opencl/cuda/metal/vulkan/host)
	auto compute_ctx = floor::get_compute_context();
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	auto fastest_device = compute_ctx->get_device(compute_device::TYPE::FASTEST);
	dev_queue = compute_ctx->create_queue(fastest_device);
	
	// parameter sanity check
	if(nbody_state.tile_size > fastest_device->max_total_local_size) {
		nbody_state.tile_size = (uint32_t)fastest_device->max_total_local_size;
		log_error("tile size too large, > max possible work-group size! - setting tile size to %u now", nbody_state.tile_size);
	}
	if((nbody_state.body_count % nbody_state.tile_size) != 0u) {
		nbody_state.body_count = ((nbody_state.body_count / nbody_state.tile_size) + 1u) * nbody_state.tile_size;
		log_error("body count not a multiple of tile size! - setting body count to %u now", nbody_state.body_count);
	}
	if(compute_ctx->get_compute_type() == COMPUTE_TYPE::HOST &&
	   nbody_state.tile_size != NBODY_TILE_SIZE) {
		log_error("compiled NBODY_TILE_SIZE (%u) must match run-time tile-size when using host compute! - using it now",
				  NBODY_TILE_SIZE);
		nbody_state.tile_size = NBODY_TILE_SIZE;
	}
	
	// init gl renderer
#if !defined(FLOOR_IOS)
	if(!nbody_state.no_opengl) {
		// setup renderer
		if(!gl_renderer::init()) {
			log_error("error during opengl initialization!");
			return -1;
		}
	}
#endif
	
	// compile the program and get the kernel functions
#if !defined(FLOOR_IOS)
	llvm_toolchain::compile_options options {
		.cli = ("-I" + floor::data_path("../nbody/src") +
				" -DNBODY_TILE_SIZE=" + to_string(nbody_state.tile_size) +
				" -DNBODY_SOFTENING=" + to_string(nbody_state.softening) + "f" +
				" -DNBODY_DAMPING=" + to_string(nbody_state.damping) + "f"),
		// override max registers that can be used, this is beneficial here as it yields about +10% of performance
		.cuda.max_registers = 36,
	};
	auto nbody_prog = compute_ctx->add_program_file(floor::data_path("../nbody/src/nbody.cpp"), options);
#else
	auto nbody_prog = compute_ctx->add_universal_binary(floor::data_path("nbody.fubar"));
#endif
	if(nbody_prog == nullptr) {
		log_error("program compilation failed");
		return -1;
	}
	auto nbody_compute = nbody_prog->get_kernel("nbody_compute");
	auto nbody_raster = nbody_prog->get_kernel("nbody_raster");
	if(nbody_compute == nullptr || nbody_raster == nullptr) {
		log_error("failed to retrieve kernel(s) from program");
		return -1;
	}
	
	// init metal/vulkan renderers (need compiled prog first)
#if defined(__APPLE__)
	if(!nbody_state.no_metal && nbody_state.no_opengl && nbody_state.no_vulkan) {
		// setup renderer
		if(!metal_renderer::init(fastest_device,
								 nbody_prog->get_kernel("lighting_vertex"),
								 nbody_prog->get_kernel("lighting_fragment"))) {
			log_error("error during metal initialization!");
			return -1;
		}
	}
#endif
#if !defined(FLOOR_NO_VULKAN)
	if(!nbody_state.no_vulkan && nbody_state.no_opengl && nbody_state.no_metal) {
		// setup renderer
		if(!vulkan_renderer::init(compute_ctx,
								  fastest_device,
								  nbody_prog->get_kernel("lighting_vertex"),
								  nbody_prog->get_kernel("lighting_fragment"))) {
			log_error("error during vulkan initialization!");
			return -1;
		}
	}
#endif
	
	// create nbody position and velocity buffers
	for(size_t i = 0; i < pos_buffer_count; ++i) {
		position_buffers[i] = compute_ctx->create_buffer(fastest_device, sizeof(float4) * nbody_state.body_count, (
			// will be reading and writing from the kernel
			COMPUTE_MEMORY_FLAG::READ_WRITE
			// host will only write data
			| COMPUTE_MEMORY_FLAG::HOST_WRITE
			// will be using the buffer with opengl
			| (!nbody_state.no_opengl ? COMPUTE_MEMORY_FLAG::OPENGL_SHARING : COMPUTE_MEMORY_FLAG::NONE)
			// automatic defaults when using OPENGL_SHARING:
			// OPENGL_READ_WRITE: again, will be reading and writing in the kernel
		), (!nbody_state.no_opengl ? GL_ARRAY_BUFFER : 0));
	}
	velocity_buffer = compute_ctx->create_buffer(fastest_device, sizeof(float3) * nbody_state.body_count,
												 COMPUTE_MEMORY_FLAG::READ_WRITE | COMPUTE_MEMORY_FLAG::HOST_WRITE);
	
	// image buffers (for s/w rendering only)
	array<shared_ptr<compute_buffer>, 2> img_buffers;
	size_t img_buffer_flip_flop { 0 };
	const uint2 img_size { floor::get_physical_screen_size() };
	if(nbody_state.no_opengl && nbody_state.no_metal && !nbody_state.benchmark) {
		img_buffers = {{
			compute_ctx->create_buffer(fastest_device, sizeof(uint32_t) * img_size.x * img_size.y,
									   COMPUTE_MEMORY_FLAG::READ_WRITE | COMPUTE_MEMORY_FLAG::HOST_READ),
			compute_ctx->create_buffer(fastest_device, sizeof(uint32_t) * img_size.x * img_size.y,
									   COMPUTE_MEMORY_FLAG::READ_WRITE | COMPUTE_MEMORY_FLAG::HOST_READ),
		}};
		img_buffers[0]->zero(dev_queue);
		img_buffers[1]->zero(dev_queue);
	}
	
	// init nbody system
	init_system();
	
	// init done, release context
	floor::release_context();
	
	// keeps track of the time between two simulation steps -> time delta in kernel
	auto time_keeper = chrono::high_resolution_clock::now();
	// is there a price for the most nested namespace/typedef?
	static const double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
	// main loop
	while(!nbody_state.done) {
		floor::get_event()->handle_events();
		
		// when benchmarking, we need to make sure computation is actually finished (all 100 iterations were computed)
		if(nbody_state.benchmark && iteration == 99) {
			dev_queue->finish();
		}
		
		// time keeping
		auto now = chrono::high_resolution_clock::now();
		auto delta = now - time_keeper;
		time_keeper = now;
		
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
		if(!nbody_state.stop) {
			//log_debug("delta: %fms /// %f gflops", 1000.0f * float(((double)delta.count()) / time_den),
			//		  compute_gflops(1000.0 * (((double)delta.count()) / time_den), false));
			
			// in ms
			sim_time_sum += ((double)delta.count()) / (time_den / 1000.0);
			
			if(iteration == 99) {
				log_debug("avg of 100 iterations: %fms ### %s gflops",
						  sim_time_sum / 100.0, compute_gflops(sim_time_sum / 100.0, false));
				floor::set_caption("nbody / " + to_string(nbody_state.body_count) + " bodies / " +
								   to_string(compute_gflops(sim_time_sum / 100.0, false)) + " gflops");
				iteration = 0;
				sim_time_sum = 0.0L;
				
				// benchmark is done after 100 iterations
				if(nbody_state.benchmark) {
					nbody_state.done = true;
				}
			}
			else {
				++iteration;
			}
			
			dev_queue->execute(nbody_compute,
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
		}
		
		// there is no proper dependency tracking yet, so always need to manually finish right now
		if(is_vulkan || is_metal) dev_queue->finish();
		
		// s/w rendering
		if(nbody_state.no_opengl && nbody_state.no_metal && nbody_state.no_vulkan && !nbody_state.benchmark) {
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
			dev_queue->execute(nbody_raster,
							   // total amount of work:
							   uint1 { img_size.x * img_size.y },
							   // work per work-group:
							   uint1 {
								   nbody_state.render_size == 0 ?
								   nbody_raster->get_kernel_entry(fastest_device)->max_total_local_size : nbody_state.render_size
							   },
							   // kernel arguments:
							   /* in_positions: */		position_buffers[buffer_flip_flop],
							   /* img: */				img_buffers[img_buffer_flip_flop],
							   /* img_old: */			img_buffers[1 - img_buffer_flip_flop],
							   /* raster_params: */		raster_params);
			
			if(is_vulkan || is_metal) dev_queue->finish();
			
			// grab the current image buffer data (read-only + blocking) ...
			auto img_data = (uchar4*)img_buffers[img_buffer_flip_flop]->map(dev_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
			
			// ... and blit it into the window
			const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
			SDL_LockSurface(wnd_surface);
			const uint2 surface_dim = { uint32_t(wnd_surface->w), uint32_t(wnd_surface->h) }; // TODO: figure out how to coerce sdl to create a 2x surface
			const uint2 render_dim = img_size.minned(floor::get_physical_screen_size());
			const uint2 scale = render_dim / surface_dim;
			for(uint32_t y = 0; y < surface_dim.y; ++y) {
				uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels + ((size_t)wnd_surface->pitch / sizeof(uint32_t)) * y;
				uint32_t img_idx = img_size.x * y * scale.y;
				for(uint32_t x = 0; x < surface_dim.x; ++x, img_idx += scale.x) {
					*px_ptr++ = SDL_MapRGB(wnd_surface->format, img_data[img_idx].x, img_data[img_idx].y, img_data[img_idx].z);
				}
			}
			img_buffers[img_buffer_flip_flop]->unmap(dev_queue, img_data);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
		// opengl/metal/vulkan rendering
		else if(!nbody_state.no_opengl || !nbody_state.no_metal || !nbody_state.no_vulkan) {
			floor::start_frame();
			if(!nbody_state.no_opengl) {
#if !defined(FLOOR_IOS)
				gl_renderer::render(dev_queue, position_buffers[cur_buffer]);
#endif
			}
#if defined(__APPLE__)
			else if(!nbody_state.no_metal) {
				metal_renderer::render(dev_queue, position_buffers[cur_buffer]);
			}
#endif
#if !defined(FLOOR_NO_VULKAN)
			else if(!nbody_state.no_vulkan) {
				vulkan_renderer::render(compute_ctx, fastest_device, dev_queue, position_buffers[cur_buffer]);
			}
#endif
			floor::end_frame();
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	// cleanup
	floor::acquire_context();
	for(size_t i = 0; i < pos_buffer_count; ++i) {
		position_buffers[i] = nullptr;
	}
	velocity_buffer = nullptr;
	for(auto img_buffer : img_buffers) {
		img_buffer = nullptr;
	}
#if !defined(FLOOR_NO_VULKAN)
	if(!nbody_state.no_vulkan) {
		vulkan_renderer::destroy(compute_ctx, fastest_device);
	}
#endif
	floor::release_context();
	
	// kthxbye
	floor::destroy();
	return 0;
}
