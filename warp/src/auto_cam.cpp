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

#include "auto_cam.hpp"

void auto_cam::run(camera& cam) {
	static /*constexpr const*/ struct { float3 pos; float2 rot; float dist { 0.0f }; } cam_points[] {
		{ { -112.948f, 24.5206f, 44.9699f }, { 22.6389f, -162.109f } },
		{ { -115.429f, 19.129f, 25.8369f }, { 16.1111f, -137.5f } },
		{ { -107.264f, 16.3839f, 7.7657f }, { 6.11106f, -100.703f } },
		{ { -88.0644f, 16.1764f, 1.96617f }, { -10.8334f, -86.0156f } },
		{ { -69.2052f, 22.6185f, 4.15588f }, { -30.0001f, -78.2812f } },
		{ { -52.6689f, 32.719f, 9.14569f }, { -44.1668f, -66.7188f } },
		{ { -37.7766f, 44.3052f, 16.0045f }, { -47.639f, -62.7344f } },
		{ { -25.377f, 55.0035f, 27.6646f }, { -24.8612f, -77.3438f } },
		{ { -12.5873f, 55.7012f, 43.1074f }, { -1.11123f, -133.281f } },
		{ { 7.31612f, 55.8963f, 45.6903f }, { -1.94456f, -147.344f } },
		{ { 27.3132f, 56.0087f, 46.6986f }, { -0.277892f, -172.109f } },
		{ { 47.3857f, 56.0087f, 46.9143f }, { 0.416553f, -181.641f } },
		{ { 63.9997f, 55.8519f, 35.6855f }, { 2.91655f, -187.891f } },
		{ { 70.9101f, 52.2207f, 17.1927f }, { 23.8888f, -219.609f } },
		{ { 62.6246f, 44.3014f, 0.710414f }, { 25.9721f, -217.031f } },
		{ { 51.4602f, 36.223f, -13.7863f }, { 26.5277f, -219.609f } },
		{ { 43.5208f, 29.8585f, -31.0269f }, { 23.0555f, -266.25f } },
		{ { 28.4669f, 25.0707f, -43.3755f }, { 17.7777f, -282.266f } },
		{ { 9.33647f, 21.6928f, -48.6351f }, { 11.6665f, -308.359f } },
		{ { -10.6433f, 19.4421f, -48.137f }, { 5.41654f, -326.016f } },
		{ { -30.672f, 18.6144f, -46.7276f }, { 5.13876f, -349.922f } },
		{ { -50.7298f, 18.5009f, -47.6858f }, { 4.30543f, -356.953f } },
		{ { -62.738f, 22.6749f, -32.1796f }, { -32.7779f, -359.453f } },
		{ { -62.7952f, 31.8632f, -14.3896f }, { -19.3057f, -360.547f } },
		{ { -64.4222f, 33.9381f, 5.52183f }, { 3.74989f, -346.641f } },
		{ { -64.1056f, 31.4625f, 25.426f }, { 15.2777f, -295.625f } },
		{ { -74.997f, 26.9294f, 41.7019f }, { 19.7221f, -268.359f } },
		{ { -92.2111f, 23.734f, 51.5358f }, { 13.0554f, -216.016f } },
	};
	static float cur_dist { 0.0f };
	static size_t cur_idx { 0 };
	static float3 cur_pos { cam_points[cur_idx].pos };
	static float2 cur_rot { cam_points[cur_idx].rot };
	static constexpr const float step_size { 20.0f };
	
	float total_dist { 0.0f }; // TODO: make cexpr
	for(size_t i = 0, count = size(cam_points); i < count; ++i) {
		total_dist += cam_points[i].pos.distance(cam_points[(i + 1) % count].pos);
		cam_points[i].dist = total_dist;
	}
	
	// time keeping
	static constexpr const long double time_den { 1.0L / (long double)chrono::high_resolution_clock::time_point::duration::period::den };
	static auto time_keeper = chrono::high_resolution_clock::now();
	auto now = chrono::high_resolution_clock::now();
	auto delta = now - time_keeper;
	time_keeper = now;
	const float step = float((long double)delta.count() * time_den);
	cur_dist += step * step_size;
	//cur_dist += 0.01f * step_size; // fixed size
	if(cur_dist >= total_dist) {
		cur_dist = fmodf(cur_dist, total_dist);
	}
	
	//
	const auto cur_iter = lower_bound(cbegin(cam_points), cend(cam_points), cur_dist, [](const auto& p, const float& dist) {
		return p.dist < dist;
	});
	auto next_iter = next(cur_iter);
	if(next_iter == cend(cam_points)) next_iter = cbegin(cam_points);
	
	auto prev_iter = prev(cur_iter != cbegin(cam_points) ? cur_iter : cend(cam_points));
	auto next_next_iter = next(next_iter);
	if(next_next_iter == cend(cam_points)) next_next_iter = cbegin(cam_points);
	
	const auto prev_dist = (cur_iter != cbegin(cam_points) ? prev(cur_iter)->dist : 0.0f);
	const auto interp = (cur_dist - prev_dist) / (cur_iter->dist - prev_dist);
	//cur_pos = cur_iter->pos.interpolated(next_iter->pos, interp);
	//cur_rot = cur_iter->rot.interpolated(next_iter->rot, interp);
	cur_pos = cur_iter->pos.catmull_rom_interpolated(next_iter->pos, prev_iter->pos, next_next_iter->pos, interp);
	cur_rot = cur_iter->rot.catmull_rom_interpolated(next_iter->rot, prev_iter->rot, next_next_iter->rot, interp);
	
	cam.set_position(cur_pos);
	cam.set_rotation(double(cur_rot.x), double(cur_rot.y));
}
