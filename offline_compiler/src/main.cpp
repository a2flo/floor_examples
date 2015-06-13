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

#if !defined(FLOOR_NO_OPENCL)
#define FLOOR_OPENCL_INFO_FUNCS
#endif

#include <floor/floor/floor.hpp>
#include <floor/compute/llvm_compute.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/core/option_handler.hpp>

#include <floor/compute/opencl/opencl_common.hpp>
#include <floor/compute/opencl/opencl_compute.hpp>
#include <floor/compute/opencl/opencl_device.hpp>

#include <floor/compute/cuda/cuda_compute.hpp>
#include <floor/compute/cuda/cuda_device.hpp>

#if !defined(FLOOR_NO_METAL)
#define FLOOR_NO_METAL // don't need any specifics and this disables the obj-c code
#endif
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>

struct option_context {
	string filename { "" };
	string output_filename { "" };
	llvm_compute::TARGET target { llvm_compute::TARGET::SPIR };
	string sub_target;
	uint32_t bitness { 64 };
	bool double_support { true };
	bool test { false };
	bool test_bin { false };
	bool cuda_sass { false };
	string cuda_sass_filename { "" };
	string test_bin_filename { "" };
	string additional_options { "" };
	size_t verbosity { (size_t)logger::LOG_TYPE::WARNING_MSG };
	string config_path { "../../data/" };
	bool done { false };
};
typedef option_handler<option_context> occ_opt_handler;

//! option -> function map
template<> unordered_map<string, occ_opt_handler::option_function> occ_opt_handler::options {
	{ "--help", [](option_context& ctx, char**&) {
		cout << ("command line options:\n"
				 "\t--src <input-file>: the source file that should be compiled\n"
				 "\t--out <output-file>: the output file name (defaults to {spir.bc,cuda.ptx,metal.ll,applecl.bc})\n"
				 "\t--target [spir|ptx|air|applecl]: sets the compile target to OpenCL SPIR, CUDA PTX, Metal Apple-IR or Apple-OpenCL\n"
				 "\t--sub-target <name>: sets the target specific sub-target\n"
				 "\t    PTX:        [sm_20|sm_21|sm_30|sm_32|sm_35|sm_37|sm_50|sm_52|sm_53], defaults to sm_20\n"
				 "\t    Metal/AIR:  [ios8|ios9|osx11][_family], family is optional and defaults to lowest available\n"
				 "\t--bitness <32|64>: sets the bitness of the target (defaults to 64)\n"
				 "\t--no-double: explicitly disables double support (only SPIR and Apple-OpenCL)\n"
				 "\t--cuda-sass <output-file>: assembles a final device binary using ptxas and then disassembles it using cuobjdump (only PTX)\n"
				 "\t--test: tests/compiles the compiled binary on the target platform (if possible) - experimental!\n"
				 "\t--test-bin <input-file>: tests/compiles the specified binary on the target platform (if possible) - experimental!\n"
				 "\t-v: verbose output (DBG level)\n"
				 "\t-vv: very verbose output (MSG level)\n"
				 "\t--config <path>: the path where config.xml is located (defaults to \"../../data/\")\n"
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
	{ "--target", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
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
		else if(target == "applecl") {
			ctx.target = llvm_compute::TARGET::APPLECL;
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
	{ "--no-double", [](option_context& ctx, char**&) {
		ctx.double_support = false;
	}},
	{ "--test", [](option_context& ctx, char**&) {
		ctx.test = true;
	}},
	{ "--test-bin", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			cerr << "invalid argument!" << endl;
			return;
		}
		ctx.test_bin_filename = *arg_ptr;
		ctx.test_bin = true;
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
	{ "-v", [](option_context& ctx, char**&) {
		ctx.verbosity = (size_t)logger::LOG_TYPE::DEBUG_MSG;
	}},
	{ "-vv", [](option_context& ctx, char**&) {
		ctx.verbosity = (size_t)logger::LOG_TYPE::UNDECORATED;
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

int main(int, char* argv[]) {
	// handle options
	option_context option_ctx;
	occ_opt_handler::parse_options(argv + 1, option_ctx);
	if(option_ctx.done) return 0;
			
	// preempt floor logger init
	logger::init(option_ctx.verbosity, false, false, true, true, "occ.txt");
	
#if !defined(FLOOR_IOS)
	floor::init(argv[0], option_ctx.config_path.c_str(), true); // call path, data path, console-only
#else
	floor::init(argv[0], (const char*)"data/", true);
#endif
	
	pair<string, vector<llvm_compute::kernel_info>> program_data;
	if(!option_ctx.test_bin) {
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
						return -2;
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
			case llvm_compute::TARGET::AIR: {
				device = make_shared<metal_device>();
				metal_device* dev = (metal_device*)device.get();
				const auto family_pos = option_ctx.sub_target.rfind('_');
				uint32_t family = 0;
				if(family_pos != string::npos) {
					family = stou(option_ctx.sub_target.substr(family_pos + 1));
					option_ctx.sub_target = option_ctx.sub_target.substr(0, family_pos);
				}
				
				if(option_ctx.sub_target == "" || option_ctx.sub_target == "ios8") {
					dev->family = (family == 0 ? 1 : family);
					dev->family_version = 1;
				}
				else if(option_ctx.sub_target == "ios9") {
					dev->family = (family == 0 ? 1 : family);
					dev->family_version = 2;
				}
				else if(option_ctx.sub_target == "osx11") {
					dev->family = (family == 0 ? 10000 : family);
					dev->family_version = 1;
				}
				else {
					log_error("invalid AIR sub-target: %s", option_ctx.sub_target);
					return -3;
				}
				log_debug("compiling to AIR (family: %u, version: %u) ...", dev->family, dev->family_version);
			} break;
			case llvm_compute::TARGET::APPLECL:
				log_debug("compiling to APPLECL ...");
				device = make_shared<opencl_device>();
				break;
		}
		device->bitness = option_ctx.bitness;
		device->double_support = option_ctx.double_support;
		
		// compile
		program_data = llvm_compute::compile_program_file(device, option_ctx.filename, option_ctx.additional_options, option_ctx.target);
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
		
		// output
		if(option_ctx.output_filename == "-") {
			// print to console, as well as to file!
			cout << program_data.first << endl;
		}
		if(option_ctx.output_filename == "" || option_ctx.output_filename == "-") {
			option_ctx.output_filename = "unknown.bin";
			switch(option_ctx.target) {
				case llvm_compute::TARGET::SPIR: option_ctx.output_filename = "spir.bc"; break;
				case llvm_compute::TARGET::PTX: option_ctx.output_filename = "cuda.ptx"; break;
				case llvm_compute::TARGET::AIR: option_ctx.output_filename = "metal.ll"; break;
				case llvm_compute::TARGET::APPLECL: option_ctx.output_filename = "applecl.bc"; break;
			}
		}
		file_io::string_to_file(option_ctx.output_filename, program_data.first);
	}
	
	// handle cuda sass generation
	if(option_ctx.cuda_sass) {
		const auto dot_pos = option_ctx.output_filename.rfind(".");
		string cubin_filename = "";
		if(dot_pos != string::npos) cubin_filename = option_ctx.output_filename.substr(0, dot_pos);
		else cubin_filename = option_ctx.output_filename;
		
		const string ptxas_cmd {
			"ptxas -O3"s +
			(option_ctx.verbosity >= (size_t)logger::LOG_TYPE::DEBUG_MSG ? " -v" : "") +
			" -m" + (option_ctx.bitness == 32 ? "32" : "64") +
			" -arch " + option_ctx.sub_target +
			" -o " + cubin_filename + ".cubin " +
			option_ctx.output_filename
		};
		const string cuobjdump_cmd {
			"cuobjdump --dump-sass " + cubin_filename + ".cubin" +
			(option_ctx.cuda_sass_filename == "-" ? ""s : " > " + option_ctx.cuda_sass_filename)
		};
		string ptxas_output = "", cuobjdump_output = "";
		core::system(ptxas_cmd, ptxas_output);
		cout << ptxas_output << endl;
		core::system(cuobjdump_cmd, cuobjdump_output);
		cout << cuobjdump_output << endl;
	}

	// test the compiled binary (if this was specified)
	if(option_ctx.test || option_ctx.test_bin) {
		switch(option_ctx.target) {
			case llvm_compute::TARGET::SPIR:
			case llvm_compute::TARGET::APPLECL: {
#if !defined(FLOOR_NO_OPENCL)
				// have to create a proper opencl context to compile anything
				auto ctx = make_shared<opencl_compute>();
				ctx->init(false, stou(floor::get_opencl_platform()), false /* no opengl here */, floor::get_opencl_whitelist());
				auto cl_ctx = ctx->get_opencl_context();
				auto dev = ctx->get_device(compute_device::TYPE::FASTEST);
				auto cl_dev = ((opencl_device*)dev.get())->device_id;
				log_debug("using device: %s", dev->name);
				
				// pretty much copy&paste from opencl_compute::add_program(...) with some small changes (only build for 1 device):
				size_t binary_length;
				const unsigned char* binary_ptr;
				string binary_str = "";
				cl_int status = CL_SUCCESS;
				if(!option_ctx.test_bin) {
					binary_length = program_data.first.size();
					binary_ptr = (const unsigned char*)program_data.first.data();
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
				const cl_program program = clCreateProgramWithBinary(cl_ctx, 1, (const cl_device_id*)&cl_dev,
																	 &binary_length, &binary_ptr, &status, &create_err);
				if(create_err != CL_SUCCESS) {
					log_error("failed to create opencl program: %u", create_err);
					log_error("devices binary status: %s", status);
					return {};
				}
				else log_debug("successfully created opencl program!");
				
				// ... and build it
				CL_CALL_ERR_PARAM_RET(clBuildProgram(program, 1, (const cl_device_id*)&cl_dev, option_ctx.additional_options.c_str(), nullptr, nullptr),
									  build_err, "failed to build opencl program", {});
				log_debug("check"); logger::flush();
				
				
				// print out build log
				log_debug("build log: %s", cl_get_info<CL_PROGRAM_BUILD_LOG>(program, cl_dev));
				
				// print kernel info
				const auto kernel_count = cl_get_info<CL_PROGRAM_NUM_KERNELS>(program);
				const auto kernel_names_str = cl_get_info<CL_PROGRAM_KERNEL_NAMES>(program);
				log_debug("got %u kernels in program: %s", kernel_count, kernel_names_str);
				
				// retrieve the compiled binaries again
				const auto binaries = cl_get_info<CL_PROGRAM_BINARIES>(program);
				for(size_t i = 0; i < binaries.size(); ++i) {
					file_io::string_to_file("binary_" + to_string(i) + ".bin", binaries[i]);
				}
#else
				log_error("opencl testing not supported on this platform (or disabled during floor compilation)");
#endif
			} break;
			case llvm_compute::TARGET::AIR:
				log_error("metal/air testing not supported yet");
				break;
			case llvm_compute::TARGET::PTX:
				log_error("cuda/ptx testing not supported yet");
				break;
		}
	}
	
	// kthxbye
	floor::destroy();
	return 0;
}
