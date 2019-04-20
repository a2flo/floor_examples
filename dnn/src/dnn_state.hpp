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

#ifndef __FLOOR_DNN_DNN_STATE_HPP__
#define __FLOOR_DNN_DNN_STATE_HPP__

#include <floor/compute/compute_context.hpp>

struct dnn_state_struct {
	//
	bool done { false };
	
	// TODO/NOTE: not implemented yet
	bool benchmark { true };
	
	//
	shared_ptr<compute_context> ctx;
	shared_ptr<compute_queue> dev_queue;
	const compute_device* dev { nullptr };
	
};
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
extern dnn_state_struct dnn_state;
#endif

#endif
