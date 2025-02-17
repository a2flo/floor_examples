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

#pragma once

#include <floor/floor/floor.hpp>
#include <floor/compute/universal_binary.hpp>

namespace fubar {

enum class TARGET_SET {
	USER_JSON,
	ALL,
	MINIMAL,
	GRAPHICS,
};

struct options_t {
	optional<string> additional_cli_options;
	optional<bool> enable_warnings;
	optional<bool> verbose_compile_output;
	optional<bool> enable_soft_printf;
	optional<bool> use_precompiled_header;
	optional<bool> compress_binaries;
	optional<bool> enable_assert;

	optional<uint32_t> cuda_max_registers;
	optional<bool> cuda_short_ptr;
	
	optional<bool> emit_debug_info;
	optional<bool> preprocess_condense;
	optional<bool> preprocess_preserve_comments;
};

bool build(const TARGET_SET target_set,
		   const string& targets_json_file_name /* if TARGET_SET::USER_JSON */,
		   const string& options_json_file_name,
		   const string& src_file_name,
		   const string& dst_archive_file_name,
		   const options_t& options);

} // namespace fubar
