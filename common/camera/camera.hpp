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

#ifndef __FLOOR_CAMERA_HPP__
#define __FLOOR_CAMERA_HPP__

#include <floor/floor/floor.hpp>
#include <floor/core/core.hpp>
#include <floor/core/event.hpp>
#include <floor/math/quaternion.hpp>
#include <chrono>

//! floor camera functions
class camera {
public:
	camera();
	~camera();

	void run();

	void set_position(const double& x, const double& y, const double& z);
	void set_position(const double3& pos);
	double3& get_position();
	const double3& get_position() const;
	const double3& get_prev_position() const;
	
	void set_rotation(const double& x, const double& y);
	void set_rotation(const double2& rot);
	double2& get_rotation();
	const double2& get_rotation() const;
	void set_rotation_wrapping(const bool& state);
	const bool& get_rotation_wrapping() const;
	
	void set_single_frame(const bool& state);
	double3 get_single_frame_direction() const;
	const quaterniond& get_single_frame_rotation() const;

	void set_rotation_speed(const double& speed);
	double get_rotation_speed() const;
	void set_cam_speed(const double& speed);
	double get_cam_speed() const;

	void set_keyboard_input(const bool& state);
	bool get_keyboard_input() const;
	void set_mouse_input(const bool& state);
	bool get_mouse_input() const;
	void set_wasd_input(const bool& state);
	bool get_wasd_input() const;
	
	double3 get_direction() const;
	static double3 get_direction(const double2 rotation);
	
	//! returns the rotation matrix for the current rotation
	template <typename fp_type> requires is_floating_point_v<fp_type>
	auto get_rotation_matrix() const {
		return (matrix4d::rotation_deg_named<'y'>(rotation.y) *
				matrix4d::rotation_deg_named<'x'>(rotation.x)).cast<fp_type>();
	}
	
	//! the full camera state
	struct camera_state_t {
		//! actual absolute position
		double3 position;
		double2 rotation;
		matrix4f rotation_matrix;
		//! normalized forward vector
		float3 forward;
	};
	//! returns the current full camera state
	camera_state_t get_current_state() const;
	
	//! used internally, but can also be used externally to force a state update (e.g. for auto-cam)
	void update_state();

protected:
	event* evt;

	double3 position, prev_position;
	double2 rotation;
	
	//! camera state can be retrieved concurrently
	struct concurrent_camera_state_t {
		mutable safe_mutex lock;
		camera_state_t state GUARDED_BY(lock);
	};
	static constexpr const uint32_t camera_state_count { 3u };
	atomic<uint32_t> current_camera_state;
	concurrent_camera_state_t camera_states[camera_state_count];
	
	bool is_single_frame = false;
	quaterniond single_frame_quat;
	double2 single_frame_direction;
	double single_frame_distance = 0.0;
	
	double up_down = 0.0;
	double rotation_speed = 150.0;
	double cam_speed = 75.0;

	bool keyboard_input = true;
	bool mouse_input = false;
	bool wasd_input = false;
	bool rotation_wrapping = true;
	unsigned int ignore_next_rotation = 0;
	
	// hack for mice on os x 10.12+ that don't have proper warp support
	bool delta_hack { false };
	int2 last_delta;
	
	// [right, left, up, down]
	atomic<bool> key_state[4] { { false }, { false }, { false }, { false } };
	
	event::handler keyboard_handler;
	bool key_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	//
	chrono::time_point<chrono::high_resolution_clock> time_keeper { chrono::high_resolution_clock::now() };
	
};

#endif
