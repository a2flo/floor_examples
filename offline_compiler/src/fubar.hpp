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

#pragma once

#include <floor/floor.hpp>
#include <floor/device/universal_binary.hpp>

namespace fl::fubar {

enum class TARGET_SET {
	USER_JSON,
	ALL,
	MINIMAL,
	GRAPHICS,
};

struct options_t {
	std::optional<std::string> additional_cli_options;
	std::optional<bool> enable_warnings;
	std::optional<bool> verbose_compile_output;
	std::optional<bool> enable_soft_printf;
	std::optional<bool> use_precompiled_header;
	std::optional<bool> compress_binaries;
	std::optional<bool> enable_assert;

	std::optional<uint32_t> cuda_max_registers;
	std::optional<bool> cuda_short_ptr;
	
	std::optional<bool> metal_restrictive_vectorization;
	
	std::optional<bool> emit_debug_info;
	std::optional<bool> preprocess_condense;
	std::optional<bool> preprocess_preserve_comments;
};

bool build(const TARGET_SET target_set,
		   const std::string& targets_json_file_name /* if TARGET_SET::USER_JSON */,
		   const std::string& options_json_file_name,
		   const std::string& src_file_name,
		   const std::string& dst_archive_file_name,
		   const options_t& options);

} // namespace fl::fubar
