/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include "function_info.hpp"
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
using namespace std;

namespace fl {

void dump_function_info(const toolchain::function_info& function_info, const toolchain::TARGET& target,
						const bool do_log_undecorated, const string prefix, const string suffix) {
	vector<pair<const toolchain::function_info*, uint32_t /* arg idx */>> arg_buffers_info;
	const auto dump_info = [&arg_buffers_info, &target, &do_log_undecorated, &prefix, &suffix](const toolchain::function_info& info,
																							   string info_prefix) {
		string info_str = "";
		for (size_t i = 0, count = info.args.size(); i < count; ++i) {
			switch (info.args[i].access) {
				case toolchain::ARG_ACCESS::READ:
					info_str += "read_only ";
					break;
				case toolchain::ARG_ACCESS::WRITE:
					info_str += "write_only ";
					break;
				case toolchain::ARG_ACCESS::READ_WRITE:
					info_str += "read_write ";
					break;
				default:
					if (info.args[i].address_space == toolchain::ARG_ADDRESS_SPACE::IMAGE) {
						log_error("image argument #$ has no access qualifier ($X)!", i, info.args[i].access);
					}
					break;
			}
			
			switch (info.args[i].address_space) {
				case toolchain::ARG_ADDRESS_SPACE::GLOBAL:
					info_str += "global ";
					break;
				case toolchain::ARG_ADDRESS_SPACE::LOCAL:
					info_str += "local ";
					break;
				case toolchain::ARG_ADDRESS_SPACE::CONSTANT:
					info_str += "constant ";
					break;
				case toolchain::ARG_ADDRESS_SPACE::IMAGE:
					info_str += "image ";
					break;
				default: break;
			}
			
			if (has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(info.args[i].flags)) {
				info_str += "argument_buffer ";
			}
			if (has_flag<toolchain::ARG_FLAG::STAGE_INPUT>(info.args[i].flags)) {
				info_str += "stage_input ";
			}
			if (has_flag<toolchain::ARG_FLAG::PUSH_CONSTANT>(info.args[i].flags)) {
				info_str += "push_constant ";
			}
			if (has_flag<toolchain::ARG_FLAG::SSBO>(info.args[i].flags)) {
				info_str += "ssbo ";
			}
			if (has_flag<toolchain::ARG_FLAG::IUB>(info.args[i].flags)) {
				info_str += "iub ";
			}
			
			if (info.args[i].is_array()) {
				if (has_flag<toolchain::ARG_FLAG::BUFFER_ARRAY>(info.args[i].flags)) {
					// implicitly SSBO
					if (target == toolchain::TARGET::SPIRV_VULKAN) {
						info_str += "ssbo-";
					}
					info_str += "buffer-";
				} else if (has_flag<toolchain::ARG_FLAG::IMAGE_ARRAY>(info.args[i].flags)) {
					info_str += "image-";
				}
				info_str += "array[" + to_string(info.args[i].array_extent) + "] ";
			}
			
			if (has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(info.args[i].flags)) {
				if (!info.args[i].argument_buffer_info) {
					log_error("argument #$ in $ is an argument buffer, but no argument buffer info exists!",
							  i, info.name);
				} else {
					arg_buffers_info.emplace_back(&info.args[i].argument_buffer_info.value(), i);
				}
			}
			
			if (info.args[i].address_space != toolchain::ARG_ADDRESS_SPACE::IMAGE) {
				info_str += to_string(info.args[i].size);
			} else {
				if (target == toolchain::TARGET::PTX) {
					// image type is not stored for ptx
					info_str += to_string(info.args[i].size);
				} else {
					switch(info.args[i].image_type) {
						case toolchain::ARG_IMAGE_TYPE::IMAGE_1D:
							info_str += "1D";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_1D_ARRAY:
							info_str += "1D array";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_1D_BUFFER:
							info_str += "1D buffer";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D:
							info_str += "2D";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY:
							info_str += "2D array";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D_DEPTH:
							info_str += "2D depth";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_DEPTH:
							info_str += "2D array depth";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D_MSAA:
							info_str += "2D msaa";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA:
							info_str += "2D array msaa";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D_MSAA_DEPTH:
							info_str += "2D msaa depth";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA_DEPTH:
							info_str += "2D array msaa depth";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_3D:
							info_str += "3D";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_CUBE:
							info_str += "cube";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_CUBE_ARRAY:
							info_str += "cube array";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_CUBE_DEPTH:
							info_str += "cube depth";
							break;
						case toolchain::ARG_IMAGE_TYPE::IMAGE_CUBE_ARRAY_DEPTH:
							info_str += "cube array depth";
							break;
						default:
							info_str += "unknown_type";
							log_error("kernel image argument #$ has no type or an unknown type ($X)!", i, info.args[i].image_type);
							break;
					}
				}
			}
			info_str += (i + 1 < count ? ", " : " ");
		}
		info_str = core::trim(info_str);
		string func_type_name;
		switch (info.type) {
			case toolchain::FUNCTION_TYPE::KERNEL:
				func_type_name = "function";
				break;
			case toolchain::FUNCTION_TYPE::VERTEX:
			case toolchain::FUNCTION_TYPE::FRAGMENT:
			case toolchain::FUNCTION_TYPE::TESSELLATION_CONTROL:
			case toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION:
				func_type_name = "shader";
				break;
			case toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT:
				func_type_name = "struct";
				break;
			case toolchain::FUNCTION_TYPE::NONE:
				break;
		}
		info_prefix = prefix + info_prefix;
#define log_with_type(str, log_type, ...) logger::log<logger::compute_arg_count(str)>(log_type, "", "", str, ##__VA_ARGS__)
		log_with_type("$'$ $: $ ($) [#args: $]$", (do_log_undecorated ? logger::LOG_TYPE::UNDECORATED : logger::LOG_TYPE::SIMPLE_MSG),
					  info_prefix,
					  info.type == toolchain::FUNCTION_TYPE::KERNEL ? "kernel" :
					  info.type == toolchain::FUNCTION_TYPE::VERTEX ? "vertex" :
					  info.type == toolchain::FUNCTION_TYPE::FRAGMENT ? "fragment" :
					  info.type == toolchain::FUNCTION_TYPE::TESSELLATION_CONTROL ? "tessellation-control" :
					  info.type == toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION ? "tessellation-evaluation" :
					  info.type == toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT ? "argument_buffer" : "unknown",
					  func_type_name, info.name, info_str, info.args.size(),
					  suffix);
#undef log_with_type
	};
	
	// dump the function itself first
	dump_info(function_info, "");
	
	// then dump any argument buffer info
	for (const auto& arg_buf_info : arg_buffers_info) {
		dump_info(*arg_buf_info.first, "-> @" + to_string(arg_buf_info.second) + " ");
	}
}

} // namespace fl
