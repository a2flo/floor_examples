/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#ifndef __FLOOR_REDUCTION_REDUCTION_STATE_HPP__
#define __FLOOR_REDUCTION_REDUCTION_STATE_HPP__

struct reduction_state_struct {
	//
	bool done { false };
	
	//
	bool benchmark { false };
	
	//
	uint32_t max_iterations { 3u };
	
	//
	enum class EXEC_MODE {
		REDUCTION_F32,
		REDUCTION_U32,
		INCLUSIVE_SCAN,
		EXCLUSIVE_SCAN,
	};
	EXEC_MODE exec_mode { EXEC_MODE::REDUCTION_F32 };
	
};
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
extern reduction_state_struct reduction_state;
#endif

#endif
