/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#include "camera.hpp"

camera::camera() : evt(floor::get_event()),
keyboard_handler(bind(&camera::key_handler, this, placeholders::_1, placeholders::_2)) {
	evt->add_internal_event_handler(keyboard_handler, EVENT_TYPE::KEY_DOWN, EVENT_TYPE::KEY_UP);
}

camera::~camera() {
	evt->remove_event_handler(keyboard_handler);
}

/*! runs the camera (expected to be called every draw)
 */
void camera::run() {
	prev_position = position;
	
	// make camera speed dependent on the time between the last update and now (scale with delta)
	static const long double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
	const auto now = chrono::high_resolution_clock::now();
	const auto delta_tp = now - time_keeper;
	time_keeper = now;
	
	auto delta = float(((long double)delta_tp.count()) / time_den);
	if(delta > 2.0f) delta = 0.0f; // ignore any updates > 2s
	const auto move_speed = delta * cam_speed;
	
	if(!is_single_frame) {
		if(keyboard_input) {
			// ... update the camera position
			const auto dir = get_direction() * move_speed;
			const auto side_dir = get_direction(float2 { 0.0f, rotation.y - 90.0f }) * move_speed;
			if(key_state[0]) {
				position.x -= side_dir.x;
				position.z -= side_dir.z;
			}
			if(key_state[1]) {
				position.x += side_dir.x;
				position.z += side_dir.z;
			}
			if(key_state[2]) {
				position += dir;
			}
			if(key_state[3]) {
				position -= dir;
			}
		}
		
		if(mouse_input) {
			// calculate the rotation via the current mouse cursor position
			int2 cursor_pos;
			const auto center_point = (floor::get_screen_size().cast<float>() * 0.5f).cast<int32_t>();
			
////////////////////////////////
// linux/windows version
#if !defined(__APPLE__)
			SDL_GetMouseState(&cursor_pos.x, &cursor_pos.y);
			
			double xpos = (1.0 / (double)floor::get_width()) * (double)cursor_pos.x;
			double ypos = (1.0 / (double)floor::get_height()) * (double)cursor_pos.y;
			
			if(xpos != 0.5 || ypos != 0.5) {
				rotation.x -= float((0.5 - ypos) * (double)rotation_speed);
				rotation.y -= float((0.5 - xpos) * (double)rotation_speed);
				
				SDL_WarpMouseInWindow(floor::get_window(), center_point.x, center_point.y);
			}
////////////////////////////////
// os x version
#else
			if(delta_hack) {
				SDL_GetMouseState(&cursor_pos.x, &cursor_pos.y);
			}
			else {
				SDL_GetRelativeMouseState(&cursor_pos.x, &cursor_pos.y);
			}
			
			if(!delta_hack || (cursor_pos.x != last_delta.x || cursor_pos.y != last_delta.y)) {
				if(delta_hack) {
					if(cursor_pos.x != center_point.x || cursor_pos.y != center_point.y) {
						const auto actual_diff = cursor_pos - last_delta;
						last_delta = cursor_pos;
						cursor_pos = actual_diff;
					}
					else {
						// ignore center reset
						cursor_pos = 0;
					}
				}
				
				double xpos = (1.0 / (double)floor::get_width()) * (double)-cursor_pos.x;
				double ypos = (1.0 / (double)floor::get_height()) * (double)-cursor_pos.y;
				
				if(xpos != 0.0 || ypos != 0.0) {
					if(!ignore_next_rotation) {
						rotation.x -= float(ypos * (double)rotation_speed);
						rotation.y -= float(xpos * (double)rotation_speed);
					}
					else ignore_next_rotation--;
					
					SDL_WarpMouseInWindow(floor::get_window(), center_point.x, center_point.y);
				}
			}
#endif
////////////////////////////////
		}
		
		if(rotation_wrapping) {
			// wrap y axis rotation, clamp x axis rotation + avoid gimbal lock
			rotation.x = const_math::clamp(rotation.x, -89.995f, 89.995f);
			rotation.y = const_math::wrap(rotation.y, 360.0f);
		}
	}
	// -> single frame camera handling
	else {
		int2 cursor_pos;
		SDL_GetRelativeMouseState(&cursor_pos.x, &cursor_pos.y);
		
		const double2 cursor_delta {
			(1.0 / (double)floor::get_width()) * (double)-cursor_pos.x,
			(1.0 / (double)floor::get_height()) * (double)-cursor_pos.y
		};
		
		if(cursor_delta.x != 0.0 || cursor_delta.y != 0.0) {
			if(!ignore_next_rotation) {
				// multiply by desired rotation speed
				delta *= rotation_speed;
				
				single_frame_direction += delta * 0.1f;
				
				// multiply existing rotation by newly computed rotation around the x and y axis
				single_frame_quat *= (quaternionf::rotation_deg(float(cursor_delta.x), float3 { 0.0f, 1.0f, 0.0f }) *
									  quaternionf::rotation_deg(float(cursor_delta.y), float3 { 1.0f, 0.0f, 0.0f }));
			}
			else ignore_next_rotation--;
			
			const float2 center_point(float2(floor::get_width(), floor::get_height()) * 0.5f);
			SDL_WarpMouseInWindow(floor::get_window(),
								  (int)roundf(center_point.x), (int)roundf(center_point.y));
		}
	}
}

void camera::set_single_frame(const bool& state) {
	if(state && !is_single_frame) {
		// init with current camera orientation
		single_frame_quat = quaternionf::rotation(0.0f, get_direction());
		single_frame_direction = 0.0f;
	}
	is_single_frame = state;
}

const quaternionf& camera::get_single_frame_rotation() const {
	return single_frame_quat;
}

float3 camera::get_single_frame_direction() const {
	const auto forward = get_direction();
	const float3 right { forward.crossed(float3 { 0.0f, 1.0f, 0.0f } /* up */).normalized() };
	return right * single_frame_direction.y + forward * single_frame_direction.x;
}

/*! sets the position of the camera
 *  @param x x coordinate
 *  @param y y coordinate
 *  @param z z coordinate
 */
void camera::set_position(const float& x, const float& y, const float& z) {
	position.set(x, y, z);
}
void camera::set_position(const float3& pos) {
	position.set(pos);
	prev_position = position; // assume full reset
}

/*! sets the rotation of the camera
 *  @param x x rotation
 *  @param y y rotation
 */
void camera::set_rotation(const float& x, const float& y) {
	rotation.set(x, y);
}
void camera::set_rotation(const float2& rot) {
	rotation.set(rot);
}

/*! returns the position of the camera
 */
float3& camera::get_position() {
	return position;
}
const float3& camera::get_position() const {
	return position;
}

const float3& camera::get_prev_position() const {
	return prev_position;
}

/*! returns the rotation of the camera
 */
float2& camera::get_rotation() {
	return rotation;
}
const float2& camera::get_rotation() const {
	return rotation;
}

/*! if cam_input is set true then arrow key input and (auto-)reposition 
 *! stuff is done automatically in the camera class. otherwise you have
 *! to do it yourself
 *  @param state the cam_input state
 */
void camera::set_keyboard_input(const bool& state) {
	keyboard_input = state;
	
	// reset key state, in case a move key is still held
	for(auto& key : key_state) key = false;
}

/*! if mouse_input is set true then the cameras rotation is controlled via
 *! the mouse - furthermore the mouse cursor is reset to (0.5, 0.5) every cycle
 *! if it's set to false nothing (no rotation) happens
 *  @param state the cam_input state
 */
void camera::set_mouse_input(const bool& state) {
	// grab input
	SDL_SetWindowGrab(floor::get_window(), (state ? SDL_TRUE : SDL_FALSE));
	
#if defined(__APPLE__)
	// this effictively calls CGAssociateMouseAndMouseCursorPosition (which will lock the cursor to the window)
	// and subsequently handles all mouse moves in relative/delta mode
	if(!delta_hack) {
		SDL_SetRelativeMouseMode(state ? SDL_TRUE : SDL_FALSE);
	}
	
	// this fixes some weird mouse positioning when switching from grab to non-grab mode
	if(mouse_input && !state) {
		const auto center_point = (floor::get_screen_size().cast<float>() * 0.5f).cast<int32_t>();
		SDL_WarpMouseInWindow(floor::get_window(), center_point.x, center_point.y);
	}
#endif
	
	ignore_next_rotation = 2;
	
	mouse_input = state;
}

/*! returns the cam_input bool
 */
bool camera::get_keyboard_input() const {
	return keyboard_input;
}

/*! returns the mouse_input bool
 */
bool camera::get_mouse_input() const {
	return mouse_input;
}

void camera::set_wasd_input(const bool& state) {
	wasd_input = state;
	
	// reset key state, in case a wasd key triggered it before
	for(auto& key : key_state) key = false;
}

bool camera::get_wasd_input() const {
	return wasd_input;
}

/*! sets the cameras rotation speed to speed
 *  @param speed the new rotation speed
 */
void camera::set_rotation_speed(const float& speed) {
	rotation_speed = speed;
}

/*! returns cameras rotation speed
 */
float camera::get_rotation_speed() const {
	return rotation_speed;
}

/*! sets the cameras speed to speed
 *  @param speed the new camera speed
 */
void camera::set_cam_speed(const float& speed) {
	cam_speed = speed;
}

/*! returns cameras speed
 */
float camera::get_cam_speed() const {
	return cam_speed;
}

float3 camera::get_direction() const {
	return get_direction(rotation);
}

float3 camera::get_direction(const float2 rotation_) {
	const float2 unit_vec_x { sinf(const_math::deg_to_rad(rotation_.x)), cosf(const_math::deg_to_rad(rotation_.x)) };
	const float2 unit_vec_y { sinf(const_math::deg_to_rad(rotation_.y)), cosf(const_math::deg_to_rad(rotation_.y)) };
	// scale x/z vector based on pitch (pitch takes priority)
	const float2 xz_dir { float2(-unit_vec_y.x, unit_vec_y.y) * unit_vec_x.y };
	// NOTE: this is normalized
	return { xz_dir.x, -unit_vec_x.x, xz_dir.y };
}

bool camera::key_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	// if keyboard input flag is not set, return
	if(!keyboard_input) return false;
	
	if(type == EVENT_TYPE::KEY_DOWN) {
		auto key_evt = static_pointer_cast<key_down_event>(obj);
		
		switch(key_evt->key) {
			case SDLK_RIGHT: key_state[0] = true; break;
			case SDLK_LEFT: key_state[1] = true; break;
			case SDLK_UP: key_state[2] = true; break;
			case SDLK_DOWN: key_state[3] = true; break;
		}
		
		if(wasd_input) {
			switch(key_evt->key) {
				case SDLK_d: key_state[0] = true; break;
				case SDLK_a: key_state[1] = true; break;
				case SDLK_w: key_state[2] = true; break;
				case SDLK_s: key_state[3] = true; break;
			}
		}
	}
	else { // EVENT_TYPE::KEY_UP
		auto key_evt = static_pointer_cast<key_up_event>(obj);
		
		switch(key_evt->key) {
			case SDLK_RIGHT: key_state[0] = false; break;
			case SDLK_LEFT: key_state[1] = false; break;
			case SDLK_UP: key_state[2] = false; break;
			case SDLK_DOWN: key_state[3] = false; break;
		}
		
		if(wasd_input) {
			switch(key_evt->key) {
				case SDLK_d: key_state[0] = false; break;
				case SDLK_a: key_state[1] = false; break;
				case SDLK_w: key_state[2] = false; break;
				case SDLK_s: key_state[3] = false; break;
			}
		}
	}
	
	return true;
}

void camera::set_rotation_wrapping(const bool& state) {
	rotation_wrapping = state;
}

const bool& camera::get_rotation_wrapping() const {
	return rotation_wrapping;
}
