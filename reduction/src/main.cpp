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
#include <floor/core/option_handler.hpp>
#include <floor/compute/compute_kernel.hpp>
#include <floor/core/aligned_ptr.hpp>
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
static const compute_device* fastest_device { nullptr };
//
static shared_ptr<compute_program> reduction_prog;
// POT sizes from 32 - 1024
#define POT_TILE_SIZES(F) \
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
#define SCAN_KERNEL_ENUMS(tile_size) \
	INCL_SCAN_LOCAL_U32_##tile_size, \
	EXCL_SCAN_LOCAL_U32_##tile_size, \
	INCL_SCAN_GLOBAL_U32_##tile_size, \
	EXCL_SCAN_GLOBAL_U32_##tile_size,
//
#define REDUCTION_KERNEL_NAMES(tile_size) \
	"reduce_add_f32_" #tile_size, \
	"reduce_add_u32_" #tile_size, \
	"reduce_add_f64_" #tile_size, \
	"reduce_add_u64_" #tile_size,
#define SCAN_KERNEL_NAMES(tile_size) \
	"incl_scan_local_u32_" #tile_size, \
	"excl_scan_local_u32_" #tile_size, \
	"incl_scan_global_u32_" #tile_size, \
	"excl_scan_global_u32_" #tile_size,
//
enum ALGO_KERNEL_TYPE : uint32_t {
	POT_TILE_SIZES(REDUCTION_KERNEL_ENUMS)
	POT_TILE_SIZES(SCAN_KERNEL_ENUMS)
	__MAX_ALGO_KERNEL_TYPE
};
//
static const array<const char*, __MAX_ALGO_KERNEL_TYPE> algo_kernel_names {{
	POT_TILE_SIZES(REDUCTION_KERNEL_NAMES)
	POT_TILE_SIZES(SCAN_KERNEL_NAMES)
}};
//
static array<shared_ptr<compute_kernel>, __MAX_ALGO_KERNEL_TYPE> algo_kernels;
//
static array<uint1, __MAX_ALGO_KERNEL_TYPE> algo_max_local_sizes;
// flag each kernel as usable/unusable (based on local size and 64-bit capabilities)
static array<bool, __MAX_ALGO_KERNEL_TYPE> algo_kernel_usable;
//
#define REDUCTION_KERNEL_SIZES_ARRAY(tile_size) \
	tile_size, tile_size, tile_size, tile_size, /* 4 kernels each */
#define SCAN_KERNEL_SIZES_ARRAY(tile_size) \
	tile_size, tile_size, tile_size, tile_size, /* 4 kernels each */
static const array<uint32_t, __MAX_ALGO_KERNEL_TYPE> algo_kernel_sizes {{
	POT_TILE_SIZES(REDUCTION_KERNEL_SIZES_ARRAY)
	POT_TILE_SIZES(SCAN_KERNEL_SIZES_ARRAY)
}};

//! option -> function map
template<> vector<pair<string, reduction_opt_handler::option_function>> reduction_opt_handler::options {
	{ "--help", [](reduction_option_context&, char**&) {
		cout << "command line options:" << endl;
		cout << "\t--size <count>: TODO" << endl;
		cout << "\t--benchmark: runs in benchmark mode (fixed 20 iterations)" << endl;
		cout << "\t--iterations: when not running in benchmark mode, set the amount of iterations (default: 3)" << endl;
		cout << "\t--reduction: performs a float reduction (default)" << endl;
		cout << "\t--reduction-uint: performs a uint32_t reduction" << endl;
		cout << "\t--incl-scan: performs an inclusive scan" << endl;
		cout << "\t--excl-scan: performs an exclusive scan" << endl;
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
	{ "--iterations", [](reduction_option_context&, char**& arg_ptr) {
		++arg_ptr;
		if (*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --iterations!" << endl;
			reduction_state.done = true;
			return;
		}
		reduction_state.max_iterations = (uint32_t)strtoul(*arg_ptr, nullptr, 10);
		cout << "iterations set: " << reduction_state.max_iterations << endl;
	}},
	{ "--reduction", [](reduction_option_context&, char**&) {
		reduction_state.exec_mode = reduction_state_struct::EXEC_MODE::REDUCTION_F32;
		cout << "running reduction (float)" << endl;
	}},
	{ "--reduction-uint", [](reduction_option_context&, char**&) {
		reduction_state.exec_mode = reduction_state_struct::EXEC_MODE::REDUCTION_U32;
		cout << "running reduction (uint32_t)" << endl;
	}},
	{ "--incl-scan", [](reduction_option_context&, char**&) {
		reduction_state.exec_mode = reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN;
		cout << "running inclusive scan" << endl;
	}},
	{ "--excl-scan", [](reduction_option_context&, char**&) {
		reduction_state.exec_mode = reduction_state_struct::EXEC_MODE::EXCLUSIVE_SCAN;
		cout << "running exclusive scan" << endl;
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
	
	decltype(algo_kernels) new_algo_kernels;
	decltype(algo_max_local_sizes) new_algo_max_local_sizes;
	decltype(algo_kernel_usable) new_algo_kernel_usable;
	for (uint32_t i = 0; i < ALGO_KERNEL_TYPE::__MAX_ALGO_KERNEL_TYPE; ++i) {
		new_algo_kernels[i] = new_reduction_prog->get_kernel(algo_kernel_names[i]);
		if (new_algo_kernels[i] == nullptr) {
			log_error("failed to retrieve kernel $ from program", algo_kernel_names[i]);
			return false;
		}
		new_algo_max_local_sizes[i] = new_algo_kernels[i]->get_kernel_entry(*fastest_device)->max_total_local_size;
		if (fastest_device->context->get_compute_type() != COMPUTE_TYPE::HOST) {
			// usable if max local size >= required tile size, and if #args != 0 (i.e. kernel isn't disabled for other reasons)
			new_algo_kernel_usable[i] = ((algo_kernel_sizes[i] <= new_algo_max_local_sizes[i].x) &&
										 !new_algo_kernels[i]->get_kernel_entry(*fastest_device)->info->args.empty());
		} else {
			// always usable with host-compute
			new_algo_kernel_usable[i] = true;
		}
		log_debug("$: local size: $, usable: $", algo_kernel_names[i], new_algo_max_local_sizes[i],
				  new_algo_kernel_usable[i]);
	}
	
	// everything was successful, exchange objects
	reduction_prog = new_reduction_prog;
	algo_kernels = new_algo_kernels;
	algo_max_local_sizes = new_algo_max_local_sizes;
	algo_kernel_usable = new_algo_kernel_usable;
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
	
	// add event handlers
	event::handler evt_handler_fnctr(&evt_handler);
	floor::get_event()->add_internal_event_handler(evt_handler_fnctr,
												   EVENT_TYPE::QUIT, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_DOWN);
	
	// get the compute context that has been automatically created (opencl/cuda/metal/vulkan/host)
	compute_ctx = floor::get_compute_context();
	
	// create a compute queue (aka command queue or stream) for the fastest device in the context
	fastest_device = compute_ctx->get_device(compute_device::TYPE::FASTEST);
	dev_queue = compute_ctx->create_queue(*fastest_device);
	
	// parameter sanity check (TODO)
	
	// compile the program and get the kernel functions
	if(!compile_kernels()) return -1;
	
	// reduction/scan buffers
	uint32_t elem_count = 0;
	if (reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_F32) {
#if !defined(FLOOR_IOS)
		elem_count = 1024 * 1024 * 256; // == 1024 MiB (32-bit), 2048 MiB (64-bit)
#else
		elem_count = 1024 * 1024 * 64; // == 256 MiB (32-bit), 512 MiB (64-bit)
#endif
	} else if (reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_U32) {
		// must use fewer elements for uint reduction, because we're limited to a 32-bit value range
#if !defined(FLOOR_IOS)
		elem_count = 1024 * 1024 * 128; // == 512 MiB (32-bit)
#else
		elem_count = 1024 * 1024 * 64; // == 256 MiB (32-bit)
#endif
	} else {
		// must use fewer elements for inclusive/exclusive scan, because we're limited to a 32-bit value range
		elem_count = 1024 * 1024 * 32;
	}
	auto red_data_sum = compute_ctx->create_buffer(*dev_queue, sizeof(uint32_t) /* 32-bit */,
												   COMPUTE_MEMORY_FLAG::READ_WRITE |
												   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
	auto compute_data = compute_ctx->create_buffer(*dev_queue, elem_count * sizeof(uint32_t) /* 32-bit */,
												   COMPUTE_MEMORY_FLAG::READ_WRITE |
												   COMPUTE_MEMORY_FLAG::HOST_WRITE);
	shared_ptr<compute_buffer> compute_output_data;
	if (reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN ||
		reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::EXCLUSIVE_SCAN) {
		compute_output_data = compute_ctx->create_buffer(*dev_queue, elem_count * sizeof(uint32_t) /* 32-bit */,
														 COMPUTE_MEMORY_FLAG::READ_WRITE |
														 COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
	}
	auto cpu_data = make_aligned_ptr<uint32_t>(elem_count);
	
	// compute the ideal global size (#units * local size), if the unit count is unknown, assume 16, so that we still get good throughput
	static constexpr const uint32_t unit_count_fallback { 12u };
	if (fastest_device->units == 0) {
		log_error("device #units is 0 - assuming $", unit_count_fallback);
	}
	const uint32_t dev_unit_count = (fastest_device->units != 0 ? fastest_device->units : unit_count_fallback);
	const uint32_t reduction_global_size = dev_unit_count * 1024u * (fastest_device->is_gpu() ? 2u : 1u);
	
	// inclusive/exclusive scan execution parameters
	const compute_queue::execution_parameters_t exec_params_scan_local {
		.execution_dim = 1u,
		.global_work_size = { elem_count, 1u, 1u },
		.local_work_size = { 1024u, 1u, 1u },
		.args = { compute_output_data, compute_data, elem_count },
		.wait_until_completion = true, // must always wait
		.debug_label = (reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN ?
						"incl_scan_local" : "excl_scan_local"),
	};
	const compute_queue::execution_parameters_t exec_params_scan_global {
		.execution_dim = 1u,
		.global_work_size = { elem_count, 1u, 1u },
		.local_work_size = { 1024u, 1u, 1u },
		.args = { compute_data, compute_output_data, elem_count },
		.wait_until_completion = true, // must always wait
		.debug_label = (reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN ?
						"incl_scan_global" : "excl_scan_global"),
	};
	
	//
	random_device rd;
	mt19937 gen(rd());
	// main loop
	uint32_t iteration = 0;
	while (!reduction_state.done) {
		floor::get_event()->handle_events();
		
		// init data
		{
			// reduction: actually init "compute_data" directly, scan: we to flip/flop these, so init "compute_output_data"
			auto compute_init_buffer = (reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_F32 ||
										reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_U32 ?
										compute_data.get() : compute_output_data.get());
			auto mrdata = compute_init_buffer->map(*dev_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
			const auto init_data = [&mrdata, &cpu_data, &gen, &elem_count]<bool is_float>() {
				uniform_real_distribution<float> f32_dist { 0.0f, 0.025f };
				uniform_int_distribution<uint32_t> u32_dist { 0, 16 };
				
				auto rdata_f32 = (float*)mrdata;
				auto rdata_u32 = (uint32_t*)mrdata;
				auto cpu_data_f32 = (float*)cpu_data.get();
				auto cpu_data_u32 = (uint32_t*)cpu_data.get();
				for (uint32_t i = 0; i < elem_count; ++i) {
					if constexpr (is_float) {
						auto val = f32_dist(gen);
						*rdata_f32++ = val;
						*cpu_data_f32++ = val;
					} else {
						auto val = u32_dist(gen);
						*rdata_u32++ = val;
						*cpu_data_u32++ = val;
					}
				}
			};
			switch (reduction_state.exec_mode) {
				case reduction_state_struct::EXEC_MODE::REDUCTION_F32:
					init_data.operator()<true>();
					break;
				case reduction_state_struct::EXEC_MODE::REDUCTION_U32:
				case reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN:
				case reduction_state_struct::EXEC_MODE::EXCLUSIVE_SCAN:
					init_data.operator()<false>();
					break;
			}
			compute_init_buffer->unmap(*dev_queue, mrdata);
		}
		
		long double expected_sum_ld = 0.0L;
		float expected_sum_f = 0.0f;
		double expected_sum = 0.0;
		uint32_t expected_sum_u32 = 0;
		switch (reduction_state.exec_mode) {
			case reduction_state_struct::EXEC_MODE::REDUCTION_F32: {
				// for expected sum on the CPU use: https://en.wikipedia.org/wiki/Kahan_summation_algorithm
				double kahan_c = 0.0;
				auto cpu_data_f32 = (float*)cpu_data.get();
#pragma clang loop vectorize(enable)
				for (uint32_t i = 0; i < elem_count; ++i) {
					const auto rnd_val = cpu_data_f32[i];
					const auto kahan_y = double(rnd_val) - kahan_c;
					const auto kahan_t = expected_sum + kahan_y;
					kahan_c = (kahan_t - expected_sum) - kahan_y;
					expected_sum = kahan_t;
					expected_sum_ld += (long double)rnd_val;
					expected_sum_f += rnd_val;
				}
				break;
			}
			case reduction_state_struct::EXEC_MODE::REDUCTION_U32: {
				auto cpu_data_u32 = (uint32_t*)cpu_data.get();
#pragma clang loop vectorize(enable)
				for (uint32_t i = 0; i < elem_count; ++i) {
					expected_sum_u32 += cpu_data_u32[i];
				}
				break;
			}
			case reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN: {
				auto cpu_data_u32 = (uint32_t*)cpu_data.get();
				for (uint32_t i = 1; i < elem_count; ++i) {
					cpu_data_u32[i] += cpu_data_u32[i - 1u];
				}
				break;
			}
			case reduction_state_struct::EXEC_MODE::EXCLUSIVE_SCAN: {
				auto cpu_data_u32 = (uint32_t*)cpu_data.get();
				uint32_t prev_sum = 0u, sum = 0u;
				for (uint32_t i = 0; i < elem_count; ++i) {
					sum += cpu_data_u32[i];
					cpu_data_u32[i] = prev_sum;
					prev_sum = sum;
				}
				break;
			}
		}
		
		//
		red_data_sum->zero(*dev_queue);
		dev_queue->finish();
		dev_queue->start_profiling();
		switch (reduction_state.exec_mode) {
			case reduction_state_struct::EXEC_MODE::REDUCTION_F32:
			case reduction_state_struct::EXEC_MODE::REDUCTION_U32: {
				if (fastest_device->cooperative_kernel_support) {
					dev_queue->execute_cooperative(*algo_kernels[reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_F32 ?
																 REDUCTION_ADD_F32_512 : REDUCTION_ADD_U32_512],
												   uint1 { fastest_device->max_coop_total_local_size /* max concurrent threads */ * fastest_device->units /* #multiprocessors */ },
												   uint1 { 512u },
												   compute_data, red_data_sum, elem_count);
				} else {
					dev_queue->execute(*algo_kernels[reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_F32 ?
													 REDUCTION_ADD_F32_1024 : REDUCTION_ADD_U32_1024],
									   uint1 { reduction_global_size },
									   uint1 { 1024 },
									   compute_data, red_data_sum, elem_count);
				}
				break;
			}
			case reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN: {
				dev_queue->execute_with_parameters(*algo_kernels[INCL_SCAN_LOCAL_U32_1024], exec_params_scan_local);
				dev_queue->execute_with_parameters(*algo_kernels[INCL_SCAN_GLOBAL_U32_1024], exec_params_scan_global);
				break;
			}
			case reduction_state_struct::EXEC_MODE::EXCLUSIVE_SCAN: {
				dev_queue->execute_with_parameters(*algo_kernels[EXCL_SCAN_LOCAL_U32_1024], exec_params_scan_local);
				dev_queue->execute_with_parameters(*algo_kernels[EXCL_SCAN_GLOBAL_U32_1024], exec_params_scan_global);
				break;
			}
		}
		const auto prof_time = dev_queue->stop_profiling();
		dev_queue->finish();
		
		const auto compute_bandwidth = [&elem_count](const uint64_t& microseconds) {
			static constexpr const size_t elem_size { sizeof(float) };
			const auto red_size = elem_size * elem_count;
			const auto size_per_second = (double(red_size) / 1000000000.0) / (double(microseconds) / 1000000.0);
			return size_per_second;
		};
		log_debug("$ computed in $ms -> $ GB/s",
				  ((reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_F32 ||
					reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::REDUCTION_U32) ? "reduction" :
				   (reduction_state.exec_mode == reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN ? "inclusive-scan" : "exclusive-scan")),
				  double(prof_time) / 1000.0, compute_bandwidth(prof_time));
		
		switch (reduction_state.exec_mode) {
			case reduction_state_struct::EXEC_MODE::REDUCTION_F32: {
				float sum = 0.0f;
				red_data_sum->read_to(sum, *dev_queue);
				const auto abs_diff = abs(double(sum) - expected_sum);
				log_debug("sum: $, expected: kahan: $, linear: $, $ -> diff: $ (sp: $)",
						  sum, expected_sum, expected_sum_ld, expected_sum_f, abs_diff, abs(sum - expected_sum_f));
				break;
			}
			case reduction_state_struct::EXEC_MODE::REDUCTION_U32: {
				uint32_t sum = 0u;
				red_data_sum->read_to(sum, *dev_queue);
				const auto abs_diff = abs(int64_t(sum) - int64_t(expected_sum_u32));
				log_debug("sum: $, expected: $ -> diff: $",
						  sum, expected_sum_u32, abs_diff);
				break;
			}
			case reduction_state_struct::EXEC_MODE::INCLUSIVE_SCAN:
			case reduction_state_struct::EXEC_MODE::EXCLUSIVE_SCAN: {
				// need to perform an element-wise compare of the CPU and GPU data to validate that everything is correct
				auto compute_output = compute_output_data->map(*dev_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
				auto cpu_data_u32 = (uint32_t*)cpu_data.get();
				auto compute_data_u32 = (uint32_t*)compute_output;
				bool correct = true;
				
				for (uint32_t i = 0; i < elem_count; ++i) {
					if (cpu_data_u32[i] != compute_data_u32[i]) {
						correct = false;
						log_error("scan output mismatch @$: CPU result: $ != compute device result: $",
								  i, cpu_data_u32[i], compute_data_u32[i]);
						break;
					}
				}
				if (correct) {
					log_debug("scan successful (CPU and compute device results match)");
				}
#if 0
				else {
					stringstream cpu_dump, gpu_dump;
					cpu_dump << "CPU: ";
					gpu_dump << "GPU: ";
					for (uint32_t i = 0; i < 1024 + 32; ++i) {
						cpu_dump << cpu_data_u32[i] << ", ";
						gpu_dump << compute_data_u32[i] << ", ";
					}
					log_warn("$", cpu_dump.str());
					log_warn("$", gpu_dump.str());
				}
#endif
				compute_output_data->unmap(*dev_queue, compute_output);
				break;
			}
		}
		
		// next
		++iteration;
		
		// when benchmarking, we need to make sure computation is actually finished (all 20 iterations were computed)
		if ((reduction_state.benchmark && iteration == 20) ||
			(!reduction_state.benchmark && iteration >= reduction_state.max_iterations)) {
			dev_queue->finish();
			break;
		}
	}
	log_msg("done!");
	
	// unregister event handler (we really don't want to react to events when destructing everything)
	floor::get_event()->remove_event_handler(evt_handler_fnctr);
	
	// cleanup
	compute_output_data = nullptr;
	compute_data = nullptr;
	red_data_sum = nullptr;
	
	// kthxbye
	floor::destroy();
	return 0;
}
