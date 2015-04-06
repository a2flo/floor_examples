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
#include <floor/ios/ios_helper.hpp>
#include <floor/core/option_handler.hpp>
#include "nbody.hpp"
#include "gl_renderer.hpp"
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
	__MAX_NBODY_SETUP
};
static NBODY_SETUP nbody_setup { NBODY_SETUP::ON_SPHERE };
static constexpr const array<const char*, (size_t)NBODY_SETUP::__MAX_NBODY_SETUP> nbody_setup_desc {{
	"pseudo-galaxy: spawns bodies in a round pseudo galaxy with body height ~= log(total-radius - body-distance + 1)",
	"cube: spawns bodies randomly inside a cube (uniform distribution)",
	"on-sphere: spawns bodies randomly on a sphere (uniform distribution)",
	"in-sphere: spawns bodies randomly inside a sphere (uniform distribution)",
	"ring: spawns bodies in a ring (uniform distribution)"
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
static long double sim_time_sum { 0.0L };
// initializes (or resets) the current nbody system
static void init_system();

//! option -> function map
template<> unordered_map<string, nbody_opt_handler::option_function> nbody_opt_handler::options {
	{ "--help", [](nbody_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--count <count>: specify the amount of bodies (default: " << nbody_state.body_count << ")" << endl;
		cout << "\t--tile-size <count>: sets the tile size / work-group size, thus also the amount of used local memory and unrolling (default: " << nbody_state.tile_size << ")" << endl;
		cout << "\t--time-step <step-size>: sets the time step size (default: " << nbody_state.time_step << ")" << endl;
		cout << "\t--mass <min> <max>: sets the random mass interval (default: " << nbody_state.mass_minmax << ")" << endl;
		cout << "\t--softening <softening>: sets the simulation softening (default: " << nbody_state.softening << ")" << endl;
		cout << "\t--damping <damping>: sets the simulation damping (default: " << nbody_state.damping << ")" << endl;
		cout << "\t--no-opengl: disables opengl rendering (uses s/w rendering instead)" << endl;
		cout << "\t--no-fma: disables use of explicit fma instructions or s/w emulation thereof (use this with non-fma cpus)" << endl;
		cout << "\t--benchmark: runs the simulation in benchmark mode, without rendering" << endl;
		cout << "\t--type <type>: sets the initial nbody setup (default: on-sphere)" << endl;
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
		cout << "\tGTX 970:      ~2600 gflops (--count 131072 --tile-size 256)" << endl;
		cout << "\tGTX 780:      ~2100 gflops (--count 131072 --tile-size 512)" << endl;
		cout << "\tGT 650M:      ~340 gflops (--count 65536 --tile-size 512)" << endl;
		cout << "\ti7-5820K:     ~105 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\ti7-4770:      ~76 gflops (--count 32768 --tile-size 8)" << endl;
		cout << "\ti7-3615QM:    ~38 gflops (--count 32768 --tile-size 8 --no-fma)" << endl;
		cout << "\ti7-950:       ~29 gflops (--count 32768 --tile-size 4 --no-fma)" << endl;
		cout << "\tiPad A7:      ~18 gflops (--count 8192 --tile-size 512)" << endl;
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
		nbody_state.mass_minmax.x = strtof(*arg_ptr, nullptr);
		
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after second --mass parameter!" << endl;
			nbody_state.done = true;
			return;
		}
		nbody_state.mass_minmax.y = strtof(*arg_ptr, nullptr);
		cout << "mass set to: " << nbody_state.mass_minmax << endl;
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
	{ "--no-fma", [](nbody_option_context&, char**&) {
		nbody_state.no_fma = true;
		cout << "fma disabled" << endl;
	}},
	{ "--benchmark", [](nbody_option_context&, char**&) {
		nbody_state.no_opengl = true; // also disable opengl
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
			
			// compute rotation around x and y axis
			quaternionf q_x, q_y;
			q_x.set_rotation(delta.x, float3 { 0.0f, 1.0f, 0.0f });
			q_y.set_rotation(delta.y, float3 { 1.0f, 0.0f, 0.0f });
			q_x *= q_y;
			
			// multiply existing rotation by newly computed one
			nbody_state.cam_rotation *= q_x;
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
	auto positions = (float4*)position_buffers[0]->map(dev_queue);
	auto velocities = (float3*)velocity_buffer->map(dev_queue);
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
				positions[i].y = core::rand(height_dev) * (ln_height * 0.5f) * (core::rand(0, 16) % 2 == 0 ? -1.0f : 1.0f);
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
				const auto rnd_rad = sphere_scale * sqrt(1.0f - rad * rad) * (rad < 0.0 ? -1.0f : 1.0f);
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
			default: floor_unreachable();
		}
		
		positions[i].w = core::rand(nbody_state.mass_minmax.x, nbody_state.mass_minmax.y);
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
	
	// init floor
#if !defined(FLOOR_IOS)
	floor::init(argv[0], (const char*)"../../data/", // call path, data path
				nbody_state.benchmark, "config.xml", // console-mode, config name
				nbody_state.benchmark ^ true); // use opengl 3.2+ core
#else
	floor::init(argv[0], (const char*)"data/");
	nbody_state.no_opengl = true;
	nbody_state.time_step = 0.02f;
	nbody_state.body_count = 4096;
#endif
	floor::set_caption("nbody");
	
	// opengl and floor context handling
	if(nbody_state.no_opengl) floor::set_use_gl_context(false);
	floor::acquire_context();
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
												   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN,
												   EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP, EVENT_TYPE::MOUSE_MOVE,
												   EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP,
												   EVENT_TYPE::FINGER_DOWN, EVENT_TYPE::FINGER_UP, EVENT_TYPE::FINGER_MOVE);
	
#if !defined(FLOOR_IOS)
	if(!nbody_state.no_opengl) {
		// setup renderer
		if(!gl_renderer::init()) {
			log_error("error during opengl initialization!");
			return -1;
		}
	}
#endif
	
	// get the compute context that has been automatically created (opencl/cuda/metal/host)
	auto compute_ctx = floor::get_compute_context();
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	auto fastest_device = compute_ctx->get_device(compute_device::TYPE::FASTEST);
	dev_queue = compute_ctx->create_queue(fastest_device);
	
	// parameter sanity check
	if(nbody_state.tile_size > fastest_device->max_work_group_size) {
		nbody_state.tile_size = (uint32_t)fastest_device->max_work_group_size;
		log_error("tile size too large, > max possible work-group size! - setting tile size to %u now", nbody_state.tile_size);
	}
	if((nbody_state.body_count % nbody_state.tile_size) != 0u) {
		nbody_state.body_count = ((nbody_state.body_count / nbody_state.tile_size) + 1u) * nbody_state.tile_size;
		log_error("body count not a multiple of tile size! - setting body count to %u now", nbody_state.body_count);
	}
	
	// compile the program and get the kernel functions
#if !defined(FLOOR_IOS)
	auto nbody_prog = compute_ctx->add_program_file(floor::data_path("../nbody/src/nbody.cpp"),
													"-I" + floor::data_path("../nbody/src") +
													" -DNBODY_TILE_SIZE=" + to_string(nbody_state.tile_size) +
													" -DNBODY_SOFTENING=" + to_string(nbody_state.softening) + "f" +
													" -DNBODY_DAMPING=" + to_string(nbody_state.damping) + "f" +
													(nbody_state.no_fma ? " -DNBODY_NO_FMA" : ""));
#else
	// for now: use a precompiled metal lib instead of compiling at runtime
	const vector<llvm_compute::kernel_info> kernel_infos {
		{
			"nbody_compute",
			{ 16, 16, 12, 4 },
			{
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT
			}
		},
		{
			"nbody_raster",
			{ 16, 12, 12, 8, 4, 64 },
			{
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT,
				llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT
			}
		}
	};
	auto nbody_prog = compute_ctx->add_precompiled_program_file(floor::data_path("nbody.metallib"), kernel_infos);
#endif
	if(nbody_prog == nullptr) {
		log_error("program compilation failed");
		return -1;
	}
	auto nbody_compute = nbody_prog->get_kernel_fuzzy("nbody_compute");
	auto nbody_raster = nbody_prog->get_kernel_fuzzy("nbody_raster");
	if(nbody_compute == nullptr || nbody_raster == nullptr) {
		log_error("failed to retrieve kernel from program");
		return -1;
	}
	
	// create device buffers
	const COMPUTE_BUFFER_FLAG position_buffer_flags {
		// will be reading and writing from the kernel
		COMPUTE_BUFFER_FLAG::READ_WRITE
		// host will read and write data
		| COMPUTE_BUFFER_FLAG::HOST_READ_WRITE
		
		// will be using the buffer with opengl
		| (!nbody_state.no_opengl ? COMPUTE_BUFFER_FLAG::OPENGL_SHARING : COMPUTE_BUFFER_FLAG::NONE)
		// automatic defaults when using OPENGL_SHARING:
		// OPENGL_READ_WRITE: again, will be reading and writing in the kernel
		// OPENGL_ARRAY_BUFFER: this is a GL_ARRAY_BUFFER buffer
	};
	
	// create nbody position and velocity buffers
	for(size_t i = 0; i < pos_buffer_count; ++i) {
		position_buffers[i] = compute_ctx->create_buffer(fastest_device, sizeof(float4) * nbody_state.body_count, position_buffer_flags);
	}
	velocity_buffer = compute_ctx->create_buffer(fastest_device, sizeof(float3) * nbody_state.body_count);
	
	// image buffers (for s/w rendering only)
	array<shared_ptr<compute_buffer>, 2> img_buffers;
	size_t img_buffer_flip_flop { 0 };
	if(nbody_state.no_opengl && !nbody_state.benchmark) {
		img_buffers = {{
			compute_ctx->create_buffer(fastest_device, sizeof(float3) * floor::get_width() * floor::get_height(),
									   COMPUTE_BUFFER_FLAG::READ_WRITE | COMPUTE_BUFFER_FLAG::HOST_READ),
			compute_ctx->create_buffer(fastest_device, sizeof(float3) * floor::get_width() * floor::get_height(),
									   COMPUTE_BUFFER_FLAG::READ_WRITE | COMPUTE_BUFFER_FLAG::HOST_READ),
		}};
		img_buffers[0]->zero(dev_queue);
		img_buffers[1]->zero(dev_queue);
	}
	const uint2 img_size { floor::get_width(), floor::get_height() };
	
	// init nbody system
	init_system();
	
	// init done, release context
	floor::release_context();
	
	// keeps track of the time between two simulation steps -> time delta in kernel
	auto time_keeper = chrono::high_resolution_clock::now();
	// is there a price for the most nested namespace/typedef?
	static const long double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
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
		const auto compute_gflops = [](const long double& iter_time_in_ms, const bool use_fma) {
			if(!use_fma) {
				const size_t flops_per_body { 19 };
				const size_t flops_per_iter { size_t(nbody_state.body_count) * size_t(nbody_state.body_count) * flops_per_body };
				return ((1000.0L / iter_time_in_ms) * (long double)flops_per_iter) / 1'000'000'000.0L;
			}
			else {
				// NOTE: GPUs and recent CPUs support fma instructions, thus performing 2 floating point operations
				// in 1 cycle instead of 2 -> to account for that, compute some kind of "actual ops done" metric
				const size_t flops_per_body_fma { 13 };
				const size_t flops_per_iter_fma { size_t(nbody_state.body_count) * size_t(nbody_state.body_count) * flops_per_body_fma };
				return ((1000.0L / iter_time_in_ms) * (long double)flops_per_iter_fma) / 1'000'000'000.0L;
			}
		};
		
		// flip/flop buffer indices
		const size_t cur_buffer = buffer_flip_flop;
		const size_t next_buffer = (buffer_flip_flop + 1) % pos_buffer_count;
		if(!nbody_state.stop) {
			//log_debug("delta: %fms /// %f gflops", 1000.0f * float(((long double)delta.count()) / time_den),
			//		  compute_gflops(1000.0L * (((long double)delta.count()) / time_den), false));
			
			// in ms
			sim_time_sum += ((long double)delta.count()) / (time_den / 1000.0L);
			
			if(iteration == 99) {
				log_debug("avg of 100 iterations: %fms ### %s gflops",
						  sim_time_sum / 100.0L, compute_gflops(sim_time_sum / 100.0L, false));
				floor::set_caption("nbody / " + to_string(nbody_state.body_count) + " bodies / " +
								   to_string(compute_gflops(sim_time_sum / 100.0L, false)) + " gflops");
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
							   size1 { nbody_state.body_count },
							   // work per work-group:
							   size1 { nbody_state.tile_size },
							   // kernel arguments:
							   /* in_positions: */		position_buffers[cur_buffer],
							   /* out_positions: */		position_buffers[next_buffer],
							   /* velocities: */		velocity_buffer,
							   /* delta: */				/*float(((long double)delta.count()) / time_den)*/
							   							// NOTE: could use a time-step scaler instead, but fixed size seems more reasonable
							   							nbody_state.time_step);
			buffer_flip_flop = next_buffer;
		}
		
		// s/w rendering
		if(nbody_state.no_opengl && !nbody_state.benchmark) {
			const matrix4f mview { nbody_state.cam_rotation.to_matrix4() * matrix4f().translate(0.0f, 0.0f, -nbody_state.distance) };
			if(!nbody_state.stop) img_buffer_flip_flop = 1 - img_buffer_flip_flop;
			dev_queue->execute(nbody_raster,
							   // total amount of work:
							   size1 { img_size.x * img_size.y },
							   // work per work-group:
							   size1 { fastest_device->max_work_group_size },
							   // kernel arguments:
							   /* in_positions: */		position_buffers[buffer_flip_flop],
							   /* img: */				img_buffers[img_buffer_flip_flop],
							   /* img_old: */			img_buffers[1 - img_buffer_flip_flop],
							   /* img_size: */			img_size,
							   /* body_count: */		nbody_state.body_count,
							   /* mview: */				mview);
			
			// grab the current image buffer data (read-only + blocking) ...
			auto img_data = (float3*)img_buffers[img_buffer_flip_flop]->map(dev_queue, COMPUTE_BUFFER_MAP_FLAG::READ | COMPUTE_BUFFER_MAP_FLAG::BLOCK);
			
			// ... and blit it into the window
			const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
			SDL_LockSurface(wnd_surface);
			const auto pitch_offset = ((size_t)wnd_surface->pitch / sizeof(uint32_t)) - (size_t)img_size.x;
			uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels;
			for(uint32_t y = 0, img_idx = 0; y < img_size.y; ++y) {
				for(uint32_t x = 0; x < img_size.x; ++x, ++img_idx) {
					const auto rgb = img_data[img_idx] * 255.0f;
					*px_ptr++ = SDL_MapRGB(wnd_surface->format, uint8_t(rgb.x), uint8_t(rgb.y), uint8_t(rgb.z));
				}
				px_ptr += pitch_offset;
			}
			img_buffers[img_buffer_flip_flop]->unmap(dev_queue, img_data);
			
			SDL_UnlockSurface(wnd_surface);
			SDL_UpdateWindowSurface(floor::get_window());
		}
		// opengl rendering
		else if(!nbody_state.no_opengl) {
			floor::start_draw();
#if !defined(FLOOR_IOS)
			gl_renderer::render(dev_queue, position_buffers[cur_buffer]);
#endif
			floor::stop_draw();
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	if(!nbody_state.no_opengl) {
		// need to kill off the shared opengl buffers before floor kills the opengl context, otherwise bad things(tm) will happen
		for(size_t i = 0; i < pos_buffer_count; ++i) {
			position_buffers[i] = nullptr;
		}
	}
	
	// kthxbye
	floor::destroy();
	return 0;
}
