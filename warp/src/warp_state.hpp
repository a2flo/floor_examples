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

#ifndef __FLOOR_WARP_WARP_STATE_HPP__
#define __FLOOR_WARP_WARP_STATE_HPP__

struct warp_state_struct {
	shared_ptr<compute_context> ctx;
	shared_ptr<compute_queue> dev_queue;
	shared_ptr<compute_device> dev;
	
	shared_ptr<compute_program> warp_prog;
	shared_ptr<compute_kernel> warp_scatter_kernel, warp_gather_kernel, clear_kernel, fixup_kernel;
	
	//
	bool done { false };
	bool stop { false };
	bool no_opengl { false };
	bool no_metal { false };
	bool is_auto_cam { false };
	bool is_single_frame { false };
	bool is_motion_only { false };
	bool is_frame_repeat { false };
	
	//
	bool is_scatter { false };
	bool is_warping { true };
	bool is_render_full { true };
	bool is_clear_frame { false };
	bool is_fixup { false };
	
	// when using gather based warping, this is the current flip flop fbo idx
	// (the one which will be rendered to next)
	uint32_t cur_fbo = 0;
	
	float gather_eps_1 { 0.0025f };
	float gather_eps_2 { 2.0f };
	uint32_t gather_dbg { 0 };
	
	//
	const float view_distance { 500.0f };
	const float fov { 72.0f };
	
	// amount of frames per second that will actually be rendered (via opengl/metal)
	uint32_t render_frame_count { 10 };
	
};
extern warp_state_struct warp_state;

#endif
