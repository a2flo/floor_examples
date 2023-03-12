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

#include "warp_shaders.hpp"

// graphics backends only
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_GRAPHICS_HOST)

#if !defined(WARP_SHADOW_FAR_PLANE)
#define WARP_SHADOW_FAR_PLANE 260.0f
#endif

using namespace warp_shaders;

//////////////////////////////////////////
// scene

struct scene_base_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
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

#if defined(WARP_USE_ARGUMENT_BUFFER) && FLOOR_COMPUTE_INFO_TESSELLATION_SUPPORT
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
		const auto factor_scaler = max(tri_area, 1.5f); // ensure large triangles get larger factors
		const auto min_distance = sqrt(distances_sq.min_element());
		factor = (half)math::clamp(max_tess_factor * factor_scaler * exp(-0.06f * min_distance), 1.0f, max_tess_factor);
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
	
	out.material_data = in_material_data;
}

#if !defined(WARP_USE_ARGUMENT_BUFFER) // -> non-arg-buffer/non-indirect
vertex auto scene_scatter_vs(buffer<const float3> in_position,
							 buffer<const float2> in_tex_coord,
							 buffer<const float3> in_normal,
							 buffer<const float3> in_binormal,
							 buffer<const float3> in_tangent,
							 buffer<const uint32_t> in_materials_data,
							 param<scene_scatter_uniforms_t> uniforms) {
	scene_scatter_in_out out;
	scene_vs(out, in_position[vertex_id], in_tex_coord[vertex_id], in_normal[vertex_id], in_binormal[vertex_id],
			 in_tangent[vertex_id], in_materials_data[vertex_id], uniforms.mvpm, uniforms.light_bias_mvpm, uniforms.cam_pos, uniforms.light_pos);
	
	float4 pos { in_position[vertex_id], 1.0f };
	float4 prev_pos = pos * uniforms.prev_mvm;
	float4 cur_pos = pos * uniforms.mvm;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}
#elif defined(WARP_USE_ARGUMENT_BUFFER) && !defined(WARP_USE_INDIRECT_COMMANDS) // -> arg-buffer/non-indirect
vertex auto scene_scatter_vs(arg_buffer<model_data_t> model_data,
							 param<scene_scatter_uniforms_t> uniforms) {
	scene_scatter_in_out out;
	const auto vtx = model_data.pnbt[model_data_t::POSITION][vertex_id];
	scene_vs(out, vtx,
			 model_data.tc[vertex_id],
			 model_data.pnbt[model_data_t::NORMAL][vertex_id],
			 model_data.pnbt[model_data_t::BINORMAL][vertex_id],
			 model_data.pnbt[model_data_t::TANGENT][vertex_id],
			 model_data.materials_data[vertex_id],
			 uniforms.mvpm, uniforms.light_bias_mvpm, uniforms.cam_pos, uniforms.light_pos);
	
	float4 pos { vtx, 1.0f };
	float4 prev_pos = pos * uniforms.prev_mvm;
	float4 cur_pos = pos * uniforms.mvm;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}
#else // -> arg-buffer/indirect
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
#endif

#if FLOOR_COMPUTE_INFO_TESSELLATION_SUPPORT
#if !defined(WARP_USE_INDIRECT_COMMANDS) // -> non-indirect
[[patch(triangle, 3)]]
tessellation_evaluation auto scene_scatter_tes(patch_in_t in [[stage_input]],
											   array<const_image_2d<float1>, material_count> disp_tex,
											   param<uint32_t> disp_mode,
											   param<scene_scatter_uniforms_t> uniforms) {
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
	
	if (should_displace && disp_mode == 2) {
		static constexpr constant const float max_displacement { 0.25f };
		auto displacement = disp_tex[material].read_linear_repeat(tex_coord) * 2.0f - 1.0f; // [-1, 1]
		vertex_pos += max_displacement * displacement * normal.normalized();
	}
	
	scene_scatter_in_out out;
	scene_vs(out, vertex_pos, tex_coord, normal, binormal, tangent, material_data,
			 uniforms.mvpm, uniforms.light_bias_mvpm, uniforms.cam_pos, uniforms.light_pos);
	
	float4 pos { vertex_pos, 1.0f };
	float4 prev_pos = pos * uniforms.prev_mvm;
	float4 cur_pos = pos * uniforms.mvm;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}
#else // -> indirect
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
#endif

// also used for forward-only
#if !defined(WARP_USE_ARGUMENT_BUFFER) // -> non-arg-buffer/non-indirect
vertex auto scene_gather_vs(buffer<const float3> in_position,
							buffer<const float2> in_tex_coord,
							buffer<const float3> in_normal,
							buffer<const float3> in_binormal,
							buffer<const float3> in_tangent,
							buffer<const uint32_t> in_materials_data,
							param<scene_gather_uniforms_t> uniforms) {
	scene_gather_in_out out;
	scene_vs(out, in_position[vertex_id], in_tex_coord[vertex_id], in_normal[vertex_id], in_binormal[vertex_id],
			 in_tangent[vertex_id], in_materials_data[vertex_id], uniforms.mvpm, uniforms.light_bias_mvpm, uniforms.cam_pos, uniforms.light_pos);
	
	float4 pos { in_position[vertex_id], 1.0f };
	out.motion_prev = pos * uniforms.prev_mvpm;
	out.motion_now = pos * uniforms.mvpm;
	out.motion_next = pos * uniforms.next_mvpm;
	
	return out;
}
#elif defined(WARP_USE_ARGUMENT_BUFFER) && !defined(WARP_USE_INDIRECT_COMMANDS) // -> arg-buffer/non-indirect
vertex auto scene_gather_vs(arg_buffer<model_data_t> model_data,
							param<scene_gather_uniforms_t> uniforms) {
	scene_gather_in_out out;
	const auto vtx = model_data.pnbt[model_data_t::POSITION][vertex_id];
	scene_vs(out, vtx,
			 model_data.tc[vertex_id],
			 model_data.pnbt[model_data_t::NORMAL][vertex_id],
			 model_data.pnbt[model_data_t::BINORMAL][vertex_id],
			 model_data.pnbt[model_data_t::TANGENT][vertex_id],
			 model_data.materials_data[vertex_id],
			 uniforms.mvpm, uniforms.light_bias_mvpm, uniforms.cam_pos, uniforms.light_pos);
	
	float4 pos { vtx, 1.0f };
	out.motion_prev = pos * uniforms.prev_mvpm;
	out.motion_now = pos * uniforms.mvpm;
	out.motion_next = pos * uniforms.next_mvpm;
	
	return out;
}
#else // -> arg-buffer/indirect
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
#endif

#if FLOOR_COMPUTE_INFO_TESSELLATION_SUPPORT
#if !defined(WARP_USE_INDIRECT_COMMANDS) // -> non-indirect
[[patch(triangle, 3)]]
tessellation_evaluation auto scene_gather_tes(patch_in_t in [[stage_input]],
											  array<const_image_2d<float1>, material_count> disp_tex,
											  param<uint32_t> disp_mode,
											  param<scene_gather_uniforms_t> uniforms) {
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
	
	if (should_displace && disp_mode == 2) {
		static constexpr constant const float max_displacement { 0.25f };
		auto displacement = disp_tex[material].read_linear_repeat(tex_coord) * 2.0f - 1.0f; // [-1, 1]
		vertex_pos += max_displacement * displacement * normal.normalized();
	}
	
	scene_gather_in_out out;
	scene_vs(out, vertex_pos, tex_coord, normal, binormal, tangent, material_data,
			 uniforms.mvpm, uniforms.light_bias_mvpm, uniforms.cam_pos, uniforms.light_pos);
	
	float4 pos { vertex_pos, 1.0f };
	out.motion_prev = pos * uniforms.prev_mvpm;
	out.motion_now = pos * uniforms.mvpm;
	out.motion_next = pos * uniforms.next_mvpm;
	
	return out;
}
#else // -> indirect
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

static float4 scene_fs(const scene_base_in_out& in,
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

#if !defined(WARP_USE_ARGUMENT_BUFFER) // -> non-arg-buffer/non-indirect
// non-argument-buffer variant: specify all material textures as separate parameters
// NOTE: since this requires 126 textures (texture locations), this only works on macOS Tier1+ (128 limit) and or iOS Tier2/A13+

fragment auto scene_scatter_fs(const scene_scatter_in_out in [[stage_input]],
							   param<uint32_t> disp_mode,
							   array<const_image_2d<float>, material_count> diff_tex,
							   array<const_image_2d<float>, material_count> spec_tex,
							   array<const_image_2d<float>, material_count> norm_tex,
							   array<const_image_2d<float1>, material_count> mask_tex,
							   array<const_image_2d<float1>, material_count> disp_tex,
							   const_image_2d_depth<float> shadow_tex) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_scatter_fragment_out {
		scene_fs(in, should_displace ? disp_mode : 0u, diff_tex[mat_idx], spec_tex[mat_idx], norm_tex[mat_idx],
				 mask_tex[mat_idx], disp_tex[mat_idx], shadow_tex),
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
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_gather_fragment_out {
		scene_fs(in, should_displace ? disp_mode : 0u, diff_tex[mat_idx], spec_tex[mat_idx], norm_tex[mat_idx],
				 mask_tex[mat_idx], disp_tex[mat_idx], shadow_tex),
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
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_scatter_fragment_out /* reuse */ {
		scene_fs(in, should_displace ? disp_mode : 0u, diff_tex[mat_idx], spec_tex[mat_idx], norm_tex[mat_idx],
				 mask_tex[mat_idx], disp_tex[mat_idx], shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}

#elif defined(WARP_USE_ARGUMENT_BUFFER) && !defined(WARP_USE_INDIRECT_COMMANDS) // -> arg-buffer/non-indirect
// argument-buffer variant: put all material textures into a single argument buffer

fragment auto scene_scatter_fs(const scene_scatter_in_out in [[stage_input]],
							   param<uint32_t> disp_mode,
							   arg_buffer<materials_t> materials,
							   const_image_2d_depth<float> shadow_tex) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_scatter_fragment_out {
		scene_fs(in, should_displace ? disp_mode : 0u, materials.diff_tex[mat_idx], materials.spec_tex[mat_idx],
				 materials.norm_tex[mat_idx], materials.mask_tex[mat_idx], materials.disp_tex[mat_idx], shadow_tex),
		encode_3d_motion(in.motion)
	};
}

fragment auto scene_gather_fs(const scene_gather_in_out in [[stage_input]],
							  param<uint32_t> disp_mode,
							  arg_buffer<materials_t> materials,
							  const_image_2d_depth<float> shadow_tex) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_gather_fragment_out {
		scene_fs(in, should_displace ? disp_mode : 0u, materials.diff_tex[mat_idx], materials.spec_tex[mat_idx],
				 materials.norm_tex[mat_idx], materials.mask_tex[mat_idx], materials.disp_tex[mat_idx], shadow_tex),
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
								  arg_buffer<materials_t> materials,
								  const_image_2d_depth<float> shadow_tex) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_scatter_fragment_out /* reuse */ {
		scene_fs(in, should_displace ? disp_mode : 0u, materials.diff_tex[mat_idx], materials.spec_tex[mat_idx],
				 materials.norm_tex[mat_idx], materials.mask_tex[mat_idx], materials.disp_tex[mat_idx], shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}

#else // -> arg-buffer/indirect
// argument-buffer + indirect variant: put all material textures into a single argument buffer + all parameters are contained within buffers

fragment auto scene_scatter_fs(const scene_scatter_in_out in [[stage_input]],
							   buffer<const frame_uniforms_t> uniforms,
							   arg_buffer<materials_t> materials,
							   arg_buffer<scene_fs_data_t> scene_fs_data) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_scatter_fragment_out {
		scene_fs(in, should_displace ? uniforms->displacement_mode : 0u, materials.diff_tex[mat_idx], materials.spec_tex[mat_idx],
				 materials.norm_tex[mat_idx], materials.mask_tex[mat_idx], materials.disp_tex[mat_idx], scene_fs_data.shadow_tex),
		encode_3d_motion(in.motion)
	};
}

fragment auto scene_gather_fs(const scene_gather_in_out in [[stage_input]],
							  buffer<const frame_uniforms_t> uniforms,
							  arg_buffer<materials_t> materials,
							  arg_buffer<scene_fs_data_t> scene_fs_data) {
	const auto mat_idx = in.material_data & 0xFFFFu;
	const auto should_displace = ((in.material_data & 0xFFFF'0000u) != 0u);
	return scene_gather_fragment_out {
		scene_fs(in, should_displace ? uniforms->displacement_mode : 0u, materials.diff_tex[mat_idx], materials.spec_tex[mat_idx],
				 materials.norm_tex[mat_idx], materials.mask_tex[mat_idx], materials.disp_tex[mat_idx], scene_fs_data.shadow_tex),
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
	return scene_scatter_fragment_out /* reuse */ {
		scene_fs(in, should_displace ? uniforms->displacement_mode : 0u, materials.diff_tex[mat_idx], materials.spec_tex[mat_idx],
				 materials.norm_tex[mat_idx], materials.mask_tex[mat_idx], materials.disp_tex[mat_idx], scene_fs_data.shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}

#endif

//////////////////////////////////////////
// shadow map

struct shadow_in_out {
	float4 position [[position]];
};

#if !defined(WARP_USE_INDIRECT_COMMANDS) // -> non-indirect
vertex shadow_in_out shadow_vs(buffer<const float3> in_position,
							   param<shadow_uniforms_t> uniforms) {
	return {
		log_depth(float4 { in_position[vertex_id], 1.0f } * uniforms.mvpm)
	};
}
#else // -> indirect
vertex shadow_in_out shadow_vs(buffer<const float3> in_position,
							   buffer<const frame_uniforms_t> uniforms) {
	return {
		log_depth(float4 { in_position[vertex_id], 1.0f } * uniforms->light_mvpm)
	};
}
#endif

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
	switch (vertex_id) {
		case 0: out.position = { 3.0f, -1.0f, 1.0f, 1.0f }; break;
		case 1: out.position = { -3.0f, -1.0f, 1.0f, 1.0f }; break;
		case 2: out.position = { 0.0f, 2.0f, 1.0f, 1.0f }; break;
		default: floor_unreachable();
	}
	out.cube_tex_coord = (out.position * sky_imvpm).xyz;
}

#if !defined(WARP_USE_INDIRECT_COMMANDS) // -> non-indirect
vertex auto skybox_scatter_vs(param<skybox_scatter_uniforms_t> uniforms) {
	skybox_scatter_in_out out;
	skybox_vs(out, uniforms.imvpm);
	
	out.cur_pos = out.position * uniforms.imvpm;
	out.prev_pos = out.position * uniforms.prev_imvpm;
	
	return out;
}

// also used for forward-only
vertex auto skybox_gather_vs(param<skybox_gather_uniforms_t> uniforms) {
	skybox_gather_in_out out;
	skybox_vs(out, uniforms.imvpm);
	
	const auto proj_vertex = out.position * uniforms.imvpm;
	out.motion_prev = proj_vertex * uniforms.prev_mvpm;
	out.motion_now = out.position;
	out.motion_next = proj_vertex * uniforms.next_mvpm;
	
	return out;
}
#else // -> indirect
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
#endif

static float4 skybox_fs(const skybox_base_in_out& in,
						const_image_cube<float> skybox_tex) {
	return skybox_tex.read_linear(in.cube_tex_coord);
}

#if !defined(WARP_USE_INDIRECT_COMMANDS) // -> non-indirect
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
#else // -> indirect
fragment auto skybox_scatter_fs(const skybox_scatter_in_out in [[stage_input]],
								arg_buffer<sky_fs_data_t> sky_fs_data) {
	return scene_scatter_fragment_out {
		skybox_fs(in, sky_fs_data.skybox_tex),
		encode_3d_motion(in.prev_pos.xyz - in.cur_pos.xyz)
	};
}

fragment auto skybox_gather_fs(const skybox_gather_in_out in [[stage_input]],
							   arg_buffer<sky_fs_data_t> sky_fs_data) {
	return scene_gather_fragment_out {
		skybox_fs(in, sky_fs_data.skybox_tex),
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
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
	};
}
#endif

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
