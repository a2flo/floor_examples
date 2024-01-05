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

#ifndef __FLOOR_CORNELL_BOX_HPP__
#define __FLOOR_CORNELL_BOX_HPP__

// ref: http://www.graphics.cornell.edu/online/box/data.html

// cornell box, with some modifications:
// * scaled by 0.1 (to avoid fp imprecision)
// * fixed x coordinates (should be the same / connect up everywhere)
// * light moved down a little bit to avoid "z-fighting"
alignas(16) static constant constexpr const float4 cornell_vertices[] {
	// floor / 0
	{ 55.00f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 55.92f, 1.0f },
	{ 55.00f, 0.0f, 55.92f, 1.0f },
	
	// light / 4
	{ 34.30f, 54.8f, 22.70f, 1.0f },
	{ 34.30f, 54.8f, 33.20f, 1.0f },
	{ 21.30f, 54.8f, 33.20f, 1.0f },
	{ 21.30f, 54.8f, 22.70f, 1.0f },
	
	// ceiling / 8
	{ 55.00f, 54.88f, 0.0f, 1.0f },
	{ 55.00f, 54.88f, 55.92f, 1.0f },
	{ 0.0f, 54.88f, 55.92f, 1.0f },
	{ 0.0f, 54.88f, 0.0f, 1.0f },
	
	// back wall / 12
	{ 55.00f, 0.0f, 55.92f, 1.0f },
	{ 0.0f, 0.0f, 55.92f, 1.0f },
	{ 0.0f, 54.88f, 55.92f, 1.0f },
	{ 55.00f, 54.88f, 55.92f, 1.0f },
	
	// right wall / 16
	{ 0.0f, 0.0f, 55.92f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 54.88f, 0.0f, 1.0f },
	{ 0.0f, 54.88f, 55.92f, 1.0f },
	
	// left wall / 20
	{ 55.00f, 0.0f, 0.0f, 1.0f },
	{ 55.00f, 0.0f, 55.92f, 1.0f },
	{ 55.00f, 54.88f, 55.92f, 1.0f },
	{ 55.00f, 54.88f, 0.0f, 1.0f },
	
	// short block / 24
	{ 13.00f, 16.50f, 6.50f, 1.0f },
	{ 8.20f, 16.50f, 22.50f, 1.0f },
	{ 24.00f, 16.50f, 27.20f, 1.0f },
	{ 29.00f, 16.50f, 11.40f, 1.0f },
	
	{ 29.00f, 0.0f, 11.40f, 1.0f },
	{ 29.00f, 16.50f, 11.40f, 1.0f },
	{ 24.00f, 16.50f, 27.20f, 1.0f },
	{ 24.00f, 0.0f, 27.20f, 1.0f },
	
	{ 13.00f, 0.0f, 6.50f, 1.0f },
	{ 13.00f, 16.50f, 6.50f, 1.0f },
	{ 29.00f, 16.50f, 11.40f, 1.0f },
	{ 29.00f, 0.0f, 11.40f, 1.0f },
	
	{ 8.20f, 0.0f, 22.50f, 1.0f },
	{ 8.20f, 16.50f, 22.50f, 1.0f },
	{ 13.00f, 16.50f, 6.50f, 1.0f },
	{ 13.00f, 0.0f, 6.50f, 1.0f },
	
	{ 24.00f, 0.0f, 27.20f, 1.0f },
	{ 24.00f, 16.50f, 27.20f, 1.0f },
	{ 8.20f, 16.50f, 22.50f, 1.0f },
	{ 8.20f, 0.0f, 22.50f, 1.0f },
	
	// tall block / 44
	{ 42.30f, 33.00f, 24.70f, 1.0f },
	{ 26.50f, 33.00f, 29.60f, 1.0f },
	{ 31.40f, 33.00f, 45.60f, 1.0f },
	{ 47.20f, 33.00f, 40.60f, 1.0f },
	
	{ 42.30f, 0.0f, 24.70f, 1.0f },
	{ 42.30f, 33.00f, 24.70f, 1.0f },
	{ 47.20f, 33.00f, 40.60f, 1.0f },
	{ 47.20f, 0.0f, 40.60f, 1.0f },
	
	{ 47.20f, 0.0f, 40.60f, 1.0f },
	{ 47.20f, 33.00f, 40.60f, 1.0f },
	{ 31.40f, 33.00f, 45.60f, 1.0f },
	{ 31.40f, 0.0f, 45.60f, 1.0f },
	
	{ 31.40f, 0.0f, 45.60f, 1.0f },
	{ 31.40f, 33.00f, 45.60f, 1.0f },
	{ 26.50f, 33.00f, 29.60f, 1.0f },
	{ 26.50f, 0.0f, 29.60f, 1.0f },
	
	{ 26.50f, 0.0f, 29.60f, 1.0f },
	{ 26.50f, 33.00f, 29.60f, 1.0f },
	{ 42.30f, 33.00f, 24.70f, 1.0f },
	{ 42.30f, 0.0f, 24.70f, 1.0f }, // 63
};

static constexpr const float tex_scaler { 8.0f };
alignas(16) static constant constexpr const float2 cornell_tex_coords[size(cornell_vertices)] {
	// floor / 0
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	// light / 4
	{ 2.0f, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, 2.0f },
	{ 2.0f, 2.0f },
	
	// ceiling / 8
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	// back wall / 12
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	// right wall / 16
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	// left wall / 20
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	// short block / 24
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	// tall block / 44
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler },
	
	{ tex_scaler, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, tex_scaler },
	{ tex_scaler, tex_scaler }, // 63
};

alignas(16) static constant constexpr const uint4 cornell_indices[size(cornell_vertices) / 2] {
	// floor
	{ 0, 1, 2, 0 },
	{ 0, 2, 3, 0 },
	
	// light
	{ 4, 5, 6, 0 },
	{ 4, 6, 7, 0 },
	
	// ceiling
	{ 8, 9, 10, 0 },
	{ 8, 10, 11, 0 },
	
	// back wall
	{ 12, 13, 14, 0 },
	{ 12, 14, 15, 0 },
	
	// right wall
	{ 16, 17, 18, 0 },
	{ 16, 18, 19, 0 },
	
	// left wall
	{ 20, 21, 22, 0 },
	{ 20, 22, 23, 0 },
	
	// short block
	{ 24, 25, 26, 0 },
	{ 24, 26, 27, 0 },
	{ 28, 29, 30, 0 },
	{ 28, 30, 31, 0 },
	{ 32, 33, 34, 0 },
	{ 32, 34, 35, 0 },
	{ 36, 37, 38, 0 },
	{ 36, 38, 39, 0 },
	{ 40, 41, 42, 0 },
	{ 40, 42, 43, 0 },
	
	// tall block
	{ 44, 45, 46, 0 },
	{ 44, 46, 47, 0 },
	{ 48, 49, 50, 0 },
	{ 48, 50, 51, 0 },
	{ 52, 53, 54, 0 },
	{ 52, 54, 55, 0 },
	{ 56, 57, 58, 0 },
	{ 56, 58, 59, 0 },
	{ 60, 61, 62, 0 },
	{ 60, 62, 63, 0 },
};

enum class CORNELL_OBJECT : uint32_t {
	FLOOR,
	LIGHT,
	CEILING,
	BACK_WALL,
	RIGHT_WALL,
	LEFT_WALL,
	SHORT_BLOCK,
	TALL_BLOCK,
	__OBJECT_COUNT,
	__OBJECT_INVALID = __OBJECT_COUNT,
};
alignas(16) static constant constexpr const CORNELL_OBJECT cornell_object_map[size(cornell_indices)] {
	CORNELL_OBJECT::FLOOR,
	CORNELL_OBJECT::FLOOR,
	CORNELL_OBJECT::LIGHT,
	CORNELL_OBJECT::LIGHT,
	CORNELL_OBJECT::CEILING,
	CORNELL_OBJECT::CEILING,
	CORNELL_OBJECT::BACK_WALL,
	CORNELL_OBJECT::BACK_WALL,
	CORNELL_OBJECT::RIGHT_WALL,
	CORNELL_OBJECT::RIGHT_WALL,
	CORNELL_OBJECT::LEFT_WALL,
	CORNELL_OBJECT::LEFT_WALL,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::SHORT_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
	CORNELL_OBJECT::TALL_BLOCK,
};

struct material {
	const float4 diffuse;
	const float4 specular; // .w is specular exponent
	const float4 emission;
	const float4 diffuse_reflectance { diffuse * const_math::_1_DIV_PI<float> };
	const float4 specular_reflectance {
		specular.x * ((specular.w + 2.0f) * const_math::_1_DIV_2PI<float>),
		specular.y * ((specular.w + 2.0f) * const_math::_1_DIV_2PI<float>),
		specular.z * ((specular.w + 2.0f) * const_math::_1_DIV_2PI<float>),
		0.0f
	};
	// 0 = white
	// 1 = cross
	// 2 = circle
	// 3 = inv_cross
	// 4 = stripe
	const uint32_t texture_index { 0u };
};
alignas(16) static constant constexpr const material cornell_materials[(size_t)CORNELL_OBJECT::__OBJECT_COUNT] {
	// floor
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f, 0.0f, 0.0f, 10.0f },
		.emission = { 0.0f },
		.texture_index = 3u,
	},
	
	// light
	material {
		.diffuse = { 0.2f },
		.specular = { 0.0f, 0.0f, 0.0f, 10.0f },
		.emission = { 25.0f },
		.texture_index = 2u,
	},
	
	// ceiling
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f, 0.0f, 0.0f, 10.0f },
		.emission = { 0.0f },
		.texture_index = 3u,
	},
	
	// back wall
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f, 0.0f, 0.0f, 10.0f },
		.emission = { 0.0f },
		.texture_index = 2u,
	},
	
	// right wall
	material {
		.diffuse = { 0.0f, 0.407843f, 0.0f, 0.0f },
		.specular = { 0.0f, 0.0f, 0.0f, 10.0f },
		.emission = { 0.0f },
		.texture_index = 1u,
	},
	
	// left wall
	material {
		.diffuse = { 0.407843f, 0.0f, 0.0f, 0.0f },
		.specular = { 0.0f, 0.0f, 0.0f, 10.0f },
		.emission = { 0.0f },
		.texture_index = 1u,
	},
	
	// short block
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f, 0.0f, 0.0f, 10.0f },
		.emission = { 0.0f },
		.texture_index = 4u,
	},
	
	// tall block
	material {
		.diffuse = { 0.1f },
		.specular = { 0.9f, 0.9f, 0.9f, 100.0f },
		.emission = { 0.0f },
		.texture_index = 4u,
	},
};

#endif
