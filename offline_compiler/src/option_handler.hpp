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

#ifndef __FLOOR_OPTION_HANDLER_HPP__
#define __FLOOR_OPTION_HANDLER_HPP__

#include <string>
#include <functional>
#include <floor/compute/llvm_compute.hpp>
using namespace std;

struct option_context {
	string filename { "" };
	llvm_compute::TARGET target { llvm_compute::TARGET::SPIR };
	string sub_target;
	bool double_support { true };
};

class option_handler {
public:
	//! parses the command line options and sets everything up inside the compiler
	static void parse_options(char* argv[], option_context& option_ctx);
	
	// use this to register any external options
	//! function callback typedef for an option (hint: use lambda expressions).
	//! NOTE: you have complete r/w access to the compiler context to set everything up
	typedef function<void(option_context&, char**& arg_ptr)> option_function;
	//! adds a single options
	static void add_option(string option, option_function func);
	//! adds multiple options at once (NOTE: use braces { ... })
	static void add_options(initializer_list<pair<const string, option_function>> options);
	
protected:
	// static class
	option_handler() = delete;
	~option_handler() = delete;
	option_handler& operator=(const option_handler&) = delete;
	
};

#endif
