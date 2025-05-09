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

#include <floor/device/toolchain.hpp>

namespace fl {

//! dumps the specified "function_info" to console,
//! requires "target" info for target-specific handling,
//! if "do_log_undecorated" is true log_undecorated() will be used instead of log_msg()
//! if "prefix"/"suffix" are set, the specified prefix/suffix will be attached to the log message
void dump_function_info(const toolchain::function_info& function_info, const toolchain::TARGET& target,
						const bool do_log_undecorated = false, const std::string prefix = "", const std::string suffix = "");

} // namespace fl
