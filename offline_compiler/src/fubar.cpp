/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
#include <floor/core/json.hpp>

namespace fubar {
	using namespace json;

	static universal_binary::target parse_cuda_target(const json_object& target_obj) {
		const auto sm_arr = target_obj.at("sm").get_or_throw<json_array>();
		if (sm_arr.size() != 2) {
			throw runtime_error("invalid 'sm' entry");
		}
		const uint2 sm {
			sm_arr[0].get_or_throw<uint32_t>(),
			sm_arr[1].get_or_throw<uint32_t>(),
		};
		
		const auto ptx_arr = target_obj.at("ptx").get_or_throw<json_array>();
		if (ptx_arr.size() != 2) {
			throw runtime_error("invalid 'ptx' entry");
		}
		const uint2 ptx {
			ptx_arr[0].get_or_throw<uint32_t>(),
			ptx_arr[1].get_or_throw<uint32_t>(),
		};
		
		return {
			.cuda = {
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::CUDA,
				.sm_major = sm.x,
				.sm_minor = sm.y,
				.ptx_isa_major = ptx.x,
				.ptx_isa_minor = ptx.y,
				.is_ptx = target_obj.at("is_ptx").get_or_throw<bool>(),
				.image_depth_compare_support = target_obj.at("image_depth_compare_support").get_or_throw<bool>(),
				._unused = 0,
			}
		};
	}

	static universal_binary::target parse_host_target(const json_object& target_obj) {
		const auto arch = target_obj.at("arch").get_or_throw<string>();
		const auto tier = target_obj.at("tier").get_or_throw<uint32_t>();
		auto cpu_tier = HOST_CPU_TIER::X86_TIER_1;
		if (arch == "x86") {
			switch (tier) {
				case 1:
					cpu_tier = HOST_CPU_TIER::X86_TIER_1;
					break;
				case 2:
					cpu_tier = HOST_CPU_TIER::X86_TIER_2;
					break;
				case 3:
					cpu_tier = HOST_CPU_TIER::X86_TIER_3;
					break;
				case 4:
					cpu_tier = HOST_CPU_TIER::X86_TIER_4;
					break;
				default:
					throw runtime_error("invalid x86 Tier: " + to_string(tier));
			}
		} else if (arch == "ARM") {
			switch (tier) {
				case 1:
					cpu_tier = HOST_CPU_TIER::ARM_TIER_1;
					break;
				case 2:
					cpu_tier = HOST_CPU_TIER::ARM_TIER_2;
					break;
				default:
					throw runtime_error("invalid ARM Tier: " + to_string(tier));
			}
		} else {
			throw runtime_error("unknown arch: " + arch);
		}
		
		return {
			.host = {
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::HOST,
				.cpu_tier = cpu_tier,
				._unused = 0,
			}
		};
	}

	static universal_binary::target parse_metal_target(const json_object& target_obj) {
		const auto version_arr = target_obj.at("version").get_or_throw<json_array>();
		if (version_arr.size() != 2) {
			throw runtime_error("invalid 'version' entry");
		}
		const uint2 version {
			version_arr[0].get_or_throw<uint32_t>(),
			version_arr[1].get_or_throw<uint32_t>(),
		};
		
		const auto target_str = target_obj.at("target").get_or_throw<string>();
		decltype(universal_binary::target_v2::metal)::DEVICE_TARGET target {};
		if (target_str == "generic") {
			target = decltype(universal_binary::target_v2::metal)::GENERIC;
		} else if (target_str == "nvidia") {
			target = decltype(universal_binary::target_v2::metal)::NVIDIA;
		} else if (target_str == "amd") {
			target = decltype(universal_binary::target_v2::metal)::AMD;
		} else if (target_str == "intel") {
			target = decltype(universal_binary::target_v2::metal)::INTEL;
		} else {
			throw runtime_error("unknown target: " + target_str);
		}
		
		return {
			.metal = {
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::METAL,
				.major = version.x,
				.minor = version.y,
				.is_ios = target_obj.at("is_ios").get_or_throw<bool>(),
				.device_target = target,
				.simd_width = target_obj.at("simd_width").get_or_throw<uint32_t>(),
				._unused = 0,
			}
		};
	}

	static universal_binary::target parse_opencl_target(const json_object& target_obj) {
		const auto version_arr = target_obj.at("version").get_or_throw<json_array>();
		if (version_arr.size() != 2) {
			throw runtime_error("invalid 'version' entry");
		}
		const uint2 version {
			version_arr[0].get_or_throw<uint32_t>(),
			version_arr[1].get_or_throw<uint32_t>(),
		};
		
		const auto target_str = target_obj.at("target").get_or_throw<string>();
		decltype(universal_binary::target_v2::opencl)::DEVICE_TARGET target {};
		if (target_str == "generic") {
			target = decltype(universal_binary::target_v2::opencl)::GENERIC;
		} else if (target_str == "generic_cpu") {
			target = decltype(universal_binary::target_v2::opencl)::GENERIC_CPU;
		} else if (target_str == "generic_gpu") {
			target = decltype(universal_binary::target_v2::opencl)::GENERIC_GPU;
		} else if (target_str == "intel_cpu") {
			target = decltype(universal_binary::target_v2::opencl)::INTEL_CPU;
		} else if (target_str == "intel_gpu") {
			target = decltype(universal_binary::target_v2::opencl)::INTEL_GPU;
		} else if (target_str == "amd_cpu") {
			target = decltype(universal_binary::target_v2::opencl)::AMD_CPU;
		} else if (target_str == "amd_gpu") {
			target = decltype(universal_binary::target_v2::opencl)::AMD_GPU;
		} else {
			throw runtime_error("unknown target: " + target_str);
		}
		
		return {
			.opencl = {
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::OPENCL,
				.major = version.x,
				.minor = version.y,
				.is_spir = target_obj.at("is_spir").get_or_throw<bool>(),
				.device_target = target,
				.image_depth_support = target_obj.at("image_depth_support").get_or_throw<bool>(),
				.image_msaa_support = target_obj.at("image_msaa_support").get_or_throw<bool>(),
				.image_mipmap_support = target_obj.at("image_mipmap_support").get_or_throw<bool>(),
				.image_mipmap_write_support = target_obj.at("image_mipmap_write_support").get_or_throw<bool>(),
				.image_read_write_support = target_obj.at("image_read_write_support").get_or_throw<bool>(),
				.double_support = target_obj.at("double_support").get_or_throw<bool>(),
				.basic_64_bit_atomics_support = target_obj.at("basic_64_bit_atomics_support").get_or_throw<bool>(),
				.extended_64_bit_atomics_support = target_obj.at("extended_64_bit_atomics_support").get_or_throw<bool>(),
				.sub_group_support = target_obj.at("sub_group_support").get_or_throw<bool>(),
				.simd_width = target_obj.at("simd_width").get_or_throw<uint32_t>(),
				._unused = 0,
			}
		};
	}

	static universal_binary::target parse_vulkan_target(const json_object& target_obj) {
		const auto version_arr = target_obj.at("version").get_or_throw<json_array>();
		if (version_arr.size() != 2) {
			throw runtime_error("invalid 'version' entry");
		}
		const uint2 version {
			version_arr[0].get_or_throw<uint32_t>(),
			version_arr[1].get_or_throw<uint32_t>(),
		};
		
		const auto spirv_arr = target_obj.at("spirv").get_or_throw<json_array>();
		if (spirv_arr.size() != 2) {
			throw runtime_error("invalid 'spirv' entry");
		}
		const uint2 spirv {
			spirv_arr[0].get_or_throw<uint32_t>(),
			spirv_arr[1].get_or_throw<uint32_t>(),
		};
		
		const auto target_str = target_obj.at("target").get_or_throw<string>();
		decltype(universal_binary::target_v2::vulkan)::DEVICE_TARGET target {};
		if (target_str == "generic") {
			target = decltype(universal_binary::target_v2::vulkan)::GENERIC;
		} else if (target_str == "nvidia") {
			target = decltype(universal_binary::target_v2::vulkan)::NVIDIA;
		} else if (target_str == "amd") {
			target = decltype(universal_binary::target_v2::vulkan)::AMD;
		} else if (target_str == "intel") {
			target = decltype(universal_binary::target_v2::vulkan)::INTEL;
		} else {
			throw runtime_error("unknown target: " + target_str);
		}
		
		return {
			.vulkan = {
				.version = universal_binary::target_format_version,
				.type = COMPUTE_TYPE::VULKAN,
				.vulkan_major = version.x,
				.vulkan_minor = version.y,
				.spirv_major = spirv.x,
				.spirv_minor = spirv.y,
				.device_target = target,
				.double_support = target_obj.at("double_support").get_or_throw<bool>(),
				.basic_64_bit_atomics_support = target_obj.at("basic_64_bit_atomics_support").get_or_throw<bool>(),
				.extended_64_bit_atomics_support = target_obj.at("extended_64_bit_atomics_support").get_or_throw<bool>(),
				._unused = 0,
			}
		};
	}

	static vector<universal_binary::target> get_json_targets(const string& targets_json_file_name) {
		try {
			auto doc = create_document(targets_json_file_name);
			if (!doc.valid) {
				throw runtime_error("invalid JSON");
			}
			const auto targets_arr = doc.root.get_or_throw<json_array>();
			vector<universal_binary::target_v2> targets;
			for (const auto& target_entry : targets_arr) {
				const auto target_obj = target_entry.get_or_throw<json_object>();
				const auto target_type = target_obj.at("type").get_or_throw<string>();
				if (target_type == "CUDA") {
					targets.emplace_back(parse_cuda_target(target_obj));
				} else if (target_type == "Host") {
					targets.emplace_back(parse_host_target(target_obj));
				} else if (target_type == "Metal") {
					targets.emplace_back(parse_metal_target(target_obj));
				} else if (target_type == "OpenCL") {
					targets.emplace_back(parse_opencl_target(target_obj));
				} else if (target_type == "Vulkan") {
					targets.emplace_back(parse_vulkan_target(target_obj));
				} else {
					throw runtime_error("unknown target: " + target_type);
				}
			}
			return targets;
		} catch (exception& exc) {
			log_error("failed to parse targets JSON: $", exc.what());
		}
		return {};
	}
	
	static vector<universal_binary::target_v2> get_targets(const TARGET_SET target_set,
														   const string& targets_json_file_name,
														   const llvm_toolchain::compile_options& options) {
		vector<universal_binary::target_v2> ret_targets;
		if (target_set == TARGET_SET::USER_JSON) {
			ret_targets = get_json_targets(targets_json_file_name);
			if (ret_targets.empty()) {
				return {};
			}
		}
		
		static const vector<universal_binary::target> minimal_targets {
#if 1
			// CUDA
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 3,
					.sm_minor = 0,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 3,
					.sm_minor = 5,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 5,
					.sm_minor = 0,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 5,
					.sm_minor = 2,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 6,
					.sm_minor = 0,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 6,
					.sm_minor = 1,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 7,
					.sm_minor = 0,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 7,
					.sm_minor = 5,
					.ptx_isa_major = 6,
					.ptx_isa_minor = 3,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 8,
					.sm_minor = 0,
					.ptx_isa_major = 7,
					.ptx_isa_minor = 0,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
			{
				.cuda = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::CUDA,
					.sm_major = 8,
					.sm_minor = 6,
					.ptx_isa_major = 7,
					.ptx_isa_minor = 1,
					.is_ptx = 1,
					.image_depth_compare_support = 0,
					._unused = 0,
				}
			},
#endif
#if 1
			// Metal
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 0,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 0,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 1,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 1,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 2,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 2,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 3,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 3,
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
				.opencl = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::OPENCL,
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
				.vulkan = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::VULKAN,
					.vulkan_major = 1,
					.vulkan_minor = 2,
					.spirv_major = 1,
					.spirv_minor = 5,
					.device_target = decltype(universal_binary::target_v2::vulkan)::GENERIC,
					.double_support = 0,
					.basic_64_bit_atomics_support = 0,
					.extended_64_bit_atomics_support = 0,
					._unused = 0,
				}
			},
#endif
#if 1
			// Host-Compute
			{
				.host = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::HOST,
					.cpu_tier = HOST_CPU_TIER::X86_TIER_1,
					._unused = 0,
				}
			},
			{
				.host = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::HOST,
					.cpu_tier = HOST_CPU_TIER::X86_TIER_2,
					._unused = 0,
				}
			},
			{
				.host = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::HOST,
					.cpu_tier = HOST_CPU_TIER::X86_TIER_3,
					._unused = 0,
				}
			},
			{
				.host = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::HOST,
					.cpu_tier = HOST_CPU_TIER::X86_TIER_4,
					._unused = 0,
				}
			},
#if 0 // TODO: enable this once ARM is supported
			{
				.host = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::HOST,
					.cpu_tier = HOST_CPU_TIER::ARM_TIER_1,
					._unused = 0,
				}
			},
			{
				.host = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::HOST,
					.cpu_tier = HOST_CPU_TIER::ARM_TIER_2,
					._unused = 0,
				}
			},
#endif
#endif
		};
		
		static const vector<universal_binary::target> graphics_targets {
#if 1
			// Metal
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 0,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 0,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 1,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 1,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 2,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 2,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 3,
					.is_ios = 0,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
			{
				.metal = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::METAL,
					.major = 2,
					.minor = 3,
					.is_ios = 1,
					.device_target = decltype(universal_binary::target_v2::metal)::GENERIC,
					.simd_width = 0,
					._unused = 0,
				}
			},
#endif
#if 1
			// Vulkan
			{
				.vulkan = {
					.version = universal_binary::target_format_version,
					.type = COMPUTE_TYPE::VULKAN,
					.vulkan_major = 1,
					.vulkan_minor = 2,
					.spirv_major = 1,
					.spirv_minor = 5,
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
		} else if (target_set == TARGET_SET::GRAPHICS) {
			ret_targets = graphics_targets;
		}
		
		// overwrite options if specified/necessary
		for (auto& target : ret_targets) {
			if (target.common.type == COMPUTE_TYPE::CUDA && target.cuda.max_registers == 0) {
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
			.metal.soft_printf = options.enable_soft_printf,
			.vulkan.soft_printf = options.enable_soft_printf,
		};
		
		const auto targets = get_targets(target_set, targets_json_file_name, toolchain_options);
		if (targets.empty()) return false;
		
		return universal_binary::build_archive_from_file(src_file_name, dst_archive_file_name,
														 toolchain_options, targets,
														 options.use_precompiled_header);
	}
	
}
