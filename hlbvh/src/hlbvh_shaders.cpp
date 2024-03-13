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

#include <floor/core/essentials.hpp>
#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

// graphics backends only
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_GRAPHICS_HOST)

#if !defined(COLLIDING_TRIANGLES_VIS)
#define COLLIDING_TRIANGLES_VIS 0
#endif

//////////////////////////////////////////
// scene

struct scene_in_out {
	float4 position [[position]];
	float4 color;
	float3 normal;
#if COLLIDING_TRIANGLES_VIS
	float collision;
#endif
};

struct scene_uniforms_t {
	matrix4f mvpm;
	float4 default_color;
	float3 light_dir;
};

vertex auto hlbvh_vertex(buffer<const float3> in_position_a,
						 buffer<const float3> in_position_b,
						 buffer<const float3> in_normal_a,
						 buffer<const float3> in_normal_b,
						 buffer<const scene_uniforms_t> uniforms,
						 param<float> delta
#if COLLIDING_TRIANGLES_VIS
						 , buffer<const uint32_t> is_collision
#endif
						 ) {
	scene_in_out out;
	
	const float4 pos {
		in_position_a[vertex_id].interpolated(in_position_b[vertex_id], delta),
		1.0f
	};
	out.position = pos * uniforms->mvpm;
	out.color = uniforms->default_color;
	out.normal = { in_normal_a[vertex_id].interpolated(in_normal_b[vertex_id], delta) };
#if COLLIDING_TRIANGLES_VIS
	out.collision = (is_collision[vertex_id] > 0u ? 1.0f : 0.0f);
#endif
	
	return out;
}

fragment auto hlbvh_fragment(const scene_in_out in [[stage_input]],
							 buffer<const scene_uniforms_t> uniforms) {
	const auto intensity = uniforms->light_dir.dot(in.normal);
	float4 color;
#if COLLIDING_TRIANGLES_VIS
	if (in.collision >= 0.999f /* due to interpolation issues, we might not get 1.0 here */) {
		color = { 1.0f, 0.0f, 0.0f, 1.0f };
		color.xyz *= max(intensity, 0.6f);
	}
	else
#endif
	{
		if (intensity > 0.95f) {
			color = { in.color.xyz * 1.2f, in.color.w };
		} else if (intensity > 0.25f) {
			color = { in.color.xyz * intensity, in.color.w };
		} else {
			color = { in.color.xyz * 0.1f, in.color.w };
		}
	}
	color.xyz *= color.w;
	
	return color;
}

#endif
