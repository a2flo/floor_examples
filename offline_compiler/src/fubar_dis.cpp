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

#include "fubar_dis.hpp"
#include "function_info.hpp"
#include <floor/compute/universal_binary.hpp>
#include <floor/compute/spirv_handler.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/core.hpp>
#include <floor/floor/floor.hpp>

namespace fubar {

void disassemble(const string& archive_file_name) {
	auto ar_data = universal_binary::load_archive(archive_file_name);
	if (!ar_data) {
		log_error("failed to open or read archive: $", archive_file_name);
		return;
	}
	const universal_binary::archive& ar = *ar_data;
	
	log_undecorated("[FUBAR: $]:\nversion: $\n#binaries: $", archive_file_name,
					ar.header.static_header.binary_format_version,
					ar.header.static_header.binary_count);
	
	log_undecorated("target overview:");
	for (size_t tidx = 0, target_count = ar.header.static_header.binary_count; tidx < target_count; ++tidx) {
		const auto& target = ar.header.targets.at(tidx);
		const auto offset = ar.header.offsets.at(tidx);
		const auto toolchain_version = ar.header.toolchain_versions.at(tidx);
		const auto hash = ar.header.hashes.at(tidx);
		
		log_undecorated("\t[#$: $]", tidx, compute_type_to_string(target.common.type));
		log_undecorated("\t\ttarget format version: $", target.common.version);
		log_undecorated("\t\ttoolchain version: $", toolchain_version);
		log_undecorated("\t\tbinary offset: $", offset);
		
		stringstream hash_hex;
		hash_hex << hex << uppercase;
		for(uint32_t i = 0; i < 32; ++i) {
			if (hash.hash[i] < 0x10) {
				hash_hex << '0';
			}
			hash_hex << uint32_t(hash.hash[i]);
		}
		log_undecorated("\t\thash: $", hash_hex.str());
		
		switch (target.common.type) {
			case COMPUTE_TYPE::OPENCL: {
				const auto& cl = target.opencl;
				log_undecorated("\t\tOpenCL target: $.$", cl.major, cl.minor);
				log_undecorated("\t\tformat: $", cl.is_spir ? "SPIR" : "SPIR-V");

				string dev_target = "<invalid>";
				switch (cl.device_target) {
					case decltype(cl.device_target)::GENERIC:
						dev_target = "generic";
						break;
					case decltype(cl.device_target)::GENERIC_CPU:
						dev_target = "generic CPU";
						break;
					case decltype(cl.device_target)::INTEL_CPU:
						dev_target = "Intel CPU";
						break;
					case decltype(cl.device_target)::AMD_CPU:
						dev_target = "AMD CPU";
						break;
					case decltype(cl.device_target)::GENERIC_GPU:
						dev_target = "generic GPU";
						break;
					case decltype(cl.device_target)::INTEL_GPU:
						dev_target = "Intel GPU";
						break;
					case decltype(cl.device_target)::AMD_GPU:
						dev_target = "AMD GPU";
						break;
				}
				log_undecorated("\t\tdevice target: $", dev_target);
				
				log_undecorated("\t\timage-depth-support: $", cl.image_depth_support ? "yes" : "no");
				log_undecorated("\t\timage-msaa-support: $", cl.image_msaa_support ? "yes" : "no");
				log_undecorated("\t\timage-mipmap-support: $", cl.image_mipmap_support ? "yes" : "no");
				log_undecorated("\t\timage-mipmap-write-support: $", cl.image_mipmap_write_support ? "yes" : "no");
				log_undecorated("\t\timage-read-write-support: $", cl.image_read_write_support ? "yes" : "no");
				log_undecorated("\t\tdouble-support: $", cl.double_support ? "yes" : "no");
				log_undecorated("\t\tbasic-64-bit-atomics-support: $", cl.basic_64_bit_atomics_support ? "yes" : "no");
				log_undecorated("\t\textended-64-bit-atomics-support: $", cl.extended_64_bit_atomics_support ? "yes" : "no");
				log_undecorated("\t\tsub-group-support: $", cl.sub_group_support ? "yes" : "no");
				log_undecorated("\t\tSIMD width: $", cl.simd_width);
				break;
			}
			case COMPUTE_TYPE::CUDA: {
				const auto& cu = target.cuda;
				log_undecorated("\t\tCUDA sm target: $.$", cu.sm_major, cu.sm_minor);
				log_undecorated("\t\tCUDA PTX target: $.$", cu.ptx_isa_major, cu.ptx_isa_minor);
				
				log_undecorated("\t\tPTX: $", cu.is_ptx ? "yes" : "no");
				log_undecorated("\t\timage-depth-compare-support: $", cu.image_depth_compare_support ? "yes" : "no");
				log_undecorated("\t\tmax registers: $", cu.max_registers);
				break;
			}
			case COMPUTE_TYPE::METAL: {
				const auto& mtl = target.metal;
				log_undecorated("\t\tMetal target: $.$", mtl.major, mtl.minor);
				
				string platform_target = "<invalid>";
				switch (mtl.platform_target) {
					case decltype(mtl.platform_target)::MACOS:
						platform_target = "macOS";
						break;
					case decltype(mtl.platform_target)::IOS:
						platform_target = "iOS";
						break;
					case decltype(mtl.platform_target)::VISIONOS:
						platform_target = "visionOS";
						break;
				}
				log_undecorated("\t\tplatform target: $", platform_target);
				
				string dev_target = "<invalid>";
				switch (mtl.device_target) {
					case decltype(mtl.device_target)::GENERIC:
						dev_target = "generic";
						break;
					case decltype(mtl.device_target)::APPLE:
						dev_target = "Apple GPU";
						break;
					case decltype(mtl.device_target)::AMD:
						dev_target = "AMD GPU";
						break;
					case decltype(mtl.device_target)::INTEL:
						dev_target = "Intel GPU";
						break;
				}
				log_undecorated("\t\tdevice target: $", dev_target);
				
				log_undecorated("\t\tbarycentric-coord-support: $", mtl.barycentric_coord_support ? "yes" : "no");
				log_undecorated("\t\tsoft-printf: $", mtl.soft_printf ? "yes" : "no");
				log_undecorated("\t\tSIMD width: $", mtl.simd_width);
				break;
			}
			case COMPUTE_TYPE::HOST: {
				const auto& hst = target.host;
				log_undecorated("\t\tHost CPU target: $", host_cpu_tier_to_string(hst.cpu_tier));
				break;
			}
			case COMPUTE_TYPE::VULKAN: {
				const auto& vk = target.vulkan;
				log_undecorated("\t\tVulkan target: $.$", vk.vulkan_major, vk.vulkan_minor);
				log_undecorated("\t\tSPIR-V target: $.$", vk.spirv_major, vk.spirv_minor);
				
				string dev_target = "<invalid>";
				switch (vk.device_target) {
					case decltype(vk.device_target)::GENERIC:
						dev_target = "generic";
						break;
					case decltype(vk.device_target)::NVIDIA:
						dev_target = "NVIDIA GPU";
						break;
					case decltype(vk.device_target)::AMD:
						dev_target = "AMD GPU";
						break;
					case decltype(vk.device_target)::INTEL:
						dev_target = "Intel GPU";
						break;
				}
				log_undecorated("\t\tdevice target: $", dev_target);
				
				log_undecorated("\t\tdouble-support: $", vk.double_support ? "yes" : "no");
				log_undecorated("\t\tbasic-64-bit-atomics-support: $", vk.basic_64_bit_atomics_support ? "yes" : "no");
				log_undecorated("\t\textended-64-bit-atomics-support: $", vk.extended_64_bit_atomics_support ? "yes" : "no");
				log_undecorated("\t\tsoft-printf: $", vk.soft_printf ? "yes" : "no");
				log_undecorated("\t\tbasic-32-bit-float-atomics-support: $", vk.basic_32_bit_float_atomics_support ? "yes" : "no");
				log_undecorated("\t\tprimitive-id-support: $", vk.primitive_id_support ? "yes" : "no");
				log_undecorated("\t\tbarycentric-coord-support: $", vk.barycentric_coord_support ? "yes" : "no");
				log_undecorated("\t\ttessellation-support: $", vk.tessellation_support ? "yes" : "no");
				log_undecorated("\t\tdescriptor-buffer-support: $", vk.descriptor_buffer_support ? "yes" : "no");
				log_undecorated("\t\tSIMD width: $", vk.simd_width);
				break;
			}
			default:
				log_error("invalid target type: $", (uint32_t)target.common.type);
				continue;
		}
		
		log_undecorated("");
	}
	
	log_undecorated("binaries:");
	for (size_t tidx = 0, target_count = ar.header.static_header.binary_count; tidx < target_count; ++tidx) {
		const auto& target = ar.header.targets.at(tidx);
		const auto& bin = ar.binaries.at(tidx);
		
		llvm_toolchain::TARGET toolchain_target {};
		switch (target.common.type) {
			case COMPUTE_TYPE::CUDA:
				toolchain_target = llvm_toolchain::TARGET::PTX;
				break;
			case COMPUTE_TYPE::OPENCL:
				toolchain_target = (target.opencl.is_spir ? llvm_toolchain::TARGET::SPIR : llvm_toolchain::TARGET::SPIRV_OPENCL);
				break;
			case COMPUTE_TYPE::METAL:
				toolchain_target = llvm_toolchain::TARGET::AIR;
				break;
			case COMPUTE_TYPE::VULKAN:
				toolchain_target = llvm_toolchain::TARGET::SPIRV_VULKAN;
				break;
			case COMPUTE_TYPE::HOST:
				toolchain_target = llvm_toolchain::TARGET::HOST_COMPUTE_CPU;
				break;
			case COMPUTE_TYPE::NONE:
				continue;
		}
		
		log_undecorated("[#$: $]", tidx, compute_type_to_string(target.common.type));
		// NOTE: this contains both the function info, but also determine the actual function count
		const auto func_info = universal_binary::translate_function_info(bin.function_info);
		log_undecorated("#functions: $", func_info.size());
		for (size_t info_idx = 0, fidx = 0, func_count = bin.static_binary_header.function_count; fidx < func_count; ++fidx) {
			const auto& func = bin.function_info.at(fidx);
			if (func.static_function_info.type == llvm_toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT) {
				continue;
			}
			assert(info_idx < func_info.size());
			dump_function_info(func_info[info_idx++], toolchain_target, true, " * ");
		}
		log_undecorated("");
		
		// tmp file creation handling
		struct tmp_file_raii_t {
			string file_name;
			bool is_valid() const {
				return !file_name.empty();
			}
			~tmp_file_raii_t() {
				if (is_valid()) {
					// remove tmp file again
					error_code ec {};
					std::filesystem::remove(file_name, ec);
				}
			}
		};
		const auto make_tmp_bin = [](const string& file_name_prefix, const string& file_name_ending,
									 std::span<const uint8_t> data) -> tmp_file_raii_t {
			auto mod_name = core::create_tmp_file_name(file_name_prefix, file_name_ending);
			if (!file_io::buffer_to_file(mod_name, (const char*)data.data(), data.size_bytes())) {
				log_error("failed to create/write tmp file: $", mod_name);
				return {};
			}
			return { mod_name };
		};
		
		if (target.common.type == COMPUTE_TYPE::VULKAN && floor::has_vulkan_toolchain()) {
			auto container = spirv_handler::load_container_from_memory(bin.data.data(), bin.data.size(),
																	   archive_file_name);
			if (!container.valid || !container.spirv_data) {
				continue;
			}
			
			uint32_t mod_idx = 0;
			for (const auto& entry : container.entries) {
				const span<const uint8_t> mod_data {
					(const uint8_t*)&container.spirv_data[entry.data_offset],
					entry.data_word_count * sizeof(uint32_t)
				};
				const auto cur_mod_idx = mod_idx++;
				
				auto tmp_file = make_tmp_bin("tmp_vk_target_" + to_string(tidx) + "_mod_" + to_string(cur_mod_idx), ".spv", mod_data);
				if (!tmp_file.is_valid()) {
					continue;
				}
				
				// disassemble
				const string dis_cmd = floor::get_vulkan_spirv_dis() + " --debug-asm --color --comment -o - " + tmp_file.file_name;
				string dis_output;
				core::system(dis_cmd, dis_output);
				log_undecorated("SPIR-V module #$:\n$", cur_mod_idx, dis_output);
			}
		} else if (target.common.type == COMPUTE_TYPE::METAL && floor::has_metal_toolchain()) {
			auto tmp_file = make_tmp_bin("tmp_mtl_target_" + to_string(tidx), ".metallib", bin.data);
			if (!tmp_file.is_valid()) {
				continue;
			}
			
			// disassemble
			const string dis_cmd = floor::get_metallib_dis() + " --color -o - " + tmp_file.file_name;
			string dis_output;
			core::system(dis_cmd, dis_output);
			log_undecorated("metallib data:\n$", dis_output);
		} else if (target.common.type == COMPUTE_TYPE::CUDA) {
			if (target.cuda.is_ptx) {
				const string ptx((const char*)bin.data.data(), bin.data.size());
				log_undecorated("CUDA PTX:\n$", ptx);
			}
			// else: unimplemented
		} else if (target.common.type == COMPUTE_TYPE::OPENCL && floor::has_opencl_toolchain()) {
			if (target.opencl.is_spir) {
				auto tmp_file = make_tmp_bin("tmp_cl_target_" + to_string(tidx), ".ll", bin.data);
				if (!tmp_file.is_valid()) {
					continue;
				}
				
				// disassemble
				const string dis_cmd = floor::get_opencl_dis() + " --color -o - " + tmp_file.file_name;
				string dis_output;
				core::system(dis_cmd, dis_output);
				log_undecorated("OpenCL SPIR:\n$", dis_output);
			} else {
				auto tmp_file = make_tmp_bin("tmp_cl_target_" + to_string(tidx), ".spv", bin.data);
				if (!tmp_file.is_valid()) {
					continue;
				}
				
				// disassemble
				const string dis_cmd = floor::get_opencl_spirv_dis() + " --debug-asm --color --comment -o - " + tmp_file.file_name;
				string dis_output;
				core::system(dis_cmd, dis_output);
				log_undecorated("OpenCL SPIR-V:\n$", dis_output);
			}
		} else if (target.common.type == COMPUTE_TYPE::HOST && floor::has_host_toolchain()) {
			auto tmp_file = make_tmp_bin("tmp_host_target_" + to_string(tidx), ".bin", bin.data);
			if (!tmp_file.is_valid()) {
				continue;
			}
			
			// disassemble
			const string dis_cmd = floor::get_host_objdump() + " -d --demangle " + tmp_file.file_name;
			string dis_output;
			core::system(dis_cmd, dis_output);
			log_undecorated("Host-Compute:\n$", dis_output);
		}
		
		log_undecorated("");
	}
}

} // namespace fubar
