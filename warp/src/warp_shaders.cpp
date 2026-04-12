/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include "warp_shaders.hpp"

// graphics backends only
#if defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_GRAPHICS_HOST_COMPUTE)

FLOOR_PUSH_AND_IGNORE_WARNING(unused-macros)

#if !defined(WARP_SHADOW_FAR_PLANE)
#define WARP_SHADOW_FAR_PLANE 260.0f
#endif

#if !defined(WARP_NEAR_PLANE)
#define WARP_NEAR_PLANE 0.5f
#endif

#if !defined(WARP_FAR_PLANE)
#define WARP_FAR_PLANE 500.0f
#endif

#if !defined(WARP_SCREEN_WIDTH)
#define WARP_SCREEN_WIDTH 1280
#endif

#if !defined(WARP_SCREEN_HEIGHT)
#define WARP_SCREEN_HEIGHT 720
#endif

#if !defined(WARP_SCREEN_FOV)
#define WARP_SCREEN_FOV 72.0f
#endif

#if !defined(WARP_REVERSE_Z)
#define WARP_REVERSE_Z 0
#endif

#if !defined(WARP_AO_HALF_RES)
#define WARP_AO_HALF_RES 0
#endif

#if !defined(WARP_AO_RES_SCALER)
#define WARP_AO_RES_SCALER 1
#endif

// if set, only render AO
#define WARP_DEBUG_AO 0

using namespace warp_shaders;

//////////////////////////////////////////
// scene

struct scene_base_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
	float3 normal;
	float3 view_dir;
	float3 light_dir;
	uint32_t material_data;
};

struct scene_scatter_in_out : scene_base_in_out {
	float3 motion;
};

struct scene_gather_in_out : scene_base_in_out {
	float4 motion_prev;
	float4 motion_now;
	float4 motion_next;
};

// NOTE: color location indices are generated automatically
struct scene_scatter_fragment_out {
	float4 color;
	float2 normal;
	uint32_t motion;
};

struct scene_gather_fragment_out {
	float4 color;
	float2 normal;
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
static inline float4 log_depth(float4 transformed_position) {
#if !WARP_REVERSE_Z
	// using the [0, 1] Z variant here
	// -> 2013 w/ optimized log and no near
	//static constexpr const float C = WARP_SHADOW_NEAR_PLANE; // 1.0
	static constexpr const float far_plane = WARP_SHADOW_FAR_PLANE; // 260.0
	static constexpr const float f_coef = 1.0f / math::log2(far_plane + 1.0f);
	transformed_position.z = math::log2(math::max(1.0e-6f, transformed_position.w + 1.0f)) * f_coef;
	transformed_position.z *= transformed_position.w;
	return transformed_position;
#else // not needed with reverse Z
	return transformed_position;
#endif
}

//! normal offset shadows
//! ref: https://web.archive.org/web/20170523200816/https://www.dissidentlogic.com/old/#Notes%20on%20the%20Normal%20Offset%20Materials
static inline float3 normal_offset_shadow(const float3 pos, const float3 normal, const float3 light_dir) {
	static constexpr const float4 shadow_offset_params { 0.5f, 8.0f, 0.0f, 0.75f };
	const auto NdotL = math::clamp(normal.dot(light_dir) * shadow_offset_params.y,
								   shadow_offset_params.z, shadow_offset_params.w);
	const auto offset = normal * NdotL * shadow_offset_params.x;
	return pos - offset;
}

//! encodes a normalized (unit sphere) 3D vector normal into a 2D vector
//! props to Martin Mittring
static inline float2 encode_normal(const float3 normal) {
	// NOTE: to work around general compiler optimizations/assumptions and floating point
	// inaccuracies, abs() the inner sqrt result (in case normal.z is slightly off) and
	// check if normal.xy is 0 (assume (1, 0) for encoding purposes then)
	return ((normal.xy.dot() > 0.0f ? normal.xy.normalized() : float2 { 1.0f, 0.0f }) *
			math::sqrt(math::abs(-normal.z * 0.5f + 0.5f))) * 0.5f + 0.5f;
}

//! decodes a previously encoded normal from 2D back to a normalized (unit sphere) 3D vector normal
//! NOTE: only using half precision here, because the storage format is half
static inline half3 decode_normal(const half2 encoded_normal) {
	const auto enc = encoded_normal * 2.0_h - 1.0_h;
	half3 normal;
	normal.z = -(enc.dot() * 2.0_h - 1.0_h);
	// again: abs() inner sqrt result, because of floating point inaccuracies
	normal.xy = (enc.dot() > 0.0_h ? enc.normalized() * math::sqrt(math::abs(1.0_h - normal.z * normal.z)) : half2 { 0.0_h, 0.0_h });
	return normal.normalized();
}

#if FLOOR_DEVICE_INFO_TESSELLATION_SUPPORT
kernel void tess_update_factors(arg_buffer<model_data_t> model_data,
								buffer<triangle_tessellation_levels_t> factors,
								param<float3> cam_position,
								param<float> max_tess_factor) {
	if (global_id.x >= model_data.triangle_count) {
		return;
	}
	const auto& vertices = model_data.pnbt[model_data_t::POSITION];
	const auto& indices = model_data.indices;
	const auto& materials_data = model_data.materials_data;
	
	const auto idx = indices[global_id.x];
	half factor = 1.0f;
	if ((materials_data[idx.x] & 0xFFFF'0000u) != 0u) {
		const float3 distances_sq {
			(vertices[idx.x] - cam_position).dot(),
			(vertices[idx.y] - cam_position).dot(),
			(vertices[idx.z] - cam_position).dot(),
		};
		const auto tri_area = (vertices[idx.z] - vertices[idx.x]).crossed(vertices[idx.z] - vertices[idx.y]).dot();
		const auto factor_scaler = math::max(tri_area, 1.5f); // ensure large triangles get larger factors
		const auto min_distance = math::sqrt(distances_sq.min_element());
		factor = (half)math::clamp(max_tess_factor * factor_scaler * math::exp(-0.06f * min_distance), 1.0f, max_tess_factor);
	}
	
	factors[global_id.x] = triangle_tessellation_levels_t {
		.outer = {
			factor,
			factor,
			factor,
		},
		.inner = factor,
	};
}
#else
kernel void tess_update_factors(buffer<const uint32_t>) {
	// nop
}
#endif

struct __attribute__((packed)) control_point_t {
	float3 position;
	float2 tex_coord;
	float3 normal;
	float3 binormal;
	float3 tangent;
	uint32_t material_data;
};

struct patch_in_t {
	patch_control_point<control_point_t> control_points;
};

static void scene_vs(scene_base_in_out& out,
					 const float3& in_position,
					 const float2& in_tex_coord,
					 const float3& in_normal,
					 const float3& in_binormal,
					 const float3& in_tangent,
					 const uint32_t& in_material_data,
					 const matrix4f& mvpm,
					 const matrix4f& light_bias_mvpm,
					 const float3& cam_pos,
					 const float3& light_pos) {
	out.tex_coord = in_tex_coord;
	
	float4 pos { in_position, 1.0f };
	out.shadow_coord = log_depth(pos * light_bias_mvpm);
	out.position = pos * mvpm;
	
	const auto vview = cam_pos - in_position;
	out.view_dir.x = vview.dot(in_tangent);
	out.view_dir.y = vview.dot(in_binormal);
	out.view_dir.z = vview.dot(in_normal);
	
	const auto vlight = light_pos - in_position;
	out.light_dir.x = vlight.dot(in_tangent);
	out.light_dir.y = vlight.dot(in_binormal);
	out.light_dir.z = vlight.dot(in_normal);
	
	out.normal = in_normal;
	
	out.material_data = in_material_data;
}

vertex auto scene_scatter_vs(arg_buffer<model_data_t> model_data,
							 buffer<const frame_uniforms_t> uniforms) {
	scene_scatter_in_out out;
	const auto vtx = model_data.pnbt[model_data_t::POSITION][vertex_id];
	scene_vs(out, vtx,
			 model_data.tc[vertex_id],
			 model_data.pnbt[model_data_t::NORMAL][vertex_id],
			 model_data.pnbt[model_data_t::BINORMAL][vertex_id],
			 model_data.pnbt[model_data_t::TANGENT][vertex_id],
			 model_data.materials_data[vertex_id],
			 uniforms->mvpm, uniforms->light_bias_mvpm, uniforms->cam_pos, uniforms->light_pos);
	
	float4 pos { vtx, 1.0f };
	float4 prev_pos = pos * uniforms->scatter.scene_prev_mvm;
	float4 cur_pos = pos * uniforms->scatter.scene_mvm;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}

#if FLOOR_DEVICE_INFO_TESSELLATION_SUPPORT
[[patch(triangle, 3)]]
tessellation_evaluation auto scene_scatter_tes(patch_in_t in [[stage_input]],
											   arg_buffer<materials_t> materials,
											   buffer<const frame_uniforms_t> uniforms) {
	// interpolate vertex position from barycentric coordinate and control points
	auto vertex_pos = (in.control_points[0].position * position_in_patch.x +
					   in.control_points[1].position * position_in_patch.y +
					   in.control_points[2].position * position_in_patch.z);
	auto tex_coord = (in.control_points[0].tex_coord * position_in_patch.x +
					  in.control_points[1].tex_coord * position_in_patch.y +
					  in.control_points[2].tex_coord * position_in_patch.z);
	auto normal = (in.control_points[0].normal * position_in_patch.x +
				   in.control_points[1].normal * position_in_patch.y +
				   in.control_points[2].normal * position_in_patch.z);
	auto binormal = (in.control_points[0].binormal * position_in_patch.x +
					 in.control_points[1].binormal * position_in_patch.y +
					 in.control_points[2].binormal * position_in_patch.z);
	auto tangent = (in.control_points[0].tangent * position_in_patch.x +
					in.control_points[1].tangent * position_in_patch.y +
					in.control_points[2].tangent * position_in_patch.z);
	auto material_data = in.control_points[0].material_data;
	auto material = material_data & 0xFFFFu; // material is the same for all
	const bool should_displace = ((material_data & 0xFFFF'0000u) != 0u);
	
	if (should_displace && uniforms->displacement_mode == 2) {
		static constexpr constant const float max_displacement { 0.25f };
		auto displacement = materials.disp_tex[material].read_linear_repeat(tex_coord) * 2.0f - 1.0f; // [-1, 1]
		vertex_pos += max_displacement * displacement * normal.normalized();
	}
	
	scene_scatter_in_out out;
	scene_vs(out, vertex_pos, tex_coord, normal, binormal, tangent, material_data,
			 uniforms->mvpm, uniforms->light_bias_mvpm, uniforms->cam_pos, uniforms->light_pos);
	
	float4 pos { vertex_pos, 1.0f };
	float4 prev_pos = pos * uniforms->scatter.scene_prev_mvm;
	float4 cur_pos = pos * uniforms->scatter.scene_mvm;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}
#endif

// also used for forward-only
vertex auto scene_gather_vs(arg_buffer<model_data_t> model_data,
							buffer<const frame_uniforms_t> uniforms) {
	scene_gather_in_out out;
	const auto vtx = model_data.pnbt[model_data_t::POSITION][vertex_id];
	scene_vs(out, vtx,
			 model_data.tc[vertex_id],
			 model_data.pnbt[model_data_t::NORMAL][vertex_id],
			 model_data.pnbt[model_data_t::BINORMAL][vertex_id],
			 model_data.pnbt[model_data_t::TANGENT][vertex_id],
			 model_data.materials_data[vertex_id],
			 uniforms->mvpm, uniforms->light_bias_mvpm, uniforms->cam_pos, uniforms->light_pos);
	
	float4 pos { vtx, 1.0f };
	out.motion_prev = pos * uniforms->gather.scene_prev_mvpm;
	out.motion_now = pos * uniforms->mvpm;
	out.motion_next = pos * uniforms->gather.scene_next_mvpm;
	
	return out;
}

#if FLOOR_DEVICE_INFO_TESSELLATION_SUPPORT
[[patch(triangle, 3)]]
tessellation_evaluation auto scene_gather_tes(patch_in_t in [[stage_input]],
											  arg_buffer<materials_t> materials,
											  buffer<const frame_uniforms_t> uniforms) {
	// interpolate vertex position from barycentric coordinate and control points
	auto vertex_pos = (in.control_points[0].position * position_in_patch.x +
					   in.control_points[1].position * position_in_patch.y +
					   in.control_points[2].position * position_in_patch.z);
	auto tex_coord = (in.control_points[0].tex_coord * position_in_patch.x +
					  in.control_points[1].tex_coord * position_in_patch.y +
					  in.control_points[2].tex_coord * position_in_patch.z);
	auto normal = (in.control_points[0].normal * position_in_patch.x +
				   in.control_points[1].normal * position_in_patch.y +
				   in.control_points[2].normal * position_in_patch.z);
	auto binormal = (in.control_points[0].binormal * position_in_patch.x +
					 in.control_points[1].binormal * position_in_patch.y +
					 in.control_points[2].binormal * position_in_patch.z);
	auto tangent = (in.control_points[0].tangent * position_in_patch.x +
					in.control_points[1].tangent * position_in_patch.y +
					in.control_points[2].tangent * position_in_patch.z);
	auto material_data = in.control_points[0].material_data;
	auto material = material_data & 0xFFFFu; // material is the same for all
	const bool should_displace = ((material_data & 0xFFFF'0000u) != 0u);
	
	if (should_displace && uniforms->displacement_mode == 2) {
		static constexpr constant const float max_displacement { 0.25f };
		auto displacement = materials.disp_tex[material].read_linear_repeat(tex_coord) * 2.0f - 1.0f; // [-1, 1]
		vertex_pos += max_displacement * displacement * normal.normalized();
	}
	
	scene_gather_in_out out;
	scene_vs(out, vertex_pos, tex_coord, normal, binormal, tangent, material_data,
			 uniforms->mvpm, uniforms->light_bias_mvpm, uniforms->cam_pos, uniforms->light_pos);
	
	float4 pos { vertex_pos, 1.0f };
	out.motion_prev = pos * uniforms->gather.scene_prev_mvpm;
	out.motion_now = pos * uniforms->mvpm;
	out.motion_next = pos * uniforms->gather.scene_next_mvpm;
	
	return out;
}
#endif

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

static std::pair<float4, float2> scene_fs(const scene_base_in_out& in,
										  const uint32_t disp_mode,
										  const const_image_2d<float>& diff_tex,
										  const const_image_2d<float>& spec_tex,
										  const const_image_2d<float>& norm_tex,
										  const const_image_2d<float1>& mask_tex,
										  const const_image_2d<float1>& disp_tex,
										  const const_image_2d_depth<float>& shadow_tex) {
	const auto in_tex_coord = in.tex_coord;
	const auto norm_view_dir = in.view_dir.normalized();

	// mask out
	if (mask_tex.read_lod_linear_repeat(in_tex_coord, 0) < 0.5f) {
		discard_fragment();
	}
	
	// compute parallax tex coord
	static constexpr constant const float parallax { 0.035f };
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
		static constexpr const COMPARE_FUNCTION compare_func {
			WARP_REVERSE_Z ? COMPARE_FUNCTION::GREATER : COMPARE_FUNCTION::LESS
		};
		
		const auto shadow_coord = in.shadow_coord.xyz / in.shadow_coord.w;
		
#if 0
		if (in.shadow_coord.w > 0.0f) {
			light_vis = shadow_tex.compare_linear<compare_func>(shadow_coord.xy, shadow_coord.z);
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
			light_vis -= 0.0625f * (1.0f - shadow_tex.compare_linear<compare_func>(shadow_coord.xy + poisson_coord * coord_factor,
																				   shadow_coord.z));
		}
#endif
		
		// diffuse
		lighting += math::max(lambert, 0.0f);
		
		// specular
		const auto spec = spec_tex.read_gradient_linear_repeat(tex_coord, tex_grad).x;
		const auto specular = math::pow(math::max(norm_half_dir.dot(normal), 0.0f), spec * 10.0f);
		lighting += math::max(specular, 0.0f);
		
		// mul with shadow and light attenuation
		lighting *= light_vis * attenuation;
	}
	lighting = math::max(lighting, ambient);
	
	auto color = diff.xyz * lighting;
#if defined(WARP_APPLY_GAMMA)
	// apply fixed inv gamma for non-wide-gamut/non-HDR displays
	// NOTE: not really accurate, but good enough
	color.pow(1.0f / 2.2f);
#endif
	
	// compute the world space normal that we will use for ambient occlusion
	const auto surf_normal = in.normal.normalized();
	const auto tangent = (surf_normal.crossed(math::abs(surf_normal.y) >= 0.9f ? float3 { 1.0f, 0.0f, 0.0f } : float3 { 0.0f, 1.0f, 0.0f }));
	const auto binormal = surf_normal.crossed(tangent);
	const auto out_normal = (tangent * normal.x + binormal * normal.y + surf_normal * normal.z).normalized();
	
	return { float4 { color, 1.0f }, encode_normal(out_normal) };
}

// argument-buffer + indirect variant: put all material textures into a single argument buffer + all parameters are contained within buffers

fragment auto scene_scatter_fs(const scene_scatter_in_out in [[stage_input]],
							   buffer<const frame_uniforms_t> uniforms,
							   arg_buffer<materials_t> materials,
							   arg_buffer<scene_fs_data_t> scene_fs_data) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	const auto [color, normal] = scene_fs(in, should_displace ? uniforms->displacement_mode : 0u, materials.diff_tex[mat_idx],
										  materials.spec_tex[mat_idx], materials.norm_tex[mat_idx], materials.mask_tex[mat_idx],
										  materials.disp_tex[mat_idx], scene_fs_data.shadow_tex);
	return scene_scatter_fragment_out {
		color, normal, encode_3d_motion(in.motion)
	};
}

fragment auto scene_gather_fs(const scene_gather_in_out in [[stage_input]],
							  buffer<const frame_uniforms_t> uniforms,
							  arg_buffer<materials_t> materials,
							  arg_buffer<scene_fs_data_t> scene_fs_data) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	const auto [color, normal] = scene_fs(in, should_displace ? uniforms->displacement_mode : 0u, materials.diff_tex[mat_idx],
										  materials.spec_tex[mat_idx], materials.norm_tex[mat_idx], materials.mask_tex[mat_idx],
										  materials.disp_tex[mat_idx], scene_fs_data.shadow_tex);
	return scene_gather_fragment_out {
		color, normal,
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		{
			(in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w),
			(in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w)
		}
	};
}

fragment auto scene_gather_fwd_fs(const scene_gather_in_out in [[stage_input]],
								  buffer<const frame_uniforms_t> uniforms,
								  arg_buffer<materials_t> materials,
								  arg_buffer<scene_fs_data_t> scene_fs_data) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	const auto [color, normal] = scene_fs(in, should_displace ? uniforms->displacement_mode : 0u, materials.diff_tex[mat_idx],
										  materials.spec_tex[mat_idx], materials.norm_tex[mat_idx], materials.mask_tex[mat_idx],
										  materials.disp_tex[mat_idx], scene_fs_data.shadow_tex);
	return scene_scatter_fragment_out /* reuse */ {
		color, normal,
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}

//////////////////////////////////////////
// shadow map

struct shadow_in_out {
	float4 position [[position]];
};

vertex shadow_in_out shadow_vs(buffer<const float3> in_position,
							   buffer<const float3> in_normal,
							   buffer<const frame_uniforms_t> uniforms) {
	auto vtx = in_position[vertex_id];
	const auto light_dir = (uniforms->light_pos - vtx).normalized();
	vtx = normal_offset_shadow(vtx, in_normal[vertex_id], light_dir);
	return {
		log_depth(float4 { vtx, 1.0f } * uniforms->light_mvpm)
	};
}

//////////////////////////////////////////
// ambient occlusion

// NOTE: taken from libwarp (see warp_kernels.hpp for more explanations)
namespace warp_camera {
static constexpr const float2 screen_size { float(WARP_SCREEN_WIDTH / WARP_AO_RES_SCALER), float(WARP_SCREEN_HEIGHT / WARP_AO_RES_SCALER) };
static constexpr const float2 inv_screen_size { 1.0f / screen_size };
static constexpr const float aspect_ratio { screen_size.x / screen_size.y };
static constexpr const float _up_vec { float(const_math::tan(const_math::deg_to_rad(double(WARP_SCREEN_FOV)) * 0.5)) };
static constexpr const float right_vec { _up_vec * aspect_ratio };
static constexpr const float up_vec { -_up_vec };
static constexpr const float2 near_far_plane { WARP_NEAR_PLANE, WARP_FAR_PLANE };

static constexpr inline float linearize_depth(const float depth) {
#if !WARP_REVERSE_Z
	// reading from the actual depth buffer which is normalized in [0, 1]
	// NOTE: for Vulkan/Metal right-handed projection matrix
	constexpr const float2 near_far_projection {
		-near_far_plane.y / (near_far_plane.y - near_far_plane.x),
		(-near_far_plane.y * near_far_plane.x) / (near_far_plane.y - near_far_plane.x),
	};
	// special case: clear/full depth, assume this comes from a normalized sky box
	return (depth == 1.0f ? 1.0f : near_far_projection.y / (depth + near_far_projection.x));
#else
	// reading from the actual depth buffer which is normalized in [0, 1]
	constexpr const float2 near_far_projection {
		-near_far_plane.y / (near_far_plane.x - near_far_plane.y) - 1.0f,
		(-near_far_plane.x * near_far_plane.y) / (near_far_plane.x - near_far_plane.y),
	};
	// special case: clear/full depth, assume this comes from a normalized sky box
	return (depth == 0.0f ? 0.0f : near_far_projection.y / (depth + near_far_projection.x));
#endif
}

static constexpr inline float3 reconstruct_position(const int2 coord, const float linear_depth) {
	constexpr const auto ce_term_1 = inv_screen_size * float2(right_vec, up_vec);
	constexpr const auto ce_term_2 = 2.0f * ce_term_1;
	constexpr const auto ce_term_3 = ce_term_1 - float2(right_vec, up_vec);
	return {
		(float2(coord) * ce_term_2 + ce_term_3) * linear_depth,
		-linear_depth
	};
}

static constexpr inline float2 reproject_position(const float3 position) {
	constexpr const auto ce_term = float2 { 1.0f / right_vec, 1.0f / up_vec };
	const auto proj_dst_coord = (position.xy * ce_term) / -position.z;
	return ((proj_dst_coord * 0.5f + 0.5f) * screen_size);
}

} // namespace warp_camera
using namespace warp_camera;

//! computes a single pseudo-random number (white noise) via hashing from a given 1D or 2D uint/float input,
//! return type can be any of { uint, half, float }, defaulting to float
//! ref: https://nullprogram.com/blog/2018/07/31/
//! ref: https://www.shadertoy.com/view/WttXWX
//! ref: https://github.com/skeeto/hash-prospector/issues/19#issuecomment-1120105785
//! NOTE: when given a 2D uint input, only the lower 16 bits of the second component are used
template <typename ret_type = float, typename input_type>
requires ((std::is_same_v<ret_type, uint32_t> || std::is_same_v<ret_type, float> || std::is_same_v<ret_type, half>) &&
		  (std::is_same_v<input_type, uint32_t> || std::is_same_v<input_type, uint2> || std::is_same_v<input_type, float> || std::is_same_v<input_type, float2>))
static constexpr inline ret_type hash(const input_type input) {
	// input handling
	uint32_t x {};
	if constexpr (std::is_same_v<input_type, uint32_t>) {
		x = input;
	} else if constexpr (std::is_same_v<input_type, uint2>) {
		x = input.x + (input.y << 16u);
	} else if constexpr (std::is_same_v<input_type, float>) {
		x = std::bit_cast<uint32_t>(input);
	} else if constexpr (std::is_same_v<input_type, float2>) {
		x = std::bit_cast<uint32_t>(input.x) + (std::bit_cast<uint32_t>(input.y) << 16u);
	} else {
		instantiation_trap_dependent_type(input_type, "unhandled input type");
	}
	
	// hashing
	x ^= x >> 16u;
	x *= 0x21F0'AAADu;
	x ^= x >> 15u;
	x *= 0x735A'2D97u;
	x ^= x >> 15u;
	
	// output handling
	if constexpr (std::is_same_v<ret_type, uint32_t>) {
		return x;
	} else if constexpr (std::is_same_v<ret_type, float> || std::is_same_v<ret_type, half>) {
		return ret_type(float(x) * (1.0f / float(0xFFFF'FFFFu)));
	} else {
		instantiation_trap_dependent_type(ret_type, "unhandled return type");
		return {};
	}
}

//! ref: https://arxiv.org/abs/2301.11376
//! ref: https://www.shadertoy.com/view/4cdfzf (TinyTexel) / https://twitter.com/Mirko_Salm/status/1865464707282243843
template <typename depth_image_type>
static inline float compute_ao(const depth_image_type& depth_buffer, const frame_uniforms_t& params,
							   const float3 spos, const float3 normal_in) {
	// NOTE: 2 is the bare minimum to hide sampling artifacts/patterns during denoising
	static constexpr const uint32_t dir_count { 3u };
#if 1 // 3x16 samples ought to be enough for everyone
	static constexpr const bool is_16_bit_vbao { true };
	// NOTE: double thickness looks better when we just have 16 sample directions
	static constexpr const float thickness { 1.0f };
#else
	static constexpr const bool is_16_bit_vbao { false };
	static constexpr const float thickness { 0.5f };
#endif
	static constexpr const float ray_marching_width { 384.0f / float(WARP_AO_RES_SCALER) };
	static constexpr const uint32_t sample_count { is_16_bit_vbao ? 16u : 32u };
	static constexpr const float sample_countf { float(sample_count) };
	static constexpr const float sample_count_normalizer { 1.0f / float(sample_count) };
	
	const auto normal_view_space = normal_in * params.rmvm;
	
	// need a larger offset here when using uniform hemisphere weighting due to increased potential for self-shadowing
	const auto pos_view_space = reconstruct_position(spos.xy, spos.z) + normal_view_space * (4.0f / 1024.0f);
	
	const auto view_vec = -pos_view_space.normalized();
	
	const auto ray_start = reproject_position(pos_view_space);
	
	float ao = 0.0f;
#pragma clang loop unroll_count(dir_count < 3u ? dir_count : 3u) // unroll, but not too much
	for (uint32_t i = 0u; i < dir_count; ++i) {
#if 0 // NOTE/TODO: this should be enabled when using temporal averaging/denoising/anti-aliasing/...
		const auto n = params.frame_idx * dir_count + (i + 1u);
		const auto white_noise_n = 48271u * n;
		const auto ign_n = n;
#else // for now, with spatial denoising only, use fixed frame-independent RNG (rather want static noise than boiling)
		// better for white noise
		const auto white_noise_n = 48271u * (i + 1u);
		// better for IGN
		const auto ign_n = (i + 1u);
#endif
		const auto ign_uv = spos.xy + 5.588238f * float(ign_n);
		const auto rng_slice_dir = math::fractional(52.9829189f * math::fractional(ign_uv.dot(float2(0.06711056f, 0.00583715f))));
		
		float2 rng_jitter {
			hash(std::bit_cast<uint32_t>(spos.x) ^ (white_noise_n << 16u) ^ std::bit_cast<uint32_t>(rng_slice_dir)),
			hash(std::bit_cast<uint32_t>(spos.y) ^ (white_noise_n << 16u) ^ std::bit_cast<uint32_t>(rng_slice_dir)),
		};
		
		// slice direction sampling
		float3 sample_dir_view_space;
		float2 dir; // screen space sampling vector
		{
			dir = float2(math::cos(rng_slice_dir * const_math::PI<float>), math::sin(rng_slice_dir * const_math::PI<float>));
			
			// set up view vector space -> view space mapping
			// NOTE: equivalent to quaternionf::rotation_from_to_vector({ 0.0f, 0.0f, -1.0f }, view_vec)
			const float3 xyz { view_vec.y, -view_vec.x, 0.0f }; // cross(from, to)
			const auto s = -view_vec.z; // dot(from, to)
			const auto u = math::rsqrt(math::max(s * 0.5f + 0.5f, 0.0f)); // rcp(cosine half-angle formula)
			const float4 q_to_view { xyz * u * 0.5f, 1.0f / u };
			
			// transform v.xy0 by unit quaternion q.xy0s
			const auto o = q_to_view.x * dir.y;
			const auto c = q_to_view.y * dir.x;
			const float3 b { o - c, -o + c, o - c };
			sample_dir_view_space = float3 { dir, 0.0f } + 2.0f * b * float3 { q_to_view.y, q_to_view.x, q_to_view.w };
			
			const auto ray_end = reproject_position(pos_view_space + sample_dir_view_space * (WARP_NEAR_PLANE * 0.5f));
			dir = (ray_end - ray_start).normalized();
		}
		
		// construct slice
		const auto slice_n = view_vec.crossed(sample_dir_view_space);
		const auto proj_n = normal_view_space - slice_n * normal_view_space.dot(slice_n);
		const auto proj_n_sqr_len = proj_n.dot();
		if (proj_n_sqr_len == 0.0f) {
			return 1.0f;
		}
		const auto proj_n_inv_len = math::rsqrt(proj_n_sqr_len);
		const auto sin_n = slice_n.crossed(proj_n).dot(view_vec) * proj_n_inv_len;
		
		// find horizons
		std::conditional_t<is_16_bit_vbao, uint16_t, uint32_t> occlusion_bits = 0u;
#pragma unroll
		for (float d = -1.0f; d <= 1.0f; d += 2.0f) {
			const auto ray_dir = dir * d;
			
			static constexpr const float s { const_math::pow(ray_marching_width, sample_count_normalizer) };
			float t = math::pow(s, rng_jitter.x); // init t: [1, s]
			rng_jitter.x = 1.0f - rng_jitter.x;
			
#pragma unroll
			for (uint32_t j = 0u; j < sample_count; ++j) {
				const auto sample_pos = ray_start + ray_dir * t;
				t *= s;
				
				// handle oob
				const auto sample_pos_u = sample_pos.cast<uint32_t>();
				if (sample_pos.x < 0.0f || sample_pos_u.x >= params.ao_screen_dim.x ||
					sample_pos.y < 0.0f || sample_pos_u.y >= params.ao_screen_dim.y) {
					continue;
				}
				
				const auto raw_depth = depth_buffer.read(sample_pos_u);
				// handle sky
FLOOR_PUSH_AND_IGNORE_WARNING(float-equal) // this must be an exact match
				static constexpr const float sky_depth { !WARP_REVERSE_Z ? 1.0f : 0.0f };
				if (raw_depth == sky_depth) {
					continue;
				}
FLOOR_POP_WARNINGS()
				
				const auto sample_depth = linearize_depth(raw_depth);
				const auto sample_posVS = reconstruct_position(sample_pos, sample_depth);
				
				const auto delta_pos_front = sample_posVS - pos_view_space;
				auto delta_pos_back = delta_pos_front - view_vec * thickness;
				
				// required for correctness (actually needed)
				delta_pos_back = reconstruct_position(sample_pos, sample_depth + thickness) - pos_view_space;
				
				// project samples onto unit circle and compute cos(angles) relative to the view vector
				float2 horizon_cos {
					delta_pos_front.normalized().dot(view_vec),
					delta_pos_back.normalized().dot(view_vec)
				};
				// sampling direction flips min/max cos(angles)
				horizon_cos = { d >= 0.0f ? horizon_cos.x : horizon_cos.y, d >= 0.0f ? horizon_cos.y : horizon_cos.x };
				
				// map to slice relative distribution
				const auto d_half = d * 0.5f;
				float2 horizon_norm = ((0.5f + 0.5f * sin_n) + d_half) - d_half * horizon_cos;
				// jitter sample locations + clamp to [0, 1]
				horizon_norm = (horizon_norm + rng_jitter.y * sample_count_normalizer).clamped(0.0f, 1.0f);
				
				// turn arc into bit mask
				// NOTE: this implicitly uses the "floor" variant here
				if constexpr (is_16_bit_vbao) {
					const auto horizon_i = (horizon_norm * sample_countf).cast<uint16_t>();
					const auto mX = uint16_t(horizon_i.x < 16u ? 0xFFFFu << horizon_i.x : 0u);
					const auto mY = uint16_t(horizon_i.y != 0u ? 0xFFFFu >> (16u - horizon_i.y) : 0u);
					occlusion_bits |= mX & mY;
				} else {
					const auto horizon_i = (horizon_norm * sample_countf).cast<uint32_t>();
					const auto mX = (horizon_i.x < 32u ? 0xFFFF'FFFFu << horizon_i.x : 0u);
					const auto mY = (horizon_i.y != 0u ? 0xFFFF'FFFFu >> (32u - horizon_i.y) : 0u);
					occlusion_bits |= mX & mY;
				}
			}
		}
		ao += 1.0f - float(math::popcount(occlusion_bits)) * sample_count_normalizer;
	}
	ao *= 1.0f / float(dir_count);
	
	return ao;
}

kernel_2d(16u, 16u) void scene_ao(const_image_2d<half2> normals_buffer,
								  const_image_2d_depth<float> depth_buffer,
								  image_2d<half1, true> ao_output,
								  buffer<const frame_uniforms_t> params) {
	if ((global_id.xy >= params->ao_screen_dim).any()) {
		return;
	}
	
	const auto depth = depth_buffer.read(global_id.xy);
	const auto normal = decode_normal(normals_buffer.read(global_id.xy));
	const auto spos = float3(global_id.xy.cast<float>() + 0.5f, linearize_depth(depth));
	const auto ao = compute_ao(depth_buffer, *params, spos, normal);
	
	ao_output.write(global_id.xy, half(ao));
}

kernel_2d(16u, 16u) void scene_ao_raw(image_2d<half4> scene_buffer,
									  const_image_2d<half2> normals_buffer,
									  const_image_2d_depth<float> depth_buffer,
									  buffer<const frame_uniforms_t> params) {
	if ((global_id.xy >= params->ao_screen_dim).any()) {
		return;
	}
	
	const auto depth = depth_buffer.read(global_id.xy);
	const auto normal = decode_normal(normals_buffer.read(global_id.xy));
	const auto spos = float3(global_id.xy.cast<float>() + 0.5f, linearize_depth(depth));
	const auto ao = compute_ao(depth_buffer, *params, spos, normal);
	if (ao > 0.0f) {
		if ((params->ao_screen_dim == scene_buffer.dim().xy).all()) [[likely]] {
			auto color = scene_buffer.read(global_id.xy);
#if !WARP_DEBUG_AO
			color.xyz *= half(ao);
#else
			color.xyz = half(ao);
#endif
			scene_buffer.write(global_id.xy, color);
		} else [[unlikely]] {
			// since this is only used for debugging/vis purposes, just use nearest upsampling
			const auto out_coord_offset = global_id.xy * 2u;
#pragma unroll
			for (uint32_t y = 0; y < 2; ++y) {
#pragma unroll
				for (uint32_t x = 0; x < 2; ++x) {
					const auto out_coord = out_coord_offset + uint2 { x, y };
					auto color = scene_buffer.read(out_coord);
#if !WARP_DEBUG_AO
					color.xyz *= half(ao);
#else
					color.xyz = half(ao);
#endif
					scene_buffer.write(out_coord, color);
				}
			}
		}
	}
}

//! ref: https://www.highperformancegraphics.org/previous/www_2010/media/RayTracing_I/HPG2010_RayTracing_I_Dammertz.pdf
//! ref: https://research.nvidia.com/sites/default/files/pubs/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A//svgf_preprint.pdf
//! ref: https://github.com/a2flo/floor_examples/blob/master/img/src/img_kernels.cpp#L77
template <uint32_t denoise_iteration /* [0, 4] */,
		  typename normals_img_type, typename depth_img_type, typename ao_input_img_type>
static inline std::pair<float, uint2> ao_denoise(const normals_img_type& normals_buffer,
												 const depth_img_type& depth_buffer,
												 const ao_input_img_type& ao_buffer,
												 const int2 image_dim) {
	static constexpr const uint32_t step_width { 1u << denoise_iteration };
	static constexpr const int step_width_i { int(step_width) };
	
	static constexpr const float sigma_iteration_inv { float(step_width) }; // == 1 / 2^-i
	// NOTE: we have really good input, so this can be kept pretty low -> keeps a lot more detail
	static constexpr const float sigma_ao { 0.05f };
	static constexpr const float sigma_ao_factor { sigma_iteration_inv / sigma_ao };
	// NOTE: same as the paper sigma (with f32 normals this could be doubled)
	static constexpr const float sigma_normal { 128.0f };
	// NOTE: changing this has suprisingly little effect -> keep it fairly low for now
	static constexpr const float sigma_depth { 4.0f };
	static constexpr const float sigma_depth_factor { sigma_iteration_inv / sigma_depth };
	
	static constexpr const uint32_t lateral_dim { 16u };
	static constexpr const uint32_t work_group_size { lateral_dim * lateral_dim };
	
#if 1
	static constexpr const uint32_t tap_count { 5u };
	static constexpr const int overlap { int(tap_count) / 2 };
	// 1D coeffs (B_3 spline): { 1/16, 1/4, 3/8, 1/4, 1/16 } -> expand/conv to 2D
	static constexpr const float coeffs[tap_count * tap_count] {
		1.0f / 256.0f, 1.0f / 64.0f, 3.0f / 128.0f, 1.0f / 64.0f, 1.0f / 256.0f,
		1.0f /  64.0f, 1.0f / 16.0f, 3.0f /  32.0f, 1.0f / 16.0f, 1.0f /  64.0f,
		3.0f / 128.0f, 3.0f / 32.0f, 9.0f /  64.0f, 3.0f / 32.0f, 3.0f / 128.0f,
		1.0f /  64.0f, 1.0f / 16.0f, 3.0f /  32.0f, 1.0f / 16.0f, 1.0f /  64.0f,
		1.0f / 256.0f, 1.0f / 64.0f, 3.0f / 128.0f, 1.0f / 64.0f, 1.0f / 256.0f
	};
#else // very little difference, but also very little perf improvement
	static constexpr const uint32_t tap_count { 3u };
	static constexpr const int overlap { int(tap_count) / 2 };
	static constexpr const float coeffs[tap_count * tap_count] {
		1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f,
		2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f,
		1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f,
	};
#endif
	
	const uint2 lid = local_id.xy;
	
	// this uses local memory as a sample + compute cache
	static constexpr const auto sample_count_x = lateral_dim + 2u * uint32_t(overlap);
	static constexpr const auto sample_count_y = sample_count_x;
	static constexpr const auto sample_count = sample_count_x * sample_count_y;
	local_buffer<half4, sample_count> samples_normal_and_ao;
	local_buffer<float, sample_count> samples_depth;
	
	const auto inner_offset = group_id.xy % (1u << denoise_iteration);
	const auto outer_offset = group_id.xy / (1u << denoise_iteration);
	const auto group_offset = ((inner_offset + outer_offset * (lateral_dim << denoise_iteration)).cast<int>() -
							   overlap * step_width_i);
	
	// read the input pixels and store them in the local buffer/"cache"
	// -> since we have more samples than work-items, we loop over the sample count and let the work-items
	//    read data multiple times to keep everything occupied as best as possible
	for (uint32_t sample_idx = lid.x + lid.y * lateral_dim; sample_idx < sample_count; sample_idx += work_group_size) {
		// compute the image coordinate for this sample index (with manual clamp-to-edge)
		const int2 img_coord {
			math::clamp(int(sample_idx % sample_count_x) * step_width_i + group_offset.x, 0, image_dim.x - 1),
			math::clamp(int(sample_idx / sample_count_x) * step_width_i + group_offset.y, 0, image_dim.y - 1),
		};
		samples_normal_and_ao[sample_idx] = {
			decode_normal(normals_buffer.read(img_coord)), ao_buffer.read(img_coord)
		};
		samples_depth[sample_idx] = linearize_depth(depth_buffer.read(img_coord));
	}
	// ensure the complete tile has been read and stored
	local_barrier();
	
	const auto base_idx = lid.x + lid.y * sample_count_x;
	const auto center_idx = base_idx + overlap + overlap * sample_count_x;
	const auto center_ao_and_normal = samples_normal_and_ao[center_idx];
	const auto center_ao = float(center_ao_and_normal.w);
	const auto center_normal = center_ao_and_normal.xyz;
	const auto center_depth = samples_depth[center_idx];
	
	const auto output_pos = (inner_offset + outer_offset * (lateral_dim << denoise_iteration)) + lid * step_width;
	float output_ao = 0.0f;
	if ((output_pos < image_dim.cast<uint32_t>()).all()) {
		float sum_ao = 0.0f;
		float sum_weights = 0.0f;
#pragma unroll
		for (uint32_t y = 0u; y < tap_count; ++y) {
#pragma unroll
			for (uint32_t x = 0u; x < tap_count; ++x) {
				const auto sample_idx = base_idx + x + y * sample_count_x;
				const auto sample_ao_and_normal = samples_normal_and_ao[sample_idx];
				const auto ao = float(sample_ao_and_normal.w);
				const auto normal = sample_ao_and_normal.xyz;
				const auto depth = samples_depth[sample_idx];
				
				// weight AO
				const auto diff_ao = ao - center_ao;
				const auto diff_ao_sqr = diff_ao * diff_ao;
				const auto weight_ao = math::min(math::exp(-diff_ao_sqr * sigma_ao_factor), 1.0f);
				
				// weight normal
				const auto weight_normal = math::pow(math::max(float(normal.dot(center_normal)), 0.0f),
													 sigma_normal * sigma_iteration_inv);
				
				// weight depth
				const auto diff_depth = depth - center_depth;
				const auto diff_depth_sqr = diff_depth * diff_depth;
				const auto weight_depth = math::min(math::exp(-diff_depth_sqr * sigma_depth_factor), 1.0f);
				
				// combine all
				const auto weight = weight_ao * weight_normal * weight_depth;
				const auto coeff = coeffs[x + y * tap_count];
				sum_ao += ao * weight * coeff;
				sum_weights += weight * coeff;
			}
		}
		output_ao = sum_ao / sum_weights;
	}
	return { output_ao, output_pos };
}

#define AO_DENOISE_ITER_KERNEL(iteration) \
kernel_2d(16u, 16u) void scene_ao_denoise_ ## iteration(const_image_2d<half2> normals_buffer, \
														const_image_2d_depth<float> depth_buffer, \
														const_image_2d<half1> ao_buffer_in, \
														image_2d<half1, true> ao_buffer_out, \
														buffer<const frame_uniforms_t> params) { \
	const auto [ao, output_pos] = ao_denoise<iteration>(normals_buffer, depth_buffer, ao_buffer_in, params->ao_screen_dim); \
	if ((output_pos < params->ao_screen_dim).all()) { \
		ao_buffer_out.write(output_pos, half(ao)); \
	} \
}
AO_DENOISE_ITER_KERNEL(0)
AO_DENOISE_ITER_KERNEL(1)
AO_DENOISE_ITER_KERNEL(2)
AO_DENOISE_ITER_KERNEL(3)
AO_DENOISE_ITER_KERNEL(4) // only used when upsampling

// only used with full resolution AO
kernel_2d(16u, 16u) void scene_ao_denoise_final(image_2d<half4> scene_buffer,
												const_image_2d<half2> normals_buffer,
												const_image_2d_depth<float> depth_buffer,
												const_image_2d<half1> ao_buffer_in,
												buffer<const frame_uniforms_t> params) {
	const auto [ao, output_pos] = ao_denoise<4u>(normals_buffer, depth_buffer, ao_buffer_in, params->ao_screen_dim);
	if ((output_pos < params->ao_screen_dim).all()) {
		auto color = scene_buffer.read(output_pos);
#if !WARP_DEBUG_AO
		color.xyz *= half(ao);
#else
		color.xyz = half(ao);
#endif
		scene_buffer.write(output_pos, color);
	}
}

//! ref: https://johanneskopf.de/publications/jbu/paper/FinalPaper_0185.pdf
//! NOTE: this has a lot more tweaking to take care of foliage and depth mismatch/coverage,
//!       and additionally drops the gaussian filter as it makes no difference in practice (at least for 3x3)
kernel_2d(16u, 16u) void upsample_and_blit_ao(image_2d<half4> scene_buffer,
											  const_image_2d<half1> ao_buffer_ds,
											  const_image_2d_depth<float> depth_buffer,
											  const_image_2d_depth<float> depth_buffer_ds,
											  const_image_2d<half2> normals_buffer,
											  const_image_2d<half2> normals_buffer_ds,
											  buffer<const frame_uniforms_t> params) {
	const auto out_coord = global_id.xy;
	const auto in_center_coord = (global_id.xy / 2u).cast<int>();
	const auto depth_center = linearize_depth(depth_buffer.read(out_coord));
	const auto normal_center = decode_normal(normals_buffer.read(out_coord));
	
	static constexpr const float sigma_depth_base { 0.2f * WARP_FAR_PLANE };
	static constexpr const float inv_far_plane_sqr { (1.0f / WARP_FAR_PLANE) * (1.0f / WARP_FAR_PLANE) };
	const auto sigma_depth_factor = math::exp(-sigma_depth_base * inv_far_plane_sqr * depth_center * depth_center);
	
	float ao = 0.0f;
	float ao_weights_sum = 0.0f;
	bool is_all_in_front = true;
#pragma unroll
	for (int y = -1; y <= 1; ++y) {
#pragma unroll
		for (int x = -1; x <= 1; ++x) {
			const auto coord = (in_center_coord + int2 { x, y }).clamped(0, params->ao_screen_dim - 1);
			const auto depth_ds = linearize_depth(depth_buffer_ds.read(coord));
			const auto ao_ds = float(ao_buffer_ds.read(coord));
			const auto normal_ds = decode_normal(normals_buffer_ds.read(coord));
			const auto depth_diff = depth_ds - depth_center;
			const auto depth_diff_sqr = depth_diff * depth_diff;
			const auto weight_normal = float(math::max(normal_center.dot(normal_ds), 0.0_h));
			const auto weight_depth = math::min(math::exp(-depth_diff_sqr * sigma_depth_factor), 1.0f);
			
			if (depth_ds >= depth_center) {
				is_all_in_front = false;
			}
			
			// use a very sharp falloff as we may have very small differences in depth/normals for "wrong" samples
			const auto full_weight = math::min(math::pow(weight_depth * weight_normal, 64.0f), 1.0f);
			ao += full_weight * ao_ds;
			ao_weights_sum += full_weight;
		}
	}
	// if all low-res depth samples are in front of the high-res depth value and the weight sum is very low,
	// reject the AO of the "in front" samples and just assume the high-res sample is fully covered -> works well for 1px foliage
	ao = (ao_weights_sum > 0.0f && !(is_all_in_front && ao_weights_sum < 0.01f) ?
		  ao / ao_weights_sum : 0.0f);
	
	auto color = scene_buffer.read(out_coord);
#if !WARP_DEBUG_AO
	color.xyz *= half(ao);
#else
	color.xyz = half(ao);
#endif
	scene_buffer.write(out_coord, color);
}

kernel_2d(16u, 16u) void downsample_framebuffers(const_image_2d<half2> normals_buffer,
												 const_image_2d_depth<float> depth_buffer,
												 image_2d<half2, true> normals_buffer_ds,
												 image_2d<float, true> depth_buffer_ds) {
	const auto out_coord = global_id.xy;
	const auto in_coord_offset = out_coord * 2u;
	
	// rather than averaging, we will use the values of the pixel with the smallest (linearized) depth
	// NOTE: depth linearization is independent from reverse or normal Z rendering -> lower lin depth is always closer to the camera
	half2 normal_out;
	float depth_out = 0.0f;
	float closest_depth = std::numeric_limits<float>::max();
#pragma unroll
	for (uint32_t y = 0; y < 2; ++y) {
#pragma unroll
		for (uint32_t x = 0; x < 2; ++x) {
			const auto in_coord = in_coord_offset + uint2 { x, y };
			const auto raw_depth = depth_buffer.read(in_coord);
			const auto depth = linearize_depth(raw_depth);
			if (depth < closest_depth || (x == 0 && y == 0) /* skip first check */) {
				normal_out = normals_buffer.read(in_coord);
				depth_out = raw_depth;
				closest_depth = depth;
			}
		}
	}
	
	normals_buffer_ds.write(out_coord, normal_out);
	depth_buffer_ds.write(out_coord, depth_out);
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

static void skybox_vs(skybox_base_in_out& out, const matrix4f& sky_imvpm) {
	static constexpr const float skybox_depth { WARP_REVERSE_Z ? 0.0f : 1.0f };
	switch (vertex_id) {
		case 0: out.position = { 3.0f, -1.0f, skybox_depth, 1.0f }; break;
		case 1: out.position = { -3.0f, -1.0f, skybox_depth, 1.0f }; break;
		case 2: out.position = { 0.0f, 2.0f, skybox_depth, 1.0f }; break;
		default: floor_unreachable();
	}
	out.cube_tex_coord = (out.position * sky_imvpm).xyz;
}

vertex auto skybox_scatter_vs(buffer<const frame_uniforms_t> uniforms) {
	skybox_scatter_in_out out;
	skybox_vs(out, uniforms->sky_imvpm);
	
	out.cur_pos = out.position * uniforms->sky_imvpm;
	out.prev_pos = out.position * uniforms->scatter.sky_prev_imvpm;
	
	return out;
}

// also used for forward-only
vertex auto skybox_gather_vs(buffer<const frame_uniforms_t> uniforms) {
	skybox_gather_in_out out;
	skybox_vs(out, uniforms->sky_imvpm);
	
	const auto proj_vertex = out.position * uniforms->sky_imvpm;
	out.motion_prev = proj_vertex * uniforms->gather.sky_prev_mvpm;
	out.motion_now = out.position;
	out.motion_next = proj_vertex * uniforms->gather.sky_next_mvpm;
	
	return out;
}

static float4 skybox_fs(const skybox_base_in_out& in,
						const_image_cube<float> skybox_tex) {
	return skybox_tex.read_linear(in.cube_tex_coord);
}

static constexpr const float2 skybox_dummy_enc_normal { 1.0f, 0.0f };

fragment auto skybox_scatter_fs(const skybox_scatter_in_out in [[stage_input]],
								arg_buffer<sky_fs_data_t> sky_fs_data) {
	return scene_scatter_fragment_out {
		skybox_fs(in, sky_fs_data.skybox_tex),
		skybox_dummy_enc_normal,
		encode_3d_motion(in.prev_pos.xyz - in.cur_pos.xyz)
	};
}

fragment auto skybox_gather_fs(const skybox_gather_in_out in [[stage_input]],
							   arg_buffer<sky_fs_data_t> sky_fs_data) {
	return scene_gather_fragment_out {
		skybox_fs(in, sky_fs_data.skybox_tex),
		skybox_dummy_enc_normal,
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		{
			(in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w),
			(in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w)
		}
	};
}

fragment auto skybox_gather_fwd_fs(const skybox_gather_in_out in [[stage_input]],
								   arg_buffer<sky_fs_data_t> sky_fs_data) {
	return scene_scatter_fragment_out /* reuse */ {
		skybox_fs(in, sky_fs_data.skybox_tex),
		skybox_dummy_enc_normal,
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}

//////////////////////////////////////////
// manual blitting

struct blit_in_out {
	float4 position [[position]];
	float2 norm_screen_pos;
};

vertex blit_in_out blit_vs() {
	switch(vertex_id) {
		case 0: return { { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } };
		case 1: return { { 1.0f, -3.0f, 0.0f, 1.0f }, { 1.0f, -3.0f } };
		case 2: return { { -3.0f, 1.0f, 0.0f, 1.0f }, { -3.0f, 1.0f } };
		default: floor_unreachable();
	}
}

fragment float4 blit_fs(const blit_in_out in [[stage_input]],
						const_image_2d<float> img) {
	return img.read(in.position.xy.cast<uint32_t>());
}
fragment float4 blit_swizzle_fs(const blit_in_out in [[stage_input]],
								const_image_2d<float> img) {
	return img.read(in.position.xy.cast<uint32_t>()).swizzle<2, 1, 0, 3>();
}

fragment float4 blit_debug_depth_fs(const blit_in_out in [[stage_input]],
									const_image_2d_depth<float> img) {
	auto depth = img.read((img.dim().xy.cast<float>() * (in.norm_screen_pos * 0.5f + 0.5f)).cast<uint32_t>());
	
	// linearize depth
	if constexpr (!WARP_REVERSE_Z) {
		constexpr const float2 near_far_projection {
			-near_far_plane.y / (near_far_plane.y - near_far_plane.x),
			(-near_far_plane.y * near_far_plane.x) / (near_far_plane.y - near_far_plane.x),
		};
		// special case: clear/full depth, assume this comes from a normalized sky box
		depth = (depth == 1.0f ? 1.0f : near_far_projection.y / (depth + near_far_projection.x));
	} else {
		constexpr const float2 near_far_projection {
			-near_far_plane.y / (near_far_plane.x - near_far_plane.y) - 1.0f,
			(-near_far_plane.x * near_far_plane.y) / (near_far_plane.x - near_far_plane.y),
		};
		// special case: clear/full depth, assume this comes from a normalized sky box
		depth = (depth == 0.0f ? 0.0f : near_far_projection.y / (depth + near_far_projection.x));
	}
	
	depth = math::fmod(depth, 1.0f);
	return { depth, depth, depth, 1.0f };
}

fragment float4 blit_debug_motion_2d_fs(const blit_in_out in [[stage_input]],
										const_image_2d<uint1> img) {
	auto motion = unpack_snorm_2x16(img.read(in.position.xy.cast<uint32_t>()));
	return { math::abs(motion.x), math::abs(motion.y), 0.0f, 1.0f };
}

fragment float4 blit_debug_motion_3d_fs(const blit_in_out in [[stage_input]],
										const_image_2d<uint1> img) {
	const auto encoded_motion = img.read(in.position.xy.cast<uint32_t>());
	
	static constexpr const float3 signs_lookup[] {
		{ 1.0f, 1.0f, 1.0f },
		{ 1.0f, 1.0f, -1.0f },
		{ 1.0f, -1.0f, 1.0f },
		{ 1.0f, -1.0f, -1.0f },
		{ -1.0f, 1.0f, 1.0f },
		{ -1.0f, 1.0f, -1.0f },
		{ -1.0f, -1.0f, 1.0f },
		{ -1.0f, -1.0f, -1.0f },
	};
	const float3 signs = signs_lookup[encoded_motion >> 29u];
	const uint3 shifted_motion {
		(encoded_motion >> 19u) & 0x3FFu,
		(encoded_motion >> 10u) & 0x1FFu,
		encoded_motion & 0x3FFu
	};
	constexpr const float3 adjust {
		const_math::log2(64.0f + 1.0f) / 1024.0f,
		const_math::log2(64.0f + 1.0f) / 512.0f,
		const_math::log2(64.0f + 1.0f) / 1024.0f
	};
	auto motion = signs * ((float3(shifted_motion) * adjust).exp2() - 1.0f);
	
	constexpr const float range = 64.0f; // [-range, range]
	motion /= range;
	motion.abs();
	
	return { motion.x, motion.y, motion.z, 1.0f };
}

fragment float4 blit_debug_motion_depth_fs(const blit_in_out in [[stage_input]],
										   const_image_2d<float2> img) {
	auto motion_depth = img.read(in.position.xy.cast<uint32_t>());
	
	motion_depth = motion_depth + near_far_plane.x - (motion_depth * (near_far_plane.x / near_far_plane.y));
	
	motion_depth = (motion_depth % 0.0005f) * 2000.0f;
	return { motion_depth.x, motion_depth.y, 0.0f, 1.0f };
}

#endif
