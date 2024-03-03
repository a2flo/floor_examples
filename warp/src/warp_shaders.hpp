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

#ifndef __FLOOR_WARP_WARP_SHADERS_HPP__
#define __FLOOR_WARP_WARP_SHADERS_HPP__

#include <floor/core/essentials.hpp>
#include <floor/math/vector_lib.hpp>

#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

namespace warp_shaders {

//! Sponza scene consists of 25 different materials
static constexpr constant const uint32_t material_count { 25u };

//! uniforms used during direct rendering
struct __attribute__((packed)) scene_base_uniforms_t {
	matrix4f mvpm;
	matrix4f light_bias_mvpm;
	float3 cam_pos;
	float3 light_pos;
};

struct __attribute__((packed)) scene_scatter_uniforms_t : scene_base_uniforms_t {
	matrix4f mvm;
	matrix4f prev_mvm;
};

struct __attribute__((packed)) scene_gather_uniforms_t : scene_base_uniforms_t {
	matrix4f next_mvpm; // @t+1
	matrix4f prev_mvpm; // @t-1
};

struct shadow_uniforms_t {
	matrix4f mvpm;
};

struct skybox_base_uniforms_t {
	matrix4f imvpm;
};

struct skybox_scatter_uniforms_t : skybox_base_uniforms_t {
	matrix4f prev_imvpm;
};

struct skybox_gather_uniforms_t : skybox_base_uniforms_t {
	matrix4f next_mvpm;
	matrix4f prev_mvpm;
};

//! uniforms used during indirect rendering
struct frame_uniforms_t {
	matrix4f mvpm;
	matrix4f light_bias_mvpm;
	matrix4f light_mvpm;
	matrix4f sky_imvpm;
	struct {
		matrix4f scene_mvm;
		matrix4f scene_prev_mvm;
		matrix4f sky_prev_imvpm;
	} scatter;
	struct {
		matrix4f scene_next_mvpm; // @t+1
		matrix4f scene_prev_mvpm; // @t-1
		matrix4f sky_next_mvpm;
		matrix4f sky_prev_mvpm;
	} gather;
	float3 cam_pos;
	float3 light_pos;
	uint32_t displacement_mode;
};

//! model data struct for argument buffer use
struct __attribute__((packed)) model_data_t {
	enum : uint32_t { POSITION, NORMAL, BINORMAL, TANGENT };
	// [position, normal, binormal, tangent]
	array<buffer<const float3>, 4> pnbt;
	// textcure coordinates
	buffer<const float2> tc;
	// per-vertex material indices
	buffer<const uint32_t> materials_data;
	// indices
	buffer<const uint3> indices;
	// #triangles
	const uint32_t triangle_count;
};

//! material images struct for argument buffer use
struct materials_t {
	array<const_image_2d<float>, material_count> diff_tex;
	array<const_image_2d<float>, material_count> spec_tex;
	array<const_image_2d<float>, material_count> norm_tex;
	array<const_image_2d<float1>, material_count> mask_tex;
	array<const_image_2d<float1>, material_count> disp_tex;
};

//! scene fragment shader data for argument buffer use
struct scene_fs_data_t {
	const_image_2d_depth<float> shadow_tex;
};

//! skybox fragment shader data for argument buffer use
struct sky_fs_data_t {
	const_image_cube<float> skybox_tex;
};

} // namespace warp_shaders

#endif
