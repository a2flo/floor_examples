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
#include <floor/core/option_handler.hpp>

struct option_context {
	string filename { "" };
	llvm_compute::TARGET target { llvm_compute::TARGET::SPIR };
	string sub_target;
	uint32_t bitness { 64 };
	bool double_support { true };
	string additional_options { "" };
};
typedef option_handler<option_context> occ_opt_handler;

//! option -> function map
template<> unordered_map<string, occ_opt_handler::option_function> occ_opt_handler::options {
	{ "--help", [](option_context&, char**&) {
		log_undecorated("command line options:\n"
						"\t--src <file>: the source file that should be compiled\n"
						"\t--target [spir|ptx|air]: sets the compile target to OpenCL SPIR, CUDA PTX or Metal Apple-IR\n"
						"\t--sub-target <name>: sets the target specific sub-target (only PTX: sm_20 - sm_53)\n"
						"\t--bitness <32|64>: sets the bitness of the target (defaults to 64)\n"
						"\t--no-double: explicitly disables double support (only SPIR)\n"
						"\t--: end of occ options, everything beyond this point is piped through to the compiler");
	}},
	{ "--src", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			log_error("invalid argument!");
			return;
		}
		ctx.filename = *arg_ptr;
	}},
	{ "--target", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			log_error("invalid argument!");
			return;
		}
		
		const string target = *arg_ptr;
		if(target == "spir") {
			ctx.target = llvm_compute::TARGET::SPIR;
		}
		else if(target == "ptx") {
			ctx.target = llvm_compute::TARGET::PTX;
		}
		else if(target == "air") {
			ctx.target = llvm_compute::TARGET::AIR;
		}
		else {
			log_error("invalid target: %s", target);
			return;
		}
	}},
	{ "--sub-target", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			log_error("invalid argument!");
			return;
		}
		ctx.sub_target = *arg_ptr;
	}},
	{ "--bitness", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			log_error("invalid argument!");
			return;
		}
		ctx.bitness = stou(*arg_ptr);
	}},
	{ "--no-double", [](option_context& ctx, char**&) {
		ctx.double_support = false;
	}},
};

int main(int, char* argv[]) {
#if !defined(FLOOR_IOS)
	floor::init(argv[0], (const char*)"../../data/", true); // call path, data path, console-only
#else
	floor::init(argv[0], (const char*)"data/", true);
#endif
	
	// handle options
	option_context option_ctx;
	occ_opt_handler::parse_options(argv + 1, option_ctx);
	
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
			device = make_shared<cuda_device>();
			if(option_ctx.sub_target != "") {
				const auto sm_pos = option_ctx.sub_target.find("sm_");
				if(sm_pos == string::npos) {
					log_error("invalid PTX sub-target: %s", option_ctx.sub_target);
				}
				else {
					const string sm_version = option_ctx.sub_target.substr(sm_pos + 3, option_ctx.sub_target.length() - sm_pos - 3);
					const auto sm_version_int = stoul(sm_version);
					((cuda_device*)device.get())->sm.x = (uint32_t)sm_version_int / 10u;
					((cuda_device*)device.get())->sm.y = (uint32_t)sm_version_int % 10u;
				}
			}
			log_debug("compiling to PTX (sm_%u) ...", ((cuda_device*)device.get())->sm.x * 10 + ((cuda_device*)device.get())->sm.y);
			break;
		case llvm_compute::TARGET::AIR:
			log_debug("compiling to AIR ...");
			device = make_shared<metal_device>();
			break;
	}
	device->bitness = option_ctx.bitness;
	device->double_support = option_ctx.double_support;
	
	// compile
	auto program_data = llvm_compute::compile_program_file(device, option_ctx.filename, option_ctx.additional_options, option_ctx.target);
	for(const auto& info : program_data.second) {
		string info_str = "";
		for(size_t i = 0, count = info.args.size(); i < count; ++i) {
			switch(info.args[i].address_space) {
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL:
					info_str += "global ";
					break;
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::LOCAL:
					info_str += "local ";
					break;
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT:
					info_str += "constant ";
					break;
				case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE:
					info_str += "image ";
					break;
				default: break;
			}
			
			if(info.args[i].address_space != llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE) {
				info_str += to_string(info.args[i].size);
			}
			else {
				switch(info.args[i].image_access) {
					case llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ:
						info_str += "read_only ";
						break;
					case llvm_compute::kernel_info::ARG_IMAGE_ACCESS::WRITE:
						info_str += "write_only ";
						break;
					case llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ_WRITE:
						info_str += "read_write ";
						break;
					default:
						info_str += "no_access? "; // shouldn't happen ...
						log_error("kernel image argument #%u has no access qualifier (%X)!", i, info.args[i].image_access);
						break;
				}
				
				if(option_ctx.target == llvm_compute::TARGET::PTX) {
					// image type is not stored for ptx
					info_str += to_string(info.args[i].size);
				}
				else {
					switch(info.args[i].image_type) {
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_1D:
							info_str += "1D";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_1D_ARRAY:
							info_str += "1D array";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_1D_BUFFER:
							info_str += "1D buffer";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D:
							info_str += "2D";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY:
							info_str += "2D array";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D_DEPTH:
							info_str += "2D depth";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_DEPTH:
							info_str += "2D array depth";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D_MSAA:
							info_str += "2D msaa";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA:
							info_str += "2D array msaa";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D_MSAA_DEPTH:
							info_str += "2D msaa depth";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA_DEPTH:
							info_str += "2D array msaa depth";
							break;
						case llvm_compute::kernel_info::ARG_IMAGE_TYPE::IMAGE_3D:
							info_str += "3D";
							break;
						default:
							info_str += "unknown_type";
							log_error("kernel image argument #%u has no type or an unknown type (%X)!", i, info.args[i].image_type);
							break;
					}
				}
			}
			info_str += (i + 1 < count ? ", " : " ");
		}
		info_str = core::trim(info_str);
		log_msg("compiled kernel: %s (%s)", info.name, info_str);
	}
	
	// kthxbye
	floor::destroy();
	return 0;
}
