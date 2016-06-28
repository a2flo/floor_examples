/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

// only metal for now
#if defined(FLOOR_COMPUTE_METAL)

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
	half2 motion_depth;
};

// props to:
// * http://outerra.blogspot.sk/2012/11/maximizing-depth-buffer-range-and.html
// * http://outerra.blogspot.sk/2009/08/logarithmic-z-buffer.html
static float4 log_depth(float4 transformed_position) {
	// actual computation:
	constexpr const float C = WARP_SHADOW_NEAR_PLANE; // 1.0
	constexpr const float far = WARP_SHADOW_FAR_PLANE; // 260.0
	constexpr const float ce_term = 2.0f / math::log(far * C + 1.0f);
	transformed_position.z = log(transformed_position.w * C + 1.0f) * ce_term - 1.0f;
	transformed_position.z *= transformed_position.w;
	return transformed_position;
}

static void scene_vs(scene_base_in_out& out,
					 buffer<const float3> in_position,
					 buffer<const float2> in_tex_coord,
					 buffer<const float3> in_normal,
					 buffer<const float3> in_binormal,
					 buffer<const float3> in_tangent,
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
}

vertex auto scene_scatter_vs(buffer<const float3> in_position,
							 buffer<const float2> in_tex_coord,
							 buffer<const float3> in_normal,
							 buffer<const float3> in_binormal,
							 buffer<const float3> in_tangent,
							 param<scene_scatter_uniforms_t> uniforms) {
	scene_scatter_in_out out;
	scene_vs(out, in_position, in_tex_coord, in_normal, in_binormal, in_tangent, uniforms);
	
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
							param<scene_gather_uniforms_t> uniforms) {
	scene_gather_in_out out;
	scene_vs(out, in_position, in_tex_coord, in_normal, in_binormal, in_tangent, uniforms);
	
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
	const auto cmotion = short2((motion * 32767.0f).clamp(-32767.0f, 32767.0f)); // +/- 2^15 - 1, fit into 16 bits
	return *(uint32_t*)&cmotion;
}

static float4 scene_fs(const scene_base_in_out& in,
					   const_image_2d<float> diff_tex,
					   const_image_2d<float> spec_tex,
					   const_image_2d<float> norm_tex,
					   const_image_2d<float1> mask_tex,
					   const_image_2d_depth<float> shadow_tex) {
	const auto tex_coord = in.tex_coord;
	
	if(mask_tex.read_lod_linear(tex_coord, 0) < 0.5f) {
		discard_fragment();
	}
	
	const auto tex_grad = dfdx_dfdy_gradient(tex_coord);
	auto diff = diff_tex.read_gradient_linear(tex_coord, tex_grad);
	
	//
	constexpr float fixed_bias = 0.001f;
	constexpr float ambient = 0.2f;
	constexpr float attenuation = 0.9f;
	float lighting = 0.0f;
	float light_vis = 1.0f;
	
	//
	const auto norm_view_dir = in.view_dir.normalized();
	const auto norm_light_dir = in.light_dir.normalized();
	const auto norm_half_dir = (norm_view_dir + norm_light_dir).normalized();
	const auto normal = norm_tex.read_gradient_linear(tex_coord, tex_grad).xyz * 2.0f - 1.0f;
	
	const auto lambert = normal.dot(norm_light_dir);
	if(lambert > 0.0f) {
		// shadow hackery
		const auto bias_lambert = min(lambert, 0.99995f); // clamp to "(0, 1)", already > 0 here
		const auto slope = sqrt(1.0f - bias_lambert * bias_lambert) / bias_lambert; // = tan(acos(lambert))
		const auto shadow_bias = math::clamp(fixed_bias * slope, 0.0f, fixed_bias * 2.0f);
		
		float3 shadow_coord = in.shadow_coord.xyz / in.shadow_coord.w;
		shadow_coord.y = 1.0f - shadow_coord.y; // why metal, why?
#if 1
		if(in.shadow_coord.w > 0.0f) {
			light_vis = shadow_tex.compare_linear<COMPARE_FUNCTION::LESS_OR_EQUAL>(shadow_coord.xy, shadow_coord.z - shadow_bias);
		}
#elif 1 // TODO/NOTE: this horribly breaks stuff, don't use it yet
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
		
#pragma unroll
		for(const auto& poisson_coord : poisson_disk) {
			light_vis -= 0.05f * (1.0f - shadow_tex.compare_linear<COMPARE_FUNCTION::LESS_OR_EQUAL>(shadow_coord.xy + poisson_coord / 2048.0f,
																									shadow_coord.z - shadow_bias));
		}
#endif
		
		// diffuse
		lighting += lambert;
		
		// specular
		const auto spec = spec_tex.read_gradient_linear(tex_coord, tex_grad).x;
		const auto specular = pow(max(norm_half_dir.dot(normal), 0.0f), spec * 10.0f);
		lighting += specular;
		
		// mul with shadow and light attenuation
		lighting *= light_vis * attenuation;
	}
	lighting = max(lighting, ambient);
	
	return { diff.xyz * lighting, 1.0f };
}

fragment auto scene_scatter_fs(const scene_scatter_in_out in [[stage_input]],
							   const_image_2d<float> diff_tex,
							   const_image_2d<float> spec_tex,
							   const_image_2d<float> norm_tex,
							   const_image_2d<float1> mask_tex,
							   const_image_2d_depth<float> shadow_tex) {
	return scene_scatter_fragment_out {
		scene_fs(in, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
		encode_3d_motion(in.motion)
	};
}

fragment auto scene_gather_fs(const scene_gather_in_out in [[stage_input]],
							  const_image_2d<float> diff_tex,
							  const_image_2d<float> spec_tex,
							  const_image_2d<float> norm_tex,
							  const_image_2d<float1> mask_tex,
							  const_image_2d_depth<float> shadow_tex) {
	return scene_gather_fragment_out {
		scene_fs(in, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		half2 {
			(half)((in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w)),
			(half)((in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w))
		}
	};
}

fragment auto scene_gather_fwd_fs(const scene_gather_in_out in [[stage_input]],
								  const_image_2d<float> diff_tex,
								  const_image_2d<float> spec_tex,
								  const_image_2d<float> norm_tex,
								  const_image_2d<float1> mask_tex,
								  const_image_2d_depth<float> shadow_tex) {
	return scene_scatter_fragment_out /* reuse */ {
		scene_fs(in, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
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
	switch(vertex_id) {
		case 0: out.position = { 0.0f, 2.0f, 1.0f, 1.0f }; break;
		case 1: out.position = { -3.0f, -1.0f, 1.0f, 1.0f }; break;
		case 2: out.position = { 3.0f, -1.0f, 1.0f, 1.0f }; break;
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
		half2 {
			(half)((in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w)),
			(half)((in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w))
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
	return img.read(uint2 { uint32_t(in.position.x), uint32_t(in.position.y) });
}
fragment float4 blit_swizzle_fs(const blit_in_out in [[stage_input]],
								const_image_2d<float> img) {
	return img.read(uint2 { uint32_t(in.position.x), uint32_t(in.position.y) }).swizzle<2, 1, 0, 3>();
}

#endif
