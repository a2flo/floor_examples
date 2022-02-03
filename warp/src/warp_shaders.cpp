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

#include <floor/core/essentials.hpp>

#if defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/common.hpp>
#endif

// metal and vulkan only
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_GRAPHICS_HOST)

#if !defined(WARP_SHADOW_NEAR_PLANE)
#define WARP_SHADOW_NEAR_PLANE 1.0f
#endif

#if !defined(WARP_SHADOW_FAR_PLANE)
#define WARP_SHADOW_FAR_PLANE 260.0f
#endif

//////////////////////////////////////////
// scene

struct scene_base_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
	float3 view_dir;
	float3 light_dir;
	uint32_t material_idx;
};

struct scene_scatter_in_out : scene_base_in_out {
	float3 motion;
};

struct scene_gather_in_out : scene_base_in_out {
	float4 motion_prev;
	float4 motion_now;
	float4 motion_next;
};

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

// NOTE: color location indices are generated automatically
struct scene_scatter_fragment_out {
	float4 color;
	uint32_t motion;
};

struct scene_gather_fragment_out {
	float4 color;
	uint32_t motion_forward;
	uint32_t motion_backward;
	// this actually is a half2, but we can't explicitly use this in vulkan,
	// however, this is of course still backed by a half2 image
	float2 motion_depth;
};

// props to:
// * https://outerra.blogspot.com/2013/07/logarithmic-depth-buffer-optimizations.html
// * https://outerra.blogspot.sk/2012/11/maximizing-depth-buffer-range-and.html
// * https://outerra.blogspot.sk/2009/08/logarithmic-z-buffer.html
static float4 log_depth(float4 transformed_position) {
	// using the [0, 1] Z variant here
	// -> 2013 w/ optimized log and no near
	//static constexpr const float C = WARP_SHADOW_NEAR_PLANE; // 1.0
	static constexpr const float far_plane = WARP_SHADOW_FAR_PLANE; // 260.0
	static constexpr const float f_coef = 1.0f / math::log2(far_plane + 1.0f);
	transformed_position.z = math::log2(math::max(1.0e-6f, transformed_position.w + 1.0f)) * f_coef;
	transformed_position.z *= transformed_position.w;
	return transformed_position;
}

static void scene_vs(scene_base_in_out& out,
					 buffer<const float3> in_position,
					 buffer<const float2> in_tex_coord,
					 buffer<const float3> in_normal,
					 buffer<const float3> in_binormal,
					 buffer<const float3> in_tangent,
					 buffer<const uint32_t> in_materials,
					 const scene_base_uniforms_t& uniforms) {
	out.tex_coord = in_tex_coord[vertex_id];
	
	float4 pos { in_position[vertex_id], 1.0f };
	out.shadow_coord = log_depth(pos * uniforms.light_bias_mvpm);
	out.position = pos * uniforms.mvpm;
	
	const auto vview = uniforms.cam_pos - in_position[vertex_id];
	out.view_dir.x = vview.dot(in_tangent[vertex_id]);
	out.view_dir.y = vview.dot(in_binormal[vertex_id]);
	out.view_dir.z = vview.dot(in_normal[vertex_id]);
	
	const auto vlight = uniforms.light_pos - in_position[vertex_id];
	out.light_dir.x = vlight.dot(in_tangent[vertex_id]);
	out.light_dir.y = vlight.dot(in_binormal[vertex_id]);
	out.light_dir.z = vlight.dot(in_normal[vertex_id]);
	
	out.material_idx = in_materials[vertex_id];
}

vertex auto scene_scatter_vs(buffer<const float3> in_position,
							 buffer<const float2> in_tex_coord,
							 buffer<const float3> in_normal,
							 buffer<const float3> in_binormal,
							 buffer<const float3> in_tangent,
							 buffer<const uint32_t> in_materials,
							 param<scene_scatter_uniforms_t> uniforms) {
	scene_scatter_in_out out;
	scene_vs(out, in_position, in_tex_coord, in_normal, in_binormal, in_tangent, in_materials, uniforms);
	
	float4 pos { in_position[vertex_id], 1.0f };
	float4 prev_pos = pos * uniforms.prev_mvm;
	float4 cur_pos = pos * uniforms.mvm;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}

// also used for forward-only
vertex auto scene_gather_vs(buffer<const float3> in_position,
							buffer<const float2> in_tex_coord,
							buffer<const float3> in_normal,
							buffer<const float3> in_binormal,
							buffer<const float3> in_tangent,
							buffer<const uint32_t> in_materials,
							param<scene_gather_uniforms_t> uniforms) {
	scene_gather_in_out out;
	scene_vs(out, in_position, in_tex_coord, in_normal, in_binormal, in_tangent, in_materials, uniforms);
	
	float4 pos { in_position[vertex_id], 1.0f };
	out.motion_prev = pos * uniforms.prev_mvpm;
	out.motion_now = pos * uniforms.mvpm;
	out.motion_next = pos * uniforms.next_mvpm;
	
	return out;
}

static uint32_t encode_3d_motion(const float3& motion) {
	constexpr const float range = 64.0f; // [-range, range]
	const float3 signs = motion.sign();
	float3 cmotion = (motion.absed().clamp(0.0f, range) + 1.0f).log2();
	// encode x and z with 10-bit, y with 9-bit (2^10, 2^9, 2^10)
	constexpr const auto ce_scale = float3 { 1024.0f, 512.0f, 1024.0f } / math::log2(range + 1.0f);
	cmotion *= ce_scale;
	cmotion.clamp(0.0f, { 1023.0f, 511.0f, 1023.0f });
	const auto ui_cmotion = (uint3)cmotion;
	return ((signs.x < 0.0f ? 0x80000000u : 0u) |
			(signs.y < 0.0f ? 0x40000000u : 0u) |
			(signs.z < 0.0f ? 0x20000000u : 0u) |
			(ui_cmotion.x << 19u) |
			(ui_cmotion.y << 10u) |
			(ui_cmotion.z));
}

static uint32_t encode_2d_motion(const float2& motion) {
	return pack_snorm_2x16(motion); // +/- 2^15 - 1, fit into 16 bits
}

static float4 scene_fs(const scene_base_in_out& in,
					   const uint32_t disp_mode,
					   const_image_2d<float>& diff_tex,
					   const_image_2d<float>& spec_tex,
					   const_image_2d<float>& norm_tex,
					   const_image_2d<float1>& mask_tex,
					   const_image_2d<float1>& disp_tex,
					   const_image_2d_depth<float>& shadow_tex) {
	const auto in_tex_coord = in.tex_coord;
	const auto norm_view_dir = in.view_dir.normalized();

	// mask out
	if (mask_tex.read_lod_linear_repeat(in_tex_coord, 0) < 0.5f) {
		discard_fragment();
	}
	
	// compute parallax tex coord
	static constexpr constant const float parallax { 0.03f };
	float2 tex_coord = in_tex_coord;
	if (disp_mode == 1) {
		float height = 0.0f, offset = 0.0f;
#pragma unroll
		for (uint32_t i = 1u; i < 8u; ++i) {
			auto grad = dfdx_dfdy_gradient(tex_coord);
			height += disp_tex.read_gradient_linear_repeat(tex_coord, grad);
			offset = parallax * ((2.0f / float(i)) * height - 1.0f);
			tex_coord = in_tex_coord + offset * norm_view_dir.xy;
		}
	}
	
	const auto tex_grad = dfdx_dfdy_gradient(tex_coord);
	auto diff = diff_tex.read_gradient_linear_repeat(tex_coord, tex_grad);
	
	//
	constexpr const float fixed_bias = 0.0001f;
	constexpr const float ambient = 0.2f;
	constexpr const float attenuation = 0.9f;
	float lighting = 0.0f;
	float light_vis = 1.0f;
	
	//
	const auto norm_light_dir = in.light_dir.normalized();
	const auto norm_half_dir = (norm_view_dir + norm_light_dir).normalized();
	const auto normal = (norm_tex.read_gradient_linear_repeat(tex_coord, tex_grad).xyz * 2.0f - 1.0f).normalized();
	
	const auto lambert = normal.dot(norm_light_dir);
	{
		// shadow hackery
		const auto bias_lambert = math::clamp(lambert, 0.000005f, 0.999995f); // clamp to "(0, 1)"
		const auto slope = sqrt(1.0f - bias_lambert * bias_lambert) / bias_lambert; // = tan(acos(lambert))
		const auto shadow_bias = math::clamp(fixed_bias * slope, 0.0f, fixed_bias * 2.0f);
		
		float3 shadow_coord = in.shadow_coord.xyz / in.shadow_coord.w;
#if 0
		if(in.shadow_coord.w > 0.0f) {
			light_vis = shadow_tex.compare_linear<COMPARE_FUNCTION::LESS_OR_EQUAL>(shadow_coord.xy, shadow_coord.z - shadow_bias);
		}
#elif 1
		// props to https://code.google.com/p/opengl-tutorial-org/source/browse/#hg%2Ftutorial16_shadowmaps for this
		static constexpr const float2 poisson_disk[] {
			{ -0.94201624f, -0.39906216f },
			{ 0.94558609f, -0.76890725f },
			{ -0.094184101f, -0.92938870f },
			{ 0.34495938f, 0.29387760f },
			{ -0.91588581f, 0.45771432f },
			{ -0.81544232f, -0.87912464f },
			{ -0.38277543f, 0.27676845f },
			{ 0.97484398f, 0.75648379f },
			{ 0.44323325f, -0.97511554f },
			{ 0.53742981f, -0.47373420f },
			{ -0.26496911f, -0.41893023f },
			{ 0.79197514f, 0.19090188f },
			{ -0.24188840f, 0.99706507f },
			{ -0.81409955f, 0.91437590f },
			{ 0.19984126f, 0.78641367f },
			{ 0.14383161f, -0.14100790f }
		};
		
		const float coord_factor = 3.0f /* wider / more blur */ / float(shadow_tex.dim().x);
#pragma unroll
		for (const auto& poisson_coord : poisson_disk) {
			light_vis -= 0.0625f * (1.0f - shadow_tex.compare_linear<COMPARE_FUNCTION::LESS_OR_EQUAL>(shadow_coord.xy + poisson_coord * coord_factor,
																									  shadow_coord.z - shadow_bias));
		}
#endif
		
		// diffuse
		lighting += max(lambert, 0.0f);
		
		// specular
		const auto spec = spec_tex.read_gradient_linear_repeat(tex_coord, tex_grad).x;
		const auto specular = pow(max(norm_half_dir.dot(normal), 0.0f), spec * 10.0f);
		lighting += max(specular, 0.0f);
		
		// mul with shadow and light attenuation
		lighting *= light_vis * attenuation;
	}
	lighting = max(lighting, ambient);
	
	auto color = diff.xyz * lighting;
#if defined(WARP_APPLY_GAMMA)
	// apply fixed inv gamma for non-wide-gamut/non-HDR displays
	// NOTE: not really accurate, but good enough
	color.pow(1.0f / 2.2f);
#endif
	return { color, 1.0f };
}

// Sponza scene consists of 25 different materials
static constexpr constant const uint32_t material_count { 25u };

fragment auto scene_scatter_fs(const scene_scatter_in_out in [[stage_input]],
							   param<uint32_t> disp_mode,
							   array<const_image_2d<float>, material_count> diff_tex,
							   array<const_image_2d<float>, material_count> spec_tex,
							   array<const_image_2d<float>, material_count> norm_tex,
							   array<const_image_2d<float1>, material_count> mask_tex,
							   array<const_image_2d<float1>, material_count> disp_tex,
							   const_image_2d_depth<float> shadow_tex) {
	return scene_scatter_fragment_out {
		scene_fs(in, disp_mode, diff_tex[in.material_idx], spec_tex[in.material_idx], norm_tex[in.material_idx],
				 mask_tex[in.material_idx], disp_tex[in.material_idx], shadow_tex),
		encode_3d_motion(in.motion)
	};
}

fragment auto scene_gather_fs(const scene_gather_in_out in [[stage_input]],
							  param<uint32_t> disp_mode,
							  array<const_image_2d<float>, material_count> diff_tex,
							  array<const_image_2d<float>, material_count> spec_tex,
							  array<const_image_2d<float>, material_count> norm_tex,
							  array<const_image_2d<float1>, material_count> mask_tex,
							  array<const_image_2d<float1>, material_count> disp_tex,
							  const_image_2d_depth<float> shadow_tex) {
	return scene_gather_fragment_out {
		scene_fs(in, disp_mode, diff_tex[in.material_idx], spec_tex[in.material_idx], norm_tex[in.material_idx],
				 mask_tex[in.material_idx], disp_tex[in.material_idx], shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		{
			(in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w),
			(in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w)
		}
	};
}

fragment auto scene_gather_fwd_fs(const scene_gather_in_out in [[stage_input]],
								  param<uint32_t> disp_mode,
								  array<const_image_2d<float>, material_count> diff_tex,
								  array<const_image_2d<float>, material_count> spec_tex,
								  array<const_image_2d<float>, material_count> norm_tex,
								  array<const_image_2d<float1>, material_count> mask_tex,
								  array<const_image_2d<float1>, material_count> disp_tex,
								  const_image_2d_depth<float> shadow_tex) {
	return scene_scatter_fragment_out /* reuse */ {
		scene_fs(in, disp_mode, diff_tex[in.material_idx], spec_tex[in.material_idx], norm_tex[in.material_idx],
				 mask_tex[in.material_idx], disp_tex[in.material_idx], shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}

//////////////////////////////////////////
// shadow map

struct shadow_uniforms_t {
	matrix4f mvpm;
};

struct shadow_in_out {
	float4 position [[position]];
};

vertex shadow_in_out shadow_vs(buffer<const float3> in_position,
							   param<shadow_uniforms_t> uniforms) {
	return {
		log_depth(float4 { in_position[vertex_id], 1.0f } * uniforms.mvpm)
	};
}

//////////////////////////////////////////
// sky box

struct skybox_base_in_out {
	float4 position [[position]];
	float3 cube_tex_coord;
};

struct skybox_scatter_in_out : skybox_base_in_out {
	float4 cur_pos;
	float4 prev_pos;
};

struct skybox_gather_in_out : skybox_base_in_out {
	float4 motion_prev;
	float4 motion_now;
	float4 motion_next;
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


static void skybox_vs(skybox_base_in_out& out, const skybox_base_uniforms_t& uniforms) {
	switch (vertex_id) {
		case 0: out.position = { 3.0f, -1.0f, 1.0f, 1.0f }; break;
		case 1: out.position = { -3.0f, -1.0f, 1.0f, 1.0f }; break;
		case 2: out.position = { 0.0f, 2.0f, 1.0f, 1.0f }; break;
		default: floor_unreachable();
	}
	out.cube_tex_coord = (out.position * uniforms.imvpm).xyz;
}

vertex auto skybox_scatter_vs(param<skybox_scatter_uniforms_t> uniforms) {
	skybox_scatter_in_out out;
	skybox_vs(out, uniforms);
	
	out.cur_pos = out.position * uniforms.imvpm;
	out.prev_pos = out.position * uniforms.prev_imvpm;
	
	return out;
}

// also used for forward-only
vertex auto skybox_gather_vs(param<skybox_gather_uniforms_t> uniforms) {
	skybox_gather_in_out out;
	skybox_vs(out, uniforms);
	
	const auto proj_vertex = out.position * uniforms.imvpm;
	out.motion_prev = proj_vertex * uniforms.prev_mvpm;
	out.motion_now = out.position;
	out.motion_next = proj_vertex * uniforms.next_mvpm;
	
	return out;
}

static float4 skybox_fs(const skybox_base_in_out& in,
						const_image_cube<float> skybox_tex) {
	return skybox_tex.read_linear(in.cube_tex_coord);
}

fragment auto skybox_scatter_fs(const skybox_scatter_in_out in [[stage_input]],
								const_image_cube<float> skybox_tex) {
	return scene_scatter_fragment_out {
		skybox_fs(in, skybox_tex),
		encode_3d_motion(in.prev_pos.xyz - in.cur_pos.xyz)
	};
}

fragment auto skybox_gather_fs(const skybox_gather_in_out in [[stage_input]],
							   const_image_cube<float> skybox_tex) {
	return scene_gather_fragment_out {
		skybox_fs(in, skybox_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		{
			(in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w),
			(in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w)
		}
	};
}

fragment auto skybox_gather_fwd_fs(const skybox_gather_in_out in [[stage_input]],
								   const_image_cube<float> skybox_tex) {
	return scene_scatter_fragment_out /* reuse */ {
		skybox_fs(in, skybox_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}

//////////////////////////////////////////
// manual blitting

struct blit_in_out {
	float4 position [[position]];
};

vertex blit_in_out blit_vs() {
	switch(vertex_id) {
		case 0: return {{ 1.0f, 1.0f, 0.0f, 1.0f }};
		case 1: return {{ 1.0f, -3.0f, 0.0f, 1.0f }};
		case 2: return {{ -3.0f, 1.0f, 0.0f, 1.0f }};
		default: floor_unreachable();
	}
}

fragment float4 blit_fs(const blit_in_out in [[stage_input]],
						const_image_2d<float> img) {
#if !defined(FLOOR_COMPUTE_VULKAN) // TODO/NOTE: position read in fs not yet supported in vulkan
	return img.read(uint2 { uint32_t(in.position.x), uint32_t(in.position.y) });
#else
	return img.read(frag_coord.xy.cast<uint32_t>());
#endif
}
fragment float4 blit_swizzle_fs(const blit_in_out in [[stage_input]],
								const_image_2d<float> img) {
#if !defined(FLOOR_COMPUTE_VULKAN) // TODO/NOTE: position read in fs not yet supported in vulkan
	return img.read(uint2 { uint32_t(in.position.x), uint32_t(in.position.y) }).swizzle<2, 1, 0, 3>();
#else
	return img.read(frag_coord.xy.cast<uint32_t>()).swizzle<2, 1, 0, 3>();
#endif
}

#endif
