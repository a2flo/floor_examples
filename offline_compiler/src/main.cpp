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

#include <floor/core/essentials.hpp>
#if !defined(FLOOR_NO_OPENCL)
#define FLOOR_OPENCL_INFO_FUNCS
#endif

#include <floor/floor/floor.hpp>
#include <floor/floor/floor_version.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/spirv_handler.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/core/option_handler.hpp>

#include <floor/compute/opencl/opencl_common.hpp>
#include <floor/compute/opencl/opencl_compute.hpp>
#include <floor/compute/opencl/opencl_device.hpp>

#include <floor/compute/cuda/cuda_compute.hpp>
#include <floor/compute/cuda/cuda_device.hpp>

#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>

#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>

#include "fubar.hpp"

#define FLOOR_OCC_VERSION_STR FLOOR_MAJOR_VERSION_STR "." << FLOOR_MINOR_VERSION_STR "." FLOOR_REVISION_VERSION_STR FLOOR_DEV_STAGE_VERSION_STR
#define FLOOR_OCC_FULL_VERSION_STR "offline compute compiler v" FLOOR_OCC_VERSION_STR

struct option_context {
	string filename { "" };
	string output_filename { "" };
	llvm_toolchain::TARGET target { llvm_toolchain::TARGET::SPIR };
	string sub_target;
	uint32_t bitness { 64 };
	OPENCL_VERSION cl_std { OPENCL_VERSION::OPENCL_1_2 };
	METAL_VERSION metal_std { METAL_VERSION::METAL_1_1 };
	uint32_t ptx_version { 43 };
	bool warnings { false };
	bool workarounds { false };
	uint32_t double_support { 0 }; // 0 = undefined/default, 1 = enabled, 2 = disabled
	bool basic_64_atomics { false };
	bool extended_64_atomics { false };
	bool sub_groups { false };
	bool sw_depth_compare { true };
	uint32_t image_rw_support { 0 }; // 0 = undefined/default, 1 = enabled, 2 = disabled
	bool test { false };
	bool test_bin { false };
	bool cuda_sass { false };
	bool spirv_text { false };
	string cuda_sdk_path { "" };
	string cuda_sass_filename { "" };
	uint32_t cuda_max_registers { 0 };
	bool cuda_no_short_ptr { false };
	string spirv_text_filename { "" };
	string test_bin_filename { "" };
	string ffi_filename { "" };
	string additional_options { "" };
	size_t verbosity { (size_t)logger::LOG_TYPE::WARNING_MSG };
	string config_path { "../../data/" };
	bool done { false };
	bool native_cl { false };
	
	bool is_fubar_build { false };
	fubar::TARGET_SET fubar_target_set { fubar::TARGET_SET::MINIMAL };
	string fubar_target_set_file_name { "" };
};
typedef option_handler<option_context> occ_opt_handler;

//! option -> function map
template<> vector<pair<string, occ_opt_handler::option_function>> occ_opt_handler::options {
	{ "--help", [](option_context& ctx, char**&) {
		cout << FLOOR_OCC_FULL_VERSION_STR << endl;
		cout << ("command line options:\n"
				 "\t--src <input-file>: the source file that should be compiled\n"
				 "\t--out <output-file>: the output file name (defaults to {$file.fubar,spir.bc,cuda.ptx,metal.air})\n"
				 "\t--fubar <targets-json|all|minimal>: builds a Floor Universal Binary ARchive (reads targets from a .json file; uses all/minimal target set)\n"
				 "\t--target [spir|ptx|air|spirv]: sets the compile target to OpenCL SPIR, CUDA PTX, Metal Apple-IR, or Vulkan/OpenCL SPIR-V\n"
				 "\t--sub-target <name>: sets the target specific sub-target\n"
				 "\t    PTX:           [sm_20|sm_21|sm_30|sm_32|sm_35|sm_37|sm_50|sm_52|sm_53|sm_60|sm_61|sm_62|sm_70|sm_72|sm_73|sm_75], defaults to sm_20\n"
				 "\t    SPIR:          [gpu|cpu|opencl-gpu|opencl-cpu], defaults to gpu\n"
				 "\t    Metal/AIR:     [ios|osx|macos], defaults to ios\n"
				 "\t    SPIR-V:        [vulkan|opencl|opencl-gpu|opencl-cpu], defaults to vulkan, when set to opencl, defaults to opencl-gpu\n"
				 "\t--bitness <32|64>: sets the bitness of the target (defaults to 64)\n"
				 "\t--cl-std <1.2|2.0|2.1|2.2>: sets the supported OpenCL version (must be 1.2 for SPIR, can be any for OpenCL SPIR-V)\n"
				 "\t--metal-std <1.1|1.2|2.0|2.1>: sets the supported Metal version (defaults to 1.1)\n"
				 "\t--ptx-version <43|50|60|61|62|63>: sets/overwrites the PTX version that should be used/emitted (defaults to 43)\n"
				 "\t--warnings: if set, enables a wide range of compiler warnings\n"
				 "\t--workarounds: if set, enable all possible workarounds (Metal and SPIR-V only)\n"
				 "\t--with-double: explicitly enables double support (only SPIR/SPIR-V)\n"
				 "\t--no-double: explicitly disables double support (only SPIR/SPIR-V)\n"
				 "\t--64-bit-atomics: explicitly enables basic 64-bit atomic operations support (only SPIR/SPIR-V, always enabled on PTX)\n"
				 "\t--ext-64-bit-atomics: explicitly enables extended 64-bit atomic operations support (only SPIR/SPIR-V, enabled on PTX if sub-target >= sm_32)\n"
				 "\t--sub-groups: explicitly enables sub-group support\n"
				 "\t--depth-compare <sw|hw>: select between software and hardware depth compare code generation (only PTX)\n"
				 "\t--image-rw <sw|hw>: sets the image r/w support mode (if unspecified, will use the target default)\n"
				 "\t--cuda-sdk <path>: path to the CUDA SDK that should be used when calling ptxas/cuobjdump (only PTX)\n"
				 "\t--cuda-sass <output-file>: assembles a final device binary using ptxas and then disassembles it using cuobjdump (only PTX)\n"
				 "\t--cuda-max-registers <#registers>: restricts/specifies how many registers can be used when jitting the PTX code (default: 0/unlimited)\n"
				 "\t--cuda-no-short-ptr: disables use of short/32-bit pointers for non-global memory\n"
				 "\t--spirv-text <output-file>: outputs human-readable SPIR-V assembly\n"
				 "\t--test: tests/compiles the compiled binary on the target platform (if possible) - experimental!\n"
				 "\t--test-bin <input-file> <function-info-file>: tests/compiles the specified binary on the target platform (if possible) - experimental!\n"
				 "\t-v: verbose output (DBG level)\n"
				 "\t-vv: very verbose output (MSG level)\n"
				 "\t--version: prints the occ/floor version\n"
				 "\t--config <path>: the path where config.json is located (defaults to \"../../data/\")\n"
				 "\t--: end of occ options, everything beyond this point is piped through to the compiler") << endl;
		ctx.done = true;
	}},
	{ "--src", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.filename = *arg_ptr;
	}},
	{ "--out", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr) {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.output_filename = *arg_ptr;
	}},
	{ "--fubar", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr) {
			cerr << "invalid argument!" << endl;
			return;
		}
		
		ctx.is_fubar_build = true;
		ctx.fubar_target_set_file_name = *arg_ptr;
		if (ctx.fubar_target_set_file_name == "all") {
			ctx.fubar_target_set = fubar::TARGET_SET::ALL;
		} else if (ctx.fubar_target_set_file_name == "minimal") {
			ctx.fubar_target_set = fubar::TARGET_SET::MINIMAL;
		} else {
			ctx.fubar_target_set = fubar::TARGET_SET::USER_JSON;
		}
	}},
	{ "--target", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		
		const string target = *arg_ptr;
		if(target == "spir") {
			ctx.target = llvm_toolchain::TARGET::SPIR;
		}
		else if(target == "ptx") {
			ctx.target = llvm_toolchain::TARGET::PTX;
		}
		else if(target == "air") {
			ctx.target = llvm_toolchain::TARGET::AIR;
		}
		else if(target == "spirv") {
			ctx.target = llvm_toolchain::TARGET::SPIRV_VULKAN;
		}
		else if(target == "native-cl") {
			// very hacky and undocumented way of compiling std opencl c on the native opencl platform
			ctx.target = llvm_toolchain::TARGET::SPIR;
			ctx.native_cl = true;
		}
		else {
			cerr << "invalid target: " << target << endl;
			return;
		}
	}},
	{ "--sub-target", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.sub_target = *arg_ptr;
	}},
	{ "--bitness", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.bitness = stou(*arg_ptr);
	}},
	{ "--cl-std", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		const string cl_version_str = *arg_ptr;
		if(cl_version_str == "1.2") { ctx.cl_std = OPENCL_VERSION::OPENCL_1_2; }
		else if(cl_version_str == "2.0") { ctx.cl_std = OPENCL_VERSION::OPENCL_2_0; }
		else if(cl_version_str == "2.1") { ctx.cl_std = OPENCL_VERSION::OPENCL_2_1; }
		else if(cl_version_str == "2.2") { ctx.cl_std = OPENCL_VERSION::OPENCL_2_2; }
		else {
			cerr << "invalid --cl-std argument" << endl;
			return;
		}
	}},
	{ "--metal-std", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		const string mtl_version_str = *arg_ptr;
		if(mtl_version_str == "1.1") { ctx.metal_std = METAL_VERSION::METAL_1_1; }
		else if(mtl_version_str == "1.2") { ctx.metal_std = METAL_VERSION::METAL_1_2; }
		else if(mtl_version_str == "2.0") { ctx.metal_std = METAL_VERSION::METAL_2_0; }
		else if(mtl_version_str == "2.1") { ctx.metal_std = METAL_VERSION::METAL_2_1; }
		else {
			cerr << "invalid --metal-std argument" << endl;
			return;
		}
	}},
	{ "--ptx-version", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.ptx_version = stou(*arg_ptr);
		if(ctx.ptx_version != 43 && ctx.ptx_version != 50 &&
		   ctx.ptx_version != 60 && ctx.ptx_version != 61 && ctx.ptx_version != 62 && ctx.ptx_version != 63) {
			cerr << "invalid --ptx-version argument" << endl;
			return;
		}
	}},
	{ "--warnings", [](option_context& ctx, char**&) {
		ctx.warnings = true;
	}},
	{ "--workarounds", [](option_context& ctx, char**&) {
		ctx.workarounds = true;
	}},
	{ "--with-double", [](option_context& ctx, char**&) {
		ctx.double_support = 1;
	}},
	{ "--no-double", [](option_context& ctx, char**&) {
		ctx.double_support = 2;
	}},
	{ "--64-bit-atomics", [](option_context& ctx, char**&) {
		ctx.basic_64_atomics = true;
	}},
	{ "--ext-64-bit-atomics", [](option_context& ctx, char**&) {
		ctx.extended_64_atomics = true;
	}},
	{ "--sub-groups", [](option_context& ctx, char**&) {
		ctx.sub_groups = true;
	}},
	{ "--depth-compare", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		const string dc_mode = *arg_ptr;
		if(dc_mode == "sw") { ctx.sw_depth_compare = true; }
		else if(dc_mode == "hw") { ctx.sw_depth_compare = false; }
		else {
			cerr << "invalid --depth-compare argument" << endl;
			return;
		}
	}},
	{ "--image-rw", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		const string rw_mode = *arg_ptr;
		if(rw_mode == "sw") { ctx.image_rw_support = 2; }
		else if(rw_mode == "hw") { ctx.image_rw_support = 1; }
		else {
			cerr << "invalid --image-rw argument" << endl;
			return;
		}
	}},
	{ "--test", [](option_context& ctx, char**&) {
		ctx.test = true;
	}},
	{ "--test-bin", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after --test-bin!" << endl;
			return;
		}
		ctx.test_bin_filename = *arg_ptr;
		
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument after second --test-bin parameter!" << endl;
			return;
		}
		ctx.ffi_filename = *arg_ptr;
		
		ctx.test_bin = true;
	}},
	{ "--cuda-sdk", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr) {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.cuda_sdk_path = *arg_ptr;
	}},
	{ "--cuda-sass", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr) {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.cuda_sass_filename = *arg_ptr;
		ctx.cuda_sass = true;
	}},
	{ "--cuda-max-registers", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr) {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.cuda_max_registers = stou(*arg_ptr);
	}},
	{ "--cuda-no-short-ptr", [](option_context& ctx, char**&) {
		ctx.cuda_no_short_ptr = true;
	}},
	{ "--spirv-text", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr) {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.spirv_text_filename = *arg_ptr;
		ctx.spirv_text = true;
	}},
	{ "-v", [](option_context& ctx, char**&) {
		ctx.verbosity = (size_t)logger::LOG_TYPE::DEBUG_MSG;
	}},
	{ "-vv", [](option_context& ctx, char**&) {
		ctx.verbosity = (size_t)logger::LOG_TYPE::UNDECORATED;
	}},
	{ "--version", [](option_context& ctx, char**&) {
		cout << FLOOR_OCC_FULL_VERSION_STR << endl;
		ctx.done = true;
	}},
	{ "--config", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr) {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.config_path = *arg_ptr;
	}},
};

static int run_normal_build(option_context& option_ctx) {
	// handle spir-v target change
	if(option_ctx.target == llvm_toolchain::TARGET::SPIRV_VULKAN &&
	   (option_ctx.sub_target == "opencl" ||
		option_ctx.sub_target == "opencl-gpu" ||
		option_ctx.sub_target == "opencl-cpu")) {
		   option_ctx.target = llvm_toolchain::TARGET::SPIRV_OPENCL;
	   }
	
	llvm_toolchain::program_data program {};
	if(!option_ctx.test_bin && !option_ctx.native_cl) {
		// post-checking
		if(option_ctx.filename.empty()) {
			log_error("no source file set!");
			return -1;
		}
		
		// create target specific device
		shared_ptr<compute_device> device;
		switch(option_ctx.target) {
			case llvm_toolchain::TARGET::SPIR:
			case llvm_toolchain::TARGET::SPIRV_OPENCL: {
				const char* target_name = (option_ctx.target == llvm_toolchain::TARGET::SPIR ? "SPIR" :
										   option_ctx.target == llvm_toolchain::TARGET::SPIRV_OPENCL ? "SPIR-V OpenCL" : "UNKNOWN");
				device = make_shared<opencl_device>();
				if(option_ctx.sub_target == "" || option_ctx.sub_target == "gpu" ||
				   option_ctx.sub_target == "opencl" || option_ctx.sub_target == "opencl-gpu") {
					device->type = compute_device::TYPE::GPU;
				}
				else if(option_ctx.sub_target == "cpu" || option_ctx.sub_target == "opencl-cpu") {
					device->type = compute_device::TYPE::CPU;
				}
				else {
					log_error("invalid %s sub-target: %s", target_name, option_ctx.sub_target);
					return -4;
				}
				((opencl_device*)device.get())->cl_version = option_ctx.cl_std;
				if(option_ctx.basic_64_atomics) device->basic_64_bit_atomics_support = true;
				if(option_ctx.extended_64_atomics) device->extended_64_bit_atomics_support = true;
				if(option_ctx.sub_groups) device->sub_group_support = true;
				device->double_support = (option_ctx.double_support != 2); // enabled by default
				// enable common image support
				device->image_support = true;
				device->image_depth_support = true;
				device->image_msaa_support = true;
				device->image_msaa_array_support = true;
				device->image_mipmap_support = true;
				device->image_mipmap_write_support = true;
				if(option_ctx.target == llvm_toolchain::TARGET::SPIRV_OPENCL) {
					((opencl_device*)device.get())->spirv_version = SPIRV_VERSION::SPIRV_1_0;
					device->image_read_write_support = (option_ctx.image_rw_support == 2 ? false : true);
					device->param_workaround = option_ctx.workarounds;
				}
				else {
					device->image_read_write_support = (option_ctx.image_rw_support == 1 ? true : false);
					if(option_ctx.target == llvm_toolchain::TARGET::SPIR && option_ctx.workarounds) {
						option_ctx.additional_options += " -Xclang -cl-spir-intel-workarounds ";
					}
				}
				log_debug("compiling to %s (%s) ...", target_name, (device->type == compute_device::TYPE::GPU ? "GPU" : "CPU"));
			} break;
			case llvm_toolchain::TARGET::PTX:
				device = make_shared<cuda_device>();
				if(option_ctx.sub_target != "") {
					const auto sm_pos = option_ctx.sub_target.find("sm_");
					if(sm_pos == string::npos) {
						log_error("invalid PTX sub-target: %s", option_ctx.sub_target);
						return -2;
					}
					else {
						const string sm_version = option_ctx.sub_target.substr(sm_pos + 3, option_ctx.sub_target.length() - sm_pos - 3);
						const auto sm_version_int = stoul(sm_version);
						((cuda_device*)device.get())->sm.x = (uint32_t)sm_version_int / 10u;
						((cuda_device*)device.get())->sm.y = (uint32_t)sm_version_int % 10u;
					}
				}
				else option_ctx.sub_target = "sm_20";
				device->image_depth_compare_support = (option_ctx.sw_depth_compare ? false : true);
				device->sub_group_shuffle_support = (((cuda_device*)device.get())->sm.x >= 3);
				device->extended_64_bit_atomics_support = (((cuda_device*)device.get())->sm.x > 3 ||
														   (((cuda_device*)device.get())->sm.x == 3 && ((cuda_device*)device.get())->sm.y >= 2));
				log_debug("compiling to PTX (sm_%u) ...", ((cuda_device*)device.get())->sm.x * 10 + ((cuda_device*)device.get())->sm.y);
				break;
			case llvm_toolchain::TARGET::AIR: {
				device = make_shared<metal_device>();
				metal_device* dev = (metal_device*)device.get();
				dev->metal_version = option_ctx.metal_std;
				
				dev->family = 1;
				dev->family_version = 1;
				if(option_ctx.sub_target == "" || option_ctx.sub_target.find("ios") == 0) {
					dev->feature_set = 0;
				}
				else if(option_ctx.sub_target.find("osx") == 0 || option_ctx.sub_target.find("macos") == 0) {
					dev->feature_set = 10000;
					if(option_ctx.workarounds) {
						option_ctx.additional_options += " -Xclang -metal-intel-workarounds -Xclang -metal-nvidia-workarounds ";
					}
				}
				else {
					log_error("invalid AIR sub-target: %s", option_ctx.sub_target);
					return -3;
				}
				device->double_support = (option_ctx.double_support == 1); // disabled by default
				log_debug("compiling to AIR (feature set: %u) ...", dev->feature_set);
			} break;
			case llvm_toolchain::TARGET::SPIRV_VULKAN:
				device = make_shared<vulkan_device>();
				if(option_ctx.sub_target != "" && option_ctx.sub_target != "vulkan") {
					log_error("invalid SPIR-V Vulkan sub-target: %s", option_ctx.sub_target);
					return -4;
				}
				device->type = compute_device::TYPE::GPU; // there are non-gpu devices as well, but this makes more sense
				if(option_ctx.basic_64_atomics) device->basic_64_bit_atomics_support = true;
				if(option_ctx.extended_64_atomics) device->extended_64_bit_atomics_support = true;
				device->double_support = (option_ctx.double_support == 1); // disabled by default
				
				// mip-mapping support is already enabled, just need to set the max supported mip level count
				device->max_mip_levels = 15;
				
				// not supported right now
				//if(option_ctx.sub_groups) device->sub_group_support = true;
				//device->image_read_write_support = (option_ctx.image_rw_support == 2 ? false : true);
				
				log_debug("compiling to SPIR-V Vulkan ...");
				break;
		}
		device->bitness = option_ctx.bitness;
		
		// compile
		llvm_toolchain::compile_options options {
			.target = option_ctx.target,
			.cli = option_ctx.additional_options,
			.enable_warnings = option_ctx.warnings,
			.cuda.ptx_version = option_ctx.ptx_version,
			.cuda.max_registers = option_ctx.cuda_max_registers,
			.cuda.short_ptr = (option_ctx.cuda_no_short_ptr ? false : true),
		};
		program = llvm_toolchain::compile_program_file(device, option_ctx.filename, options);
		for(const auto& info : program.functions) {
			string info_str = "";
			for(size_t i = 0, count = info.args.size(); i < count; ++i) {
				switch(info.args[i].address_space) {
					case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::GLOBAL:
						info_str += "global ";
						break;
					case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::LOCAL:
						info_str += "local ";
						break;
					case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::CONSTANT:
						info_str += "constant ";
						break;
					case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::IMAGE:
						info_str += "image ";
						break;
					default: break;
				}
				
				switch(info.args[i].special_type) {
					case llvm_toolchain::function_info::SPECIAL_TYPE::STAGE_INPUT:
						info_str += "stage_input ";
						break;
					case llvm_toolchain::function_info::SPECIAL_TYPE::PUSH_CONSTANT:
						info_str += "push_constant ";
						break;
					case llvm_toolchain::function_info::SPECIAL_TYPE::SSBO:
						info_str += "ssbo ";
						break;
					case llvm_toolchain::function_info::SPECIAL_TYPE::IMAGE_ARRAY:
						info_str += "array [" + to_string(info.args[i].size) + "] ";
						break;
					default: break;
				}
				
				if(info.args[i].address_space != llvm_toolchain::function_info::ARG_ADDRESS_SPACE::IMAGE) {
					info_str += to_string(info.args[i].size);
				}
				else {
					switch(info.args[i].image_access) {
						case llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ:
							info_str += "read_only ";
							break;
						case llvm_toolchain::function_info::ARG_IMAGE_ACCESS::WRITE:
							info_str += "write_only ";
							break;
						case llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ_WRITE:
							info_str += "read_write ";
							break;
						default:
							info_str += "no_access? "; // shouldn't happen ...
							log_error("kernel image argument #%u has no access qualifier (%X)!", i, info.args[i].image_access);
							break;
					}
					
					if(option_ctx.target == llvm_toolchain::TARGET::PTX) {
						// image type is not stored for ptx
						info_str += to_string(info.args[i].size);
					}
					else {
						switch(info.args[i].image_type) {
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_1D:
								info_str += "1D";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_1D_ARRAY:
								info_str += "1D array";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_1D_BUFFER:
								info_str += "1D buffer";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D:
								info_str += "2D";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY:
								info_str += "2D array";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D_DEPTH:
								info_str += "2D depth";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_DEPTH:
								info_str += "2D array depth";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D_MSAA:
								info_str += "2D msaa";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA:
								info_str += "2D array msaa";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D_MSAA_DEPTH:
								info_str += "2D msaa depth";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA_DEPTH:
								info_str += "2D array msaa depth";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_3D:
								info_str += "3D";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_CUBE:
								info_str += "cube";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_CUBE_ARRAY:
								info_str += "cube array";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_CUBE_DEPTH:
								info_str += "cube depth";
								break;
							case llvm_toolchain::function_info::ARG_IMAGE_TYPE::IMAGE_CUBE_ARRAY_DEPTH:
								info_str += "cube array depth";
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
			log_msg("compiled %s function: %s (%s)",
					info.type == llvm_toolchain::function_info::FUNCTION_TYPE::KERNEL ? "kernel" :
					info.type == llvm_toolchain::function_info::FUNCTION_TYPE::VERTEX ? "vertex" :
					info.type == llvm_toolchain::function_info::FUNCTION_TYPE::FRAGMENT ? "fragment" : "unknown",
					info.name, info_str);
		}
		
		// output
		const bool wants_console_output = (option_ctx.output_filename == "-");
		if(option_ctx.output_filename == "" || option_ctx.output_filename == "-") {
			// output file name?
			option_ctx.output_filename = "unknown.bin";
			switch(option_ctx.target) {
				case llvm_toolchain::TARGET::SPIR: option_ctx.output_filename = "spir.bc"; break;
				case llvm_toolchain::TARGET::PTX: option_ctx.output_filename = "cuda.ptx"; break;
				// don't do anything for air and spir-v, it's already stored in a file
				case llvm_toolchain::TARGET::AIR:
				case llvm_toolchain::TARGET::SPIRV_VULKAN:
				case llvm_toolchain::TARGET::SPIRV_OPENCL: option_ctx.output_filename = ""; break;
			}
		}
		
		// write to file
		if(option_ctx.target == llvm_toolchain::TARGET::AIR ||
		   option_ctx.target == llvm_toolchain::TARGET::SPIRV_VULKAN ||
		   option_ctx.target == llvm_toolchain::TARGET::SPIRV_OPENCL) {
			// program_data.first always contains the air/spir-v binary filename
			// -> just move the file if an output file was actually requested
			if(option_ctx.output_filename != "") {
				core::system("mv \"" + program.data_or_filename + "\" \"" + option_ctx.output_filename + "\"");
			}
			else option_ctx.output_filename = program.data_or_filename;
		}
		else file_io::string_to_file(option_ctx.output_filename, program.data_or_filename);
		
		// print to console, as well as to file!
		if(wants_console_output) {
			if(option_ctx.target == llvm_toolchain::TARGET::AIR) {
				string output = "";
				core::system("\"" + floor::get_metal_dis() + "\" -o - " + option_ctx.output_filename, output);
				log_undecorated("%s", output);
			}
			else if(option_ctx.target == llvm_toolchain::TARGET::SPIR) {
				string output = "";
				core::system("\"" + floor::get_opencl_dis() + "\" -o - " + option_ctx.output_filename, output);
				log_undecorated("%s", output);
			}
			else if(option_ctx.target == llvm_toolchain::TARGET::SPIRV_VULKAN ||
					option_ctx.target == llvm_toolchain::TARGET::SPIRV_OPENCL) {
				string output = "";
				core::system("\"" + (option_ctx.target == llvm_toolchain::TARGET::SPIRV_VULKAN ?
									 floor::get_vulkan_spirv_dis() : floor::get_opencl_spirv_dis()) +
							 "\" --debug-asm " + option_ctx.output_filename, output);
				log_undecorated("%s", output);
			}
			else {
				log_undecorated("%s", program.data_or_filename);
			}
		}
	}
	
	// handle cuda sass generation
	if(option_ctx.cuda_sass) {
		const auto dot_pos = option_ctx.output_filename.rfind(".");
		string cubin_filename = "";
		if(dot_pos != string::npos) cubin_filename = option_ctx.output_filename.substr(0, dot_pos);
		else cubin_filename = option_ctx.output_filename;
		
		const string cuda_bin_path = (option_ctx.cuda_sdk_path.empty() ? "" : option_ctx.cuda_sdk_path + "/bin/");
		const string ptxas_cmd {
			cuda_bin_path + "ptxas -O3"s +
			(option_ctx.verbosity >= (size_t)logger::LOG_TYPE::DEBUG_MSG ? " -v" : "") +
			" -m" + (option_ctx.bitness == 32 ? "32" : "64") +
			" -arch " + option_ctx.sub_target +
			" -o " + cubin_filename + ".cubin " +
			option_ctx.output_filename
		};
		if(floor::get_toolchain_log_commands()) log_debug("ptxas cmd: %s", ptxas_cmd);
		const string cuobjdump_cmd {
			cuda_bin_path + "cuobjdump --dump-sass " + cubin_filename + ".cubin" +
			(option_ctx.cuda_sass_filename == "-" ? ""s : " > " + option_ctx.cuda_sass_filename)
		};
		if(floor::get_toolchain_log_commands()) log_debug("cuobjdump cmd: %s", cuobjdump_cmd);
		string ptxas_output = "", cuobjdump_output = "";
		core::system(ptxas_cmd, ptxas_output);
		log_undecorated("%s", ptxas_output);
		core::system(cuobjdump_cmd, cuobjdump_output);
		log_undecorated("%s", cuobjdump_output);
	}
	
	// handle spir-v text assembly output
	if(option_ctx.spirv_text) {
		const auto& spirv_dis = (option_ctx.target == llvm_toolchain::TARGET::SPIRV_VULKAN ?
								 floor::get_vulkan_spirv_dis() : floor::get_opencl_spirv_dis());
		string spirv_dis_output = "";
		core::system("\"" + spirv_dis + "\" -o " + option_ctx.spirv_text_filename + " " + option_ctx.output_filename, spirv_dis_output);
		if(spirv_dis_output != "") {
			log_msg("spirv-dis: %s", spirv_dis_output);
		}
	}
	
	// test the compiled binary (if this was specified)
	if(option_ctx.test || option_ctx.test_bin || option_ctx.native_cl) {
		switch(option_ctx.target) {
			case llvm_toolchain::TARGET::SPIR:
			case llvm_toolchain::TARGET::SPIRV_OPENCL: {
#if !defined(FLOOR_NO_OPENCL)
				// have to create a proper opencl context to compile anything
				auto ctx = make_shared<opencl_compute>(floor::get_opencl_platform(), false /* no opengl here */, floor::get_opencl_whitelist());
				auto cl_ctx = ctx->get_opencl_context();
				auto dev = ctx->get_device(compute_device::TYPE::FASTEST);
				if(dev == nullptr) {
					log_error("no device available!");
					break;
				}
				log_debug("using device: %s", dev->name);
				auto cl_dev = ((opencl_device*)dev.get())->device_id;
				
				cl_program opencl_program { nullptr };
				if(!option_ctx.native_cl) {
					// pretty much copy&paste from opencl_compute::add_program(...) with some small changes (only build for 1 device):
					size_t binary_length;
					const unsigned char* binary_ptr;
					string binary_str = "";
					cl_int status = CL_SUCCESS;
					if(!option_ctx.test_bin) {
						binary_length = program.data_or_filename.size();
						binary_ptr = (const unsigned char*)program.data_or_filename.data();
					}
					else {
						if(!file_io::file_to_string(option_ctx.test_bin_filename, binary_str)) {
							log_error("failed to read test binary %s", option_ctx.test_bin_filename);
							break;
						}
						binary_length = binary_str.size();
						binary_ptr = (const unsigned char*)binary_str.data();
					}
					logger::flush();
					
					// create the program object ...
					cl_int create_err = CL_SUCCESS;
					if(option_ctx.target != llvm_toolchain::TARGET::SPIRV_OPENCL) {
						opencl_program = clCreateProgramWithBinary(cl_ctx, 1, (const cl_device_id*)&cl_dev,
																   &binary_length, &binary_ptr, &status, &create_err);
						if(create_err != CL_SUCCESS) {
							log_error("failed to create opencl program: %u", create_err, cl_error_to_string(create_err));
							log_error("devices binary status: %s", status);
							return {};
						}
						else log_debug("successfully created opencl program!");
					}
					else {
#if !defined(CL_VERSION_2_1)
						log_error("need to compile occ with OpenCL 2.1 support to build SPIR-V binaries");
						return {};
#else
						opencl_program = clCreateProgramWithIL(cl_ctx, (const void*)binary_ptr, binary_length, &create_err);
						if(create_err != CL_SUCCESS) {
							log_error("failed to create opencl program from IL/SPIR-V: %u: %s", create_err, cl_error_to_string(create_err));
							return {};
						}
						else log_debug("successfully created opencl program (from IL/SPIR-V)!");
#endif
					}
					
					// ... and build it
					const string build_options {
						(option_ctx.target == llvm_toolchain::TARGET::SPIR ? " -x spir -spir-std=1.2" : "")
					};
					CL_CALL_ERR_PARAM_RET(clBuildProgram(opencl_program, 1, (const cl_device_id*)&cl_dev, build_options.c_str(), nullptr, nullptr),
										  build_err, "failed to build opencl program", {});
					log_debug("check"); logger::flush();
				}
				else {
					// build std opencl c source code (for testing/dev purposes)
					const auto src = file_io::file_to_string(option_ctx.filename);
					const char* src_ptr = src.c_str();
					const auto src_size = src.size();
					cl_int create_err = CL_SUCCESS;
					opencl_program = clCreateProgramWithSource(cl_ctx, 1, &src_ptr, &src_size, &create_err);
					if(create_err != CL_SUCCESS) {
						log_error("failed to create opencl program: %u", create_err, cl_error_to_string(create_err));
						return {};
					}
					else log_debug("successfully created opencl program!");
					
					const string build_options = option_ctx.additional_options;
					CL_CALL_ERR_PARAM_RET(clBuildProgram(opencl_program, 1, (const cl_device_id*)&cl_dev, build_options.c_str(), nullptr, nullptr),
										  build_err, "failed to build opencl program", {});
					log_debug("check"); logger::flush();
				}
				
				// print out build log
				log_debug("build log: %s", cl_get_info<CL_PROGRAM_BUILD_LOG>(opencl_program, cl_dev));
				
				// print kernel info
				const auto kernel_count = cl_get_info<CL_PROGRAM_NUM_KERNELS>(opencl_program);
				const auto kernel_names_str = cl_get_info<CL_PROGRAM_KERNEL_NAMES>(opencl_program);
				log_debug("got %u kernels in program: %s", kernel_count, kernel_names_str);
				
				// retrieve the compiled binaries again
				const auto binaries = cl_get_info<CL_PROGRAM_BINARIES>(opencl_program);
				for(size_t i = 0; i < binaries.size(); ++i) {
					file_io::string_to_file("binary_" + core::to_file_name(dev->name) + ".bin", binaries[i]);
				}
#else
				log_error("opencl testing not supported on this platform (or disabled during floor compilation)");
#endif
			} break;
			case llvm_toolchain::TARGET::AIR: {
#if !defined(FLOOR_NO_METAL)
				auto ctx = make_shared<metal_compute>(floor::get_metal_whitelist());
				auto dev = ctx->get_device(compute_device::TYPE::FASTEST);
				if(dev == nullptr) {
					log_error("no device available!");
					break;
				}
				log_debug("using device: %s", dev->name);
				
				//
				if(option_ctx.test_bin) {
					program.valid = true;
					program.data_or_filename = option_ctx.test_bin_filename;
					
					if(!llvm_toolchain::create_floor_function_info(option_ctx.ffi_filename, program.functions, floor::get_metal_toolchain_version())) {
						log_error("failed to create floor function info from specified ffi file %s", option_ctx.ffi_filename);
						break;
					}
				}
				
				auto prog_entry = ctx->create_program_entry(dev, program, llvm_toolchain::TARGET::AIR);
				if(!prog_entry->valid) {
					log_error("program compilation failed!");
				}
				
				auto prog = ctx->create_metal_test_program(prog_entry);
				if(prog == nullptr) {
					log_error("device program compilation failed!");
				}
#else
				log_error("metal testing not supported on this platform (or disabled during floor compilation)");
#endif
			} break;
			case llvm_toolchain::TARGET::PTX: {
#if !defined(FLOOR_NO_CUDA)
				auto ctx = make_shared<cuda_compute>(floor::get_cuda_whitelist());
				auto dev = ctx->get_device(compute_device::TYPE::FASTEST);
				if(dev == nullptr) {
					log_error("no device available!");
					break;
				}
				log_debug("using device: %s", dev->name);
				
				//
				if(option_ctx.test_bin) {
					program.data_or_filename = "";
					if(!file_io::file_to_string(option_ctx.test_bin_filename, program.data_or_filename)) {
						log_error("failed to read test binary %s", option_ctx.test_bin_filename);
						break;
					}
					
					if(!llvm_toolchain::create_floor_function_info(option_ctx.ffi_filename, program.functions, floor::get_cuda_toolchain_version())) {
						log_error("failed to create floor function info from specified ffi file %s", option_ctx.ffi_filename);
						break;
					}
				}
				
				auto prog_entry = ctx->create_cuda_program((cuda_device*)dev.get(), program);
				if(!prog_entry.valid) {
					log_error("program compilation failed!");
				}
#else
				log_error("cuda testing not supported on this platform (or disabled during floor compilation)");
#endif
			} break;
			case llvm_toolchain::TARGET::SPIRV_VULKAN: {
#if !defined(FLOOR_NO_VULKAN)
				auto ctx = make_shared<vulkan_compute>(false, floor::get_vulkan_whitelist());
				auto dev = ctx->get_device(compute_device::TYPE::FASTEST);
				if(dev == nullptr) {
					log_error("no device available!");
					break;
				}
				log_debug("using device: %s", dev->name);
				
				//
				if(option_ctx.test_bin) {
					program.valid = true;
					program.data_or_filename = option_ctx.test_bin_filename;
					
					if(!llvm_toolchain::create_floor_function_info(option_ctx.ffi_filename, program.functions,
																   floor::get_vulkan_toolchain_version())) {
						log_error("failed to create floor function info from specified ffi file %s",
								  option_ctx.ffi_filename);
						break;
					}
				}
				
				// this will do both the initial vkCreateShaderModule and the pipeline/program creation (if compute)
				// NOTE: NVIDIA/Intel only do rudimentary checking in vkCreateShaderModule (if any at all?), while
				//       AMD does proper checking there -> to fully check everything, we need to actually create
				//       a pipeline (however, this can only be done for compute right now)
				vulkan_program::program_map_type prog_map;
				prog_map.insert_or_assign((vulkan_device*)dev.get(),
										  ctx->create_vulkan_program(dev, program));
				ctx->add_program(move(prog_map)); // will have already printed an error (if sth was wrong)
#else
				log_error("vulkan testing not supported on this platform (or disabled during floor compilation)");
#endif
			} break;
		}
	}
	
	return 0;
}

int main(int, char* argv[]) {
	// handle options
	option_context option_ctx;
	occ_opt_handler::parse_options(argv + 1, option_ctx);
	if(option_ctx.done) return 0;
			
	// preempt floor logger init
	logger::init(option_ctx.verbosity, false, false, true, true, "occ.txt");
	
	if(!floor::init(floor::init_state {
		.call_path = argv[0],
#if !defined(FLOOR_IOS)
		.data_path = option_ctx.config_path.c_str(),
#else
		.data_path = "data/",
#endif
		.console_only = true,
	})) {
		return -1;
	}
	
	int ret_code = 0;
	if (option_ctx.is_fubar_build) {
		string dst_archive_file_name = option_ctx.output_filename;
		if (dst_archive_file_name.empty()) {
			dst_archive_file_name = option_ctx.filename;
			
			const auto dot_pos = dst_archive_file_name.rfind('.');
			if (dot_pos != string::npos) {
				dst_archive_file_name = dst_archive_file_name.substr(0, dot_pos);
			}
			
			dst_archive_file_name += ".fubar";
		}
		
		const fubar::options_t options {
			.additional_cli_options = option_ctx.additional_options,
			.enable_warnings = option_ctx.warnings,
			.verbose_compile_output = (option_ctx.verbosity == size_t(logger::LOG_TYPE::UNDECORATED)),
			.cuda_max_registers = option_ctx.cuda_max_registers,
		};
		if (!fubar::build(option_ctx.fubar_target_set, option_ctx.fubar_target_set_file_name,
						  option_ctx.filename, dst_archive_file_name, options)) {
			ret_code = -42;
		}
	} else {
		ret_code = run_normal_build(option_ctx);
	}
	
	// kthxbye
	floor::destroy();
	return ret_code;
}
