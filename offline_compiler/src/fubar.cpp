/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include "fubar.hpp"

namespace fubar {
	static vector<universal_binary::target_v2> get_json_targets(const string& targets_json_file_name floor_unused) {
		// TODO: implement this
		return {};
	}
	
	static vector<universal_binary::target_v2> get_targets(const TARGET_SET target_set,
														   const string& targets_json_file_name,
														   const llvm_toolchain::compile_options& options) {
		vector<universal_binary::target_v2> ret_targets;
		if (target_set == TARGET_SET::USER_JSON) {
			ret_targets = get_json_targets(targets_json_file_name);
		}
		
		static const vector<universal_binary::target> minimal_targets {
#if 1
			// CUDA
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 3,
					.sm_minor = 0,
					.ptx_isa_major = 4,
					.ptx_isa_minor = 3,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 3,
					.sm_minor = 5,
					.ptx_isa_major = 4,
					.ptx_isa_minor = 3,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 5,
					.sm_minor = 0,
					.ptx_isa_major = 4,
					.ptx_isa_minor = 3,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 5,
					.sm_minor = 2,
					.ptx_isa_major = 4,
					.ptx_isa_minor = 3,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 6,
					.sm_minor = 0,
					.ptx_isa_major = 5,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 6,
					.sm_minor = 1,
					.ptx_isa_major = 5,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 7,
					.sm_minor = 0,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.cuda = {
					.sm_major = 7,
					.sm_minor = 5,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 3,
					.is_ptx = 1,
					.image_depth_compare_support = 1,
					._unused = 0,
				}
			},
#endif
#if 1
			// Metal
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::METAL,
				.metal = {
					.major = 1,
					.minor = 1,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::METAL,
				.metal = {
					.major = 1,
					.minor = 1,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::METAL,
				.metal = {
					.major = 2,
					.minor = 0,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::METAL,
				.metal = {
					.major = 2,
					.minor = 0,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::METAL,
				.metal = {
					.major = 2,
					.minor = 1,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::METAL,
				.metal = {
					.major = 2,
					.minor = 1,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
#endif
#if 1
			// OpenCL
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::OPENCL,
				.opencl = {
					.major = 1,
					.minor = 2,
					.is_spir = 1,
					.device_target = decltype(universal_binary::target_v2::opencl)::GENERIC,
					.image_depth_support = 0,
					.image_msaa_support = 0,
					.image_mipmap_support = 0,
					.image_mipmap_write_support = 0,
					.image_read_write_support = 0,
					.double_support = 0,
					.basic_64_bit_atomics_support = 0,
					.extended_64_bit_atomics_support = 0,
					.sub_group_support = 0,
					.simd_width = 0,
					._unused = 0,
				}
			},
#endif
#if 1
			// Vulkan
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::VULKAN,
				.vulkan = {
					.vulkan_major = 1,
					.vulkan_minor = 0,
					.spirv_major = 1,
					.spirv_minor = 0,
					.device_target = decltype(universal_binary::target_v2::vulkan)::GENERIC,
					.double_support = 0,
					.basic_64_bit_atomics_support = 0,
					.extended_64_bit_atomics_support = 0,
					._unused = 0,
				}
			},
			{
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::VULKAN,
				.vulkan = {
					.vulkan_major = 1,
					.vulkan_minor = 1,
					.spirv_major = 1,
					.spirv_minor = 3,
					.device_target = decltype(universal_binary::target_v2::vulkan)::GENERIC,
					.double_support = 0,
					.basic_64_bit_atomics_support = 0,
					.extended_64_bit_atomics_support = 0,
					._unused = 0,
				}
			},
#endif
		};
		
		if (target_set == TARGET_SET::MINIMAL) {
			ret_targets = minimal_targets;
		} else {
			//return all_targets; // TODO: implement this
			ret_targets = minimal_targets;
		}
		
		// overwrite options if specified/necessary
		for (auto& target : ret_targets) {
			if (target.type == COMPUTE_TYPE::CUDA && target.cuda.max_registers == 0) {
				target.cuda.max_registers = options.cuda.max_registers;
			}
		}
		
		return ret_targets;
	}
	
	bool build(const TARGET_SET target_set,
			   const string& targets_json_file_name,
			   const string& src_file_name,
			   const string& dst_archive_file_name,
			   const options_t& options) {
		llvm_toolchain::compile_options toolchain_options {
			.cli = options.additional_cli_options,
			.enable_warnings = options.enable_warnings,
			.silence_debug_output = !options.verbose_compile_output,
			.cuda.max_registers = options.cuda_max_registers,
		};
		
		const auto targets = get_targets(target_set, targets_json_file_name, toolchain_options);
		if (targets.empty()) return false;
		
		return universal_binary::build_archive_from_file(src_file_name, dst_archive_file_name,
														 toolchain_options, targets);
	}
	
}
