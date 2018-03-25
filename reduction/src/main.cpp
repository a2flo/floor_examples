/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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
#include "reduction_state.hpp"
reduction_state_struct reduction_state;

struct reduction_option_context {
	// unused
	string additional_options { "" };
};
typedef option_handler<reduction_option_context> reduction_opt_handler;

//
static shared_ptr<compute_context> compute_ctx;
// device compute/command queue
static shared_ptr<compute_queue> dev_queue;
//
static shared_ptr<compute_device> fastest_device;
//
static shared_ptr<compute_program> reduction_prog;
// POT sizes from 32 - 1024
#define REDUCTION_TILE_SIZE(F) \
F(32) \
F(64) \
F(128) \
F(256) \
F(512) \
F(1024)
//
#define REDUCTION_KERNEL_ENUMS(tile_size) \
	REDUCTION_ADD_F32_##tile_size, \
	REDUCTION_ADD_U32_##tile_size, \
	REDUCTION_ADD_F64_##tile_size, \
	REDUCTION_ADD_U64_##tile_size,
//
#define REDUCTION_KERNEL_NAMES(tile_size) \
	"reduce_add_f32_" #tile_size, \
	"reduce_add_u32_" #tile_size, \
	"reduce_add_f64_" #tile_size, \
	"reduce_add_u64_" #tile_size,
//
enum REDUCTION_KERNEL : uint32_t {
	REDUCTION_TILE_SIZE(REDUCTION_KERNEL_ENUMS)
	__MAX_REDUCTION_KERNEL
};
//
static const array<const char*, __MAX_REDUCTION_KERNEL> reduction_kernel_names {{
	REDUCTION_TILE_SIZE(REDUCTION_KERNEL_NAMES)
}};
//
static array<shared_ptr<compute_kernel>, __MAX_REDUCTION_KERNEL> reduction_kernels;
//
static array<uint1, __MAX_REDUCTION_KERNEL> reduction_max_local_sizes;
// flag each kernel as usable/unusable (based on local size and 64-bit capabilities)
static array<bool, __MAX_REDUCTION_KERNEL> reduction_kernel_usable;
//
#define REDUCTION_KERNEL_SIZES_ARRAY(tile_size) \
	tile_size, tile_size, tile_size, tile_size, /* 4 kernels each */
static const array<uint32_t, __MAX_REDUCTION_KERNEL> reduction_kernel_sizes {{
	REDUCTION_TILE_SIZE(REDUCTION_KERNEL_SIZES_ARRAY)
}};

//! option -> function map
template<> vector<pair<string, reduction_opt_handler::option_function>> reduction_opt_handler::options {
	{ "--help", [](reduction_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--size <count>: TODO" << endl;
		cout << "\t--benchmark: runs the reduction in benchmark mode" << endl;
		reduction_state.done = true;
	}},
	{ "--size", [](reduction_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --count!" << endl;
			reduction_state.done = true;
			return;
		}
		// TODO
		//reduction_state. = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		//cout << "size set to: " << reduction_state. << endl;
	}},
	{ "--benchmark", [](reduction_option_context&, char**&) {
		reduction_state.benchmark = true;
		cout << "benchmark mode enabled" << endl;
	}},
	// ignore xcode debug arg
	{ "-NSDocumentRevisionsDebugMode", [](reduction_option_context&, char**&) {} },
};

static bool compile_kernels() {
#if !defined(FLOOR_IOS)
	llvm_toolchain::compile_options options {
		.cli = ("-I" + floor::data_path("../reduction/src")),
		.cuda.ptx_version = 60
	};
	auto new_reduction_prog = compute_ctx->add_program_file(floor::data_path("../reduction/src/reduction.cpp"), options);
#else
	// TODO
#endif
	if(new_reduction_prog == nullptr) {
		log_error("program compilation failed");
		return false;
	}
	
	decltype(reduction_kernels) new_reduction_kernels;
	decltype(reduction_max_local_sizes) new_reduction_max_local_sizes;
	decltype(reduction_kernel_usable) new_reduction_kernel_usable;
	for(uint32_t i = 0; i < REDUCTION_KERNEL::__MAX_REDUCTION_KERNEL; ++i) {
		new_reduction_kernels[i] = new_reduction_prog->get_kernel(reduction_kernel_names[i]);
		if(new_reduction_kernels[i] == nullptr) {
			log_error("failed to retrieve kernel %s from program", reduction_kernel_names[i]);
			return false;
		}
		new_reduction_max_local_sizes[i] = new_reduction_kernels[i]->get_kernel_entry(fastest_device)->max_total_local_size;
		if(fastest_device->context->get_compute_type() != COMPUTE_TYPE::HOST) {
			// usable if max local size >= required tile size, and if #args != 0 (i.e. kernel isn't disabled for other reasons)
			new_reduction_kernel_usable[i] = ((reduction_kernel_sizes[i] <= new_reduction_max_local_sizes[i].x) &&
											  !new_reduction_kernels[i]->get_kernel_entry(fastest_device)->info->args.empty());
		}
		else {
			// always usable with host-compute
			new_reduction_kernel_usable[i] = true;
		}
		log_debug("%s: local size: %u, usable: %u", reduction_kernel_names[i], new_reduction_max_local_sizes[i],
				  new_reduction_kernel_usable[i]);
	}
	
	// everything was successful, exchange objects
	reduction_prog = new_reduction_prog;
	reduction_kernels = new_reduction_kernels;
	reduction_max_local_sizes = new_reduction_max_local_sizes;
	reduction_kernel_usable = new_reduction_kernel_usable;
	return true;
}

static bool evt_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::QUIT) {
		reduction_state.done = true;
		return true;
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			case SDLK_ESCAPE:
			case SDLK_q:
				reduction_state.done = true;
				break;
			case SDLK_c:
				compile_kernels();
				break;
			default: break;
		}
		return true;
	}
	else if(type == EVENT_TYPE::KEY_DOWN) {
		switch(((shared_ptr<key_up_event>&)obj)->key) {
			default: break;
		}
		return true;
	}
	return false;
}

int main(int, char* argv[]) {
	// handle options
	reduction_option_context option_ctx;
	reduction_opt_handler::parse_options(argv + 1, option_ctx);
	if(reduction_state.done) return 0;
	
	// init floor
	if(!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = "../../data/",
#else
		.data_path = "data/",
#endif
		.app_name = "reduction",
		.console_only = true,
		.renderer = floor::RENDERER::NONE, // opengl/vulkan/metal are disabled
	})) {
		return -1;
	}
	
	// floor context handling
	floor::acquire_context();
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
												   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN);
	
	// get the compute context that has been automatically created (opencl/cuda/metal/vulkan/host)
	compute_ctx = floor::get_compute_context();
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	fastest_device = compute_ctx->get_device(compute_device::TYPE::FASTEST);
	dev_queue = compute_ctx->create_queue(fastest_device);
	
	// parameter sanity check (TODO)
	
	// compile the program and get the kernel functions
	if(!compile_kernels()) return -1;
	
	// reduction buffers
	static constexpr const uint32_t reduction_elem_count { 1024 * 1024 * 256 }; // == 1024 MiB (32-bit), 2048 MiB (64-bit)
	//auto red_data_sum = compute_ctx->create_buffer(fastest_device, sizeof(uint64_t) /* 64-bit */);
	//auto red_data = compute_ctx->create_buffer(fastest_device, reduction_elem_count * sizeof(uint64_t) /* 64-bit */);
	auto red_data_sum = compute_ctx->create_buffer(fastest_device, sizeof(uint32_t) /* 32-bit */);
	auto red_data = compute_ctx->create_buffer(fastest_device, reduction_elem_count * sizeof(uint32_t) /* 32-bit */);
	
	// compute the ideal global size (#units * local size), if the unit count is unknown, assume 16, so that we still get good throughput
	static constexpr const uint32_t unit_count_fallback { 16u };
	if (fastest_device->units == 0) {
		log_error("device #units is 0 - assuming %u", unit_count_fallback);
	}
	const uint32_t dev_unit_count = (fastest_device->units != 0 ? fastest_device->units : unit_count_fallback);
	const uint32_t reduction_global_size = dev_unit_count * 1024u * (fastest_device->is_gpu() ? 2u : 1u);
	
	// init done, release context
	floor::release_context();
	
	//
	random_device rd;
	mt19937 gen(rd());
	uniform_real_distribution<float> f32_dist { 0.0f, 0.025f };
	uniform_real_distribution<double> f64_dist { 0.0, 0.05 };
	uniform_int_distribution<uint32_t> u32_dist { 0, 16 };
	uniform_int_distribution<uint64_t> u64_dist { 0, 16 };
	// main loop
	uint32_t iteration = 0;
	while(!reduction_state.done) {
		floor::get_event()->handle_events();
		
		// init data
		// for expected sum on the CPU use: https://en.wikipedia.org/wiki/Kahan_summation_algorithm
		auto mrdata = (float*)red_data->map(dev_queue);
		auto rdata = mrdata;
		double expected_sum = 0.0, kahan_c = 0.0;
		long double expected_sum_ld = 0.0L;
		float expected_sum_f = 0.0f;
#pragma clang loop vectorize(enable)
		for(uint32_t i = 0; i < reduction_elem_count; ++i) {
			const auto rnd_val = f32_dist(gen);
			const auto kahan_y = double(rnd_val) - kahan_c;
			const auto kahan_t = expected_sum + kahan_y;
			kahan_c = (kahan_t - expected_sum) - kahan_y;
			expected_sum = kahan_t;
			expected_sum_ld += (long double)rnd_val;
			expected_sum_f += rnd_val;
			*rdata++ = rnd_val;
		}
		red_data->unmap(dev_queue, mrdata);
		
		//
		red_data_sum->zero(dev_queue);
		dev_queue->finish();
		dev_queue->start_profiling();
		if(fastest_device->cooperative_kernel_support) {
			// TODO: query max concurrent threads
			dev_queue->execute_cooperative(reduction_kernels[REDUCTION_ADD_F32_1024],
										   uint1 { 2048 /* max concurrent threads */ * fastest_device->units /* #multiprocessors */ },
										   uint1 { 1024 },
										   red_data, red_data_sum, reduction_elem_count);
		}
		else {
			dev_queue->execute(reduction_kernels[REDUCTION_ADD_F32_1024],
							   uint1 { reduction_global_size },
							   uint1 { 1024 },
							   red_data, red_data_sum, reduction_elem_count);
		}
		const auto prof_time = dev_queue->stop_profiling();
		dev_queue->finish();
		
		const auto compute_bandwidth = [](const uint64_t& microseconds) {
			static constexpr const size_t elem_size { sizeof(float) };
			const auto red_size = elem_size * reduction_elem_count;
			const auto size_per_second = (double(red_size) / 1000000000.0) / (double(microseconds) / 1000000.0);
			return size_per_second;
		};
		log_debug("reduction computed in %fms -> %u GB/s", double(prof_time) / 1000.0, compute_bandwidth(prof_time));
		
		float sum = 0.0f;
		red_data_sum->read_to(sum, dev_queue);
		const auto abs_diff = abs(double(sum) - expected_sum);
		log_debug("sum: %f, expected: kahan: %f, linear: %f, %f -> diff: %f (sp: %f)",
				  sum, expected_sum, expected_sum_ld, expected_sum_f, abs_diff, abs(sum - expected_sum_f));
		
		// next
		++iteration;
		
		// when benchmarking, we need to make sure computation is actually finished (all 20 iterations were computed)
		if(reduction_state.benchmark && iteration == 20) {
			dev_queue->finish();
			break;
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	// cleanup
	floor::acquire_context();
	red_data = nullptr;
	red_data_sum = nullptr;
	floor::release_context();
	
	// kthxbye
	floor::destroy();
	return 0;
}
