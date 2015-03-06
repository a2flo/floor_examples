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
#include <floor/compute/llvm_compute.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/cuda/cuda_device.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include "option_handler.hpp"

int main(int, char* argv[]) {
#if !defined(FLOOR_IOS)
	floor::init(argv[0], (const char*)"../../data/", true); // call path, data path, console-only
#else
	floor::init(argv[0], (const char*)"data/", true);
#endif
	
	// handle options
	option_context option_ctx;
	option_handler::parse_options(argv + 1, option_ctx);
	
	// post-checking
	if(option_ctx.filename.empty()) {
		log_error("no source file set!");
		return -1;
	}
	
	// create target specific device
	shared_ptr<compute_device> device;
	switch(option_ctx.target) {
		case llvm_compute::TARGET::SPIR:
			log_debug("compiling to SPIR ...");
			device = make_shared<opencl_device>();
			break;
		case llvm_compute::TARGET::PTX:
			log_debug("compiling to PTX ...");
			device = make_shared<cuda_device>();
			break;
		case llvm_compute::TARGET::AIR:
			log_debug("compiling to AIR ...");
			device = make_shared<metal_device>();
			break;
	}
	device->double_support = option_ctx.double_support;
	
	// compile
	auto program_data = llvm_compute::compile_program_file(device, option_ctx.filename, "", option_ctx.target);
	for(const auto& info : program_data.second) {
		string sizes_str = "";
		for(size_t i = 0, count = info.arg_sizes.size(); i < count; ++i) {
			switch(info.arg_address_spaces[i]) {
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL:
					sizes_str += "global ";
					break;
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::LOCAL:
					sizes_str += "local ";
					break;
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT:
					sizes_str += "constant ";
					break;
				default: break;
			}
			sizes_str += to_string(info.arg_sizes[i]) + (i + 1 < count ? "," : "") + " ";
		}
		sizes_str = core::trim(sizes_str);
		log_msg("compiled kernel: %s (%s)", info.name, sizes_str);
	}
	
	// kthxbye
	floor::destroy();
	return 0;
}
