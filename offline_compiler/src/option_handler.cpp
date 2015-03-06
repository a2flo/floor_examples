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

#include "option_handler.hpp"
#include <floor/core/logger.hpp>
#include <unordered_map>
#include <stdexcept>

//! option -> function map
static unordered_map<string, option_handler::option_function> options {
	// already add all static options
	{ "--help", [](option_context&, char**&) {
		// TODO:
		log_undecorated("command line options:\n"
						"\t--src <file>: the source file that should be compiled\n"
						"\t--target [spir|ptx|air]: sets the compile target to OpenCL SPIR, CUDA PTX or Metal Apple-IR\n"
						"\t--subtarget <id>: sets the target specific sub-target\n"
						"\t--no-double: explicitly disables double support (only SPIR)\n");
	}},
	{ "--src", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			log_error("invalid argument!");
			return;
		}
		ctx.filename = *arg_ptr;
	}},
	{ "--target", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			log_error("invalid argument!");
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
		else {
			log_error("invalid target: %s", target);
			return;
		}
	}},
	{ "--subtarget", [](option_context& ctx, char**& arg_ptr) {
		++arg_ptr;
		if(*arg_ptr == nullptr || **arg_ptr == '-') {
			log_error("invalid argument!");
			return;
		}
		ctx.sub_target = *arg_ptr;
	}},
	{ "--no-double", [](option_context& ctx, char**&) {
		ctx.double_support = false;
	}},
};

void option_handler::parse_options(char* argv[], option_context& option_ctx) {
	static const auto& const_options = options;
	
	// parse compiler options
	auto arg_ptr = argv;
	for(; const auto arg = *arg_ptr; ++arg_ptr) {
		// all options must start with "-"
		if(arg[0] != '-') break;
		// special arg: "-": stdin/not an option
		else if(arg[1] == '\0') { // "-"
			break;
		}
		// special arg: "--": explicit end of options
		else if(arg[1] == '-' && arg[2] == '\0') {
			++arg_ptr;
			break;
		}
		
		// for all other options: call the registered function and provide the current context
		// if there is no function registered for an option, this will throw an "out_of_range" exception
		try {
			const_options.at(arg)(option_ctx, arg_ptr);
		}
		catch(out_of_range&) {
			log_error("unknown argument '%s'", arg);
		}
		catch(...) {
			log_error("caught unknown exception");
		}
	}
}

void option_handler::add_option(string option, option_handler::option_function func) {
	options.emplace(option, func);
}

void option_handler::add_options(initializer_list<pair<const string, option_handler::option_function>> options_) {
	options.insert(options_);
}
