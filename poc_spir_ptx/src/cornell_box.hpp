
#ifndef __CORNELL_BOX_HPP__
#define __CORNELL_BOX_HPP__

// ref: http://www.graphics.cornell.edu/online/box/data.html

// cornell box, with some modifications:
// * scaled by 0.1 (do avoid fp imprecision)
// * fixed x coordinates (should be the same / connect up everywhere)
// * light moved down a little bit to avoid "z-fighting"
static constant constexpr const array<float3, 64> cornell_vertices {{
	// floor / 0
	float3 { 55.00f, 0.0f, 0.0f },
	float3 { 0.0f, 0.0f, 0.0f },
	float3 { 0.0f, 0.0f, 55.92f },
	float3 { 55.00f, 0.0f, 55.92f },
	
	// light / 4
	float3 { 34.30f, 54.8f, 22.70f },
	float3 { 34.30f, 54.8f, 33.20f },
	float3 { 21.30f, 54.8f, 33.20f },
	float3 { 21.30f, 54.8f, 22.70f },
	
	// ceiling / 8
	float3 { 55.00f, 54.88f, 0.0f },
	float3 { 55.00f, 54.88f, 55.92f },
	float3 { 0.0f, 54.88f, 55.92f },
	float3 { 0.0f, 54.88f, 0.0f },
	
	// back wall / 12
	float3 { 55.00f, 0.0f, 55.92f },
	float3 { 0.0f, 0.0f, 55.92f },
	float3 { 0.0f, 54.88f, 55.92f },
	float3 { 55.00f, 54.88f, 55.92f },
	
	// right wall / 16
	float3 { 0.0f, 0.0f, 55.92f },
	float3 { 0.0f, 0.0f, 0.0f },
	float3 { 0.0f, 54.88f, 0.0f },
	float3 { 0.0f, 54.88f, 55.92f },
	
	// left wall / 20
	float3 { 55.00f, 0.0f, 0.0f },
	float3 { 55.00f, 0.0f, 55.92f },
	float3 { 55.00f, 54.88f, 55.92f },
	float3 { 55.00f, 54.88f, 0.0f },
	
	// short block / 24
	float3 { 13.00f, 16.50f, 6.50f },
	float3 { 8.20f, 16.50f, 22.50f },
	float3 { 24.00f, 16.50f, 27.20f },
	float3 { 29.00f, 16.50f, 11.40f },
	
	float3 { 29.00f, 0.0f, 11.40f },
	float3 { 29.00f, 16.50f, 11.40f },
	float3 { 24.00f, 16.50f, 27.20f },
	float3 { 24.00f, 0.0f, 27.20f },
	
	float3 { 13.00f, 0.0f, 6.50f },
	float3 { 13.00f, 16.50f, 6.50f },
	float3 { 29.00f, 16.50f, 11.40f },
	float3 { 29.00f, 0.0f, 11.40f },
	
	float3 { 8.20f, 0.0f, 22.50f },
	float3 { 8.20f, 16.50f, 22.50f },
	float3 { 13.00f, 16.50f, 6.50f },
	float3 { 13.00f, 0.0f, 6.50f },
	
	float3 { 24.00f, 0.0f, 27.20f },
	float3 { 24.00f, 16.50f, 27.20f },
	float3 { 8.20f, 16.50f, 22.50f },
	float3 { 8.20f, 0.0f, 22.50f },
	
	// tall block / 44
	float3 { 42.30f, 33.00f, 24.70f },
	float3 { 26.50f, 33.00f, 29.60f },
	float3 { 31.40f, 33.00f, 45.60f },
	float3 { 47.20f, 33.00f, 40.60f },
	
	float3 { 42.30f, 0.0f, 24.70f },
	float3 { 42.30f, 33.00f, 24.70f },
	float3 { 47.20f, 33.00f, 40.60f },
	float3 { 47.20f, 0.0f, 40.60f },
	
	float3 { 47.20f, 0.0f, 40.60f },
	float3 { 47.20f, 33.00f, 40.60f },
	float3 { 31.40f, 33.00f, 45.60f },
	float3 { 31.40f, 0.0f, 45.60f },
	
	float3 { 31.40f, 0.0f, 45.60f },
	float3 { 31.40f, 33.00f, 45.60f },
	float3 { 26.50f, 33.00f, 29.60f },
	float3 { 26.50f, 0.0f, 29.60f },
	
	float3 { 26.50f, 0.0f, 29.60f },
	float3 { 26.50f, 33.00f, 29.60f },
	float3 { 42.30f, 33.00f, 24.70f },
	float3 { 42.30f, 0.0f, 24.70f }, // 63
}};

static constant constexpr const array<uint3, cornell_vertices.size() / 2> cornell_indices {{
	// floor
	uint3 { 0, 1, 2 },
	uint3 { 0, 2, 3 },
	
	// light
	uint3 { 4, 5, 6 },
	uint3 { 4, 6, 7 },
	
	// ceiling
	uint3 { 8, 9, 10 },
	uint3 { 8, 10, 11 },
	
	// back wall
	uint3 { 12, 13, 14 },
	uint3 { 12, 14, 15 },
	
	// right wall
	uint3 { 16, 17, 18 },
	uint3 { 16, 18, 19 },
	
	// left wall
	uint3 { 20, 21, 22 },
	uint3 { 20, 22, 23 },
	
	// short block
	uint3 { 24, 25, 26 },
	uint3 { 24, 26, 27 },
	uint3 { 28, 29, 30 },
	uint3 { 28, 30, 31 },
	uint3 { 32, 33, 34 },
	uint3 { 32, 34, 35 },
	uint3 { 36, 37, 38 },
	uint3 { 36, 38, 39 },
	uint3 { 40, 41, 42 },
	uint3 { 40, 42, 43 },
	
	// tall block
	uint3 { 44, 45, 46 },
	uint3 { 44, 46, 47 },
	uint3 { 48, 49, 50 },
	uint3 { 48, 50, 51 },
	uint3 { 52, 53, 54 },
	uint3 { 52, 54, 55 },
	uint3 { 56, 57, 58 },
	uint3 { 56, 58, 59 },
	uint3 { 60, 61, 62 },
	uint3 { 60, 62, 63 },
}};

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
};
static constant constexpr const array<CORNELL_OBJECT, cornell_indices.size()> cornell_object_map {{
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
}};

struct material {
	const float3 diffuse;
	const float3 specular;
	const float specular_exponent;
	const float3 emission;
	const float3 diffuse_reflectance { float3(diffuse) * float(const_math::_1_DIV_PI<float>) };
	const float3 specular_reflectance { float3(specular) * ((float(specular_exponent) + 2.0f) * const_math::_1_DIV_2PI<float>) };
};
static constant constexpr const array<material, (size_t)CORNELL_OBJECT::__OBJECT_COUNT> cornell_materials {{
	// floor
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f },
		.specular_exponent = 10.0f,
		.emission = { 0.0f },
	},
	
	// light
	material {
		.diffuse = { 0.2f },
		.specular = { 0.0f },
		.specular_exponent = 10.0f,
		.emission = { 15.0f },
	},
	
	// ceiling
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f },
		.specular_exponent = 10.0f,
		.emission = { 0.0f },
	},
	
	// back wall
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f },
		.specular_exponent = 10.0f,
		.emission = { 0.0f },
	},
	
	// right wall
	material {
		.diffuse = { 0.0f, 0.407843f, 0.0f },
		.specular = { 0.0f },
		.specular_exponent = 10.0f,
		.emission = { 0.0f },
	},
	
	// left wall
	material {
		.diffuse = { 0.407843f, 0.0f, 0.0f },
		.specular = { 0.0f },
		.specular_exponent = 10.0f,
		.emission = { 0.0f },
	},
	
	// short block
	material {
		.diffuse = { 0.407843f },
		.specular = { 0.0f },
		.specular_exponent = 10.0f,
		.emission = { 0.0f },
	},
	
	// tall block
	material {
		.diffuse = { 0.1f },
		.specular = { 0.9f },
		.specular_exponent = 100.0f,
		.emission = { 0.0f },
	},
}};

#endif
