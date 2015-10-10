
#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

//////////////////////////////////////////
// scene

// since there is no inheritance in metal, this base class will be awkwardly copypasted to both "inheriting" classes
struct scene_base_in_out {
	float4 position;
	float4 shadow_coord;
	float2 tex_coord;
	float3 view_dir;
	float3 light_dir;
};

struct scene_scatter_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
	float3 view_dir;
	float3 light_dir;
	float3 motion;
};

struct scene_gather_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
	float3 view_dir;
	float3 light_dir;
	float4 motion_prev;
	float4 motion_now;
	float4 motion_next;
};

// same as above ...
struct scene_base_uniforms_t {
	matrix_float4x4 mvpm;
	matrix_float4x4 m0;
	matrix_float4x4 m1;
	matrix_float4x4 light_bias_mvpm;
	packed_float3 cam_pos;
	packed_float3 light_pos;
};

struct scene_scatter_uniforms_t {
	matrix_float4x4 mvpm;
	matrix_float4x4 mvm;
	matrix_float4x4 prev_mvm;
	matrix_float4x4 light_bias_mvpm;
	packed_float3 cam_pos;
	packed_float3 light_pos;
};

struct scene_gather_uniforms_t {
	matrix_float4x4 mvpm; // @t
	matrix_float4x4 next_mvpm; // @t+1
	matrix_float4x4 prev_mvpm; // @t-1
	matrix_float4x4 light_bias_mvpm;
	packed_float3 cam_pos;
	packed_float3 light_pos;
};

struct scene_scatter_fragment_out {
	float4 color [[color(0)]];
	uint32_t motion [[color(1)]];
};

struct scene_gather_fragment_out {
	float4 color [[color(0)]];
	uint32_t motion_forward [[color(1)]];
	uint32_t motion_backward [[color(2)]];
	float motion_depth_forward [[color(3)]];
	float motion_depth_backward [[color(4)]];
};

// props to:
// * http://outerra.blogspot.sk/2012/11/maximizing-depth-buffer-range-and.html
// * http://outerra.blogspot.sk/2009/08/logarithmic-z-buffer.html
static float4 log_depth(float4 transformed_position) {
	// actual computation:
	// constexpr const float C = 1.0f;
	// constexpr const float far = 260.0f;
	// transformed_position.z = 2.0f * log(transformed_position.w * C + 1.0f) / log(far * C + 1.0f) - 1.0f;
	// but since metal has no compile-time math, condense it manually ->
	transformed_position.z = 2.0f * log(transformed_position.w + 1.0f) * 0.179710006757103f - 1.0f;
	transformed_position.z *= transformed_position.w;
	return transformed_position;
}

static void scene_vs(thread scene_base_in_out* out,
					 device const packed_float3* in_position,
					 device const packed_float2* in_tex_coord,
					 device const packed_float3* in_normal,
					 device const packed_float3* in_binormal,
					 device const packed_float3* in_tangent,
					 constant scene_base_uniforms_t* uniforms,
					 const unsigned int vid) {
	out->tex_coord = in_tex_coord[vid];
	
	float4 pos(in_position[vid], 1.0f);
	out->shadow_coord = log_depth(uniforms->light_bias_mvpm * pos);
	out->position = uniforms->mvpm * pos;
	
	float3 vview = uniforms->cam_pos - in_position[vid];
	out->view_dir.x = dot(vview, in_tangent[vid]);
	out->view_dir.y = dot(vview, in_binormal[vid]);
	out->view_dir.z = dot(vview, in_normal[vid]);
	
	float3 vlight = uniforms->light_pos - in_position[vid];
	out->light_dir.x = dot(vlight, in_tangent[vid]);
	out->light_dir.y = dot(vlight, in_binormal[vid]);
	out->light_dir.z = dot(vlight, in_normal[vid]);
}

vertex scene_scatter_in_out scene_scatter_vs(device const packed_float3* in_position [[buffer(0)]],
											 device const packed_float2* in_tex_coord [[buffer(1)]],
											 device const packed_float3* in_normal [[buffer(2)]],
											 device const packed_float3* in_binormal [[buffer(3)]],
											 device const packed_float3* in_tangent [[buffer(4)]],
											 constant scene_scatter_uniforms_t& uniforms [[buffer(5)]],
											 const unsigned int vid [[vertex_id]]) {
	scene_scatter_in_out out;
	
	scene_vs((thread scene_base_in_out*)&out, in_position, in_tex_coord, in_normal, in_binormal, in_tangent,
			 (constant scene_base_uniforms_t*)&uniforms, vid);
	
	float4 pos(in_position[vid], 1.0f);
	float4 prev_pos = uniforms.prev_mvm * pos;
	float4 cur_pos = uniforms.mvm * pos;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}

vertex scene_gather_in_out scene_gather_vs(device const packed_float3* in_position [[buffer(0)]],
										   device const packed_float2* in_tex_coord [[buffer(1)]],
										   device const packed_float3* in_normal [[buffer(2)]],
										   device const packed_float3* in_binormal [[buffer(3)]],
										   device const packed_float3* in_tangent [[buffer(4)]],
										   constant scene_gather_uniforms_t& uniforms [[buffer(5)]],
										   const unsigned int vid [[vertex_id]]) {
	scene_gather_in_out out;
	
	scene_vs((thread scene_base_in_out*)&out, in_position, in_tex_coord, in_normal, in_binormal, in_tangent,
			 (constant scene_base_uniforms_t*)&uniforms, vid);
	
	float4 pos(in_position[vid], 1.0f);
	out.motion_prev = uniforms.prev_mvpm * pos;
	out.motion_now = uniforms.mvpm * pos;
	out.motion_next = uniforms.next_mvpm * pos;
	
	return out;
}

static uint32_t encode_3d_motion(thread const float3& motion) {
	constexpr const float range = 64.0; // [-range, range]
	const float3 signs = sign(motion);
	float3 cmotion = clamp(abs(motion), 0.0, range);
	// use log2 scaling
	cmotion = precise::log2(cmotion + 1.0);
	// encode x and z with 10-bit, y with 9-bit
	//cmotion *= 1.0 / precise::log2(range + 1.0);
	cmotion *= 0.16604764621594f; // no constexpr math :(
	cmotion.xz *= 1024.0f; // 2^10
	cmotion.y *= 512.0f; // 2^9
	return ((signs.x < 0.0f ? 0x80000000u : 0u) |
			(signs.y < 0.0f ? 0x40000000u : 0u) |
			(signs.z < 0.0f ? 0x20000000u : 0u) |
			(clamp(uint32_t(cmotion.x), 0u, 1023u) << 19u) |
			(clamp(uint32_t(cmotion.y), 0u, 511u) << 10u) |
			(clamp(uint32_t(cmotion.z), 0u, 1023u)));
}

static uint32_t encode_2d_motion(thread const float2& motion) {
	// could use this if it wasn't broken on intel gpus ...
	//return pack_float_to_snorm2x16(motion);
	float2 cmotion = clamp(motion * 32767.0f, -32767.0f, 32767.0f); // +/- 2^15 - 1, fit into 16 bits
	//return as_type<uint>(short2(cmotion)); // this is also broken
	uint2 umotion = as_type<uint2>(int2(cmotion)) & 0xFFFFu;
	return (umotion.x | (umotion.y << 16u));
}

static float4 scene_fs(const thread scene_base_in_out* in,
					   const sampler smplr,
					   texture2d<float> diff_tex,
					   texture2d<float> spec_tex,
					   texture2d<float> norm_tex,
					   texture2d<float> mask_tex,
					   depth2d<float> shadow_tex) {
	constexpr sampler shadow_smplr {
		coord::normalized,
		filter::linear,
		mag_filter::linear,
		min_filter::linear,
		address::clamp_to_edge,
		compare_func::less_equal,
	};
	
	float mask = mask_tex.sample(smplr, in->tex_coord, level { 0.0f }).x;
	if(mask < 0.5f) discard_fragment();
	
	const gradient2d tex_grad { dfdx(in->tex_coord), dfdy(in->tex_coord) };
	auto diff = diff_tex.sample(smplr, in->tex_coord, tex_grad);
	
	//
	constexpr float fixed_bias = 0.001f;
	constexpr float ambient = 0.2f;
	constexpr float attenuation = 0.9f;
	float lighting = 0.0f;
	float light_vis = 1.0f;
	
	//
	float3 norm_view_dir = normalize(in->view_dir);
	float3 norm_light_dir = normalize(in->light_dir);
	float3 norm_half_dir = normalize(norm_view_dir + norm_light_dir);
	float3 normal = norm_tex.sample(smplr, in->tex_coord, tex_grad).xyz * 2.0f - 1.0f;
	
	float lambert = dot(normal, norm_light_dir);
	if(lambert > 0.0f) {
		// shadow hackery
		float bias_lambert = min(lambert, 0.99995f); // clamp to "(0, 1)", already > 0 here
		float slope = sqrt(1.0f - bias_lambert * bias_lambert) / bias_lambert; // = tan(acos(lambert))
		float shadow_bias = clamp(fixed_bias * slope, 0.0f, fixed_bias * 2.0f);
		
		float3 shadow_coord = in->shadow_coord.xyz / in->shadow_coord.w;
		shadow_coord.y = 1.0f - shadow_coord.y; // why metal, why?
#if 0
		if(in.shadow_coord.w > 0.0f) {
			light_vis = shadow_tex.sample_compare(shadow_smplr, shadow_coord.xy, shadow_coord.z - shadow_bias);
		}
#elif 1
		// props to https://code.google.com/p/opengl-tutorial-org/source/browse/#hg%2Ftutorial16_shadowmaps for this
		constexpr float2 poisson_disk[16] {
			{ -0.94201624, -0.39906216 },
			{ 0.94558609, -0.76890725 },
			{ -0.094184101, -0.92938870 },
			{ 0.34495938, 0.29387760 },
			{ -0.91588581, 0.45771432 },
			{ -0.81544232, -0.87912464 },
			{ -0.38277543, 0.27676845 },
			{ 0.97484398, 0.75648379 },
			{ 0.44323325, -0.97511554 },
			{ 0.53742981, -0.47373420 },
			{ -0.26496911, -0.41893023 },
			{ 0.79197514, 0.19090188 },
			{ -0.24188840, 0.99706507 },
			{ -0.81409955, 0.91437590 },
			{ 0.19984126, 0.78641367 },
			{ 0.14383161, -0.14100790 }
		};
		
		for(int i = 0; i < 16; ++i) {
			light_vis -= 0.05 * (1.0 - shadow_tex.sample_compare(shadow_smplr,
																 shadow_coord.xy + poisson_disk[i] / 2048.0f,
																 shadow_coord.z - shadow_bias));
		}
#else
		// no shadow
#endif
		
		// diffuse
		lighting += lambert;
		
		// specular
		float spec = spec_tex.sample(smplr, in->tex_coord, tex_grad).x;
		float specular = pow(max(dot(norm_half_dir, normal), 0.0f), spec * 10.0f);
		lighting += specular;
		
		// mul with shadow and light attenuation
		lighting *= light_vis * attenuation;
	}
	lighting = max(lighting, ambient);
	
	return float4(diff.xyz * lighting, 1.0f);
}

fragment scene_scatter_fragment_out scene_scatter_fs(const scene_scatter_in_out in [[stage_in]],
													 const sampler smplr [[sampler(0)]],
													 texture2d<float> diff_tex [[texture(0)]],
													 texture2d<float> spec_tex [[texture(1)]],
													 texture2d<float> norm_tex [[texture(2)]],
													 texture2d<float> mask_tex [[texture(3)]],
													 depth2d<float> shadow_tex [[texture(4)]]) {
	return {
		scene_fs((const thread scene_base_in_out*)&in, smplr, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
		encode_3d_motion(in.motion)
	};
}

fragment scene_gather_fragment_out scene_gather_fs(const scene_gather_in_out in [[stage_in]],
												   const sampler smplr [[sampler(0)]],
												   texture2d<float> diff_tex [[texture(0)]],
												   texture2d<float> spec_tex [[texture(1)]],
												   texture2d<float> norm_tex [[texture(2)]],
												   texture2d<float> mask_tex [[texture(3)]],
												   depth2d<float> shadow_tex [[texture(4)]]) {
	return {
		scene_fs((const thread scene_base_in_out*)&in, smplr, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		(in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w),
		(in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w)
	};
}

//////////////////////////////////////////
// shadow map

struct shadow_uniforms_t {
	matrix_float4x4 mvpm;
};

struct shadow_in_out {
	float4 position [[position]];
};

vertex shadow_in_out shadow_vs(device const packed_float3* in_position [[buffer(0)]],
							   constant shadow_uniforms_t& uniforms [[buffer(1)]],
							   const unsigned int vid [[vertex_id]]) {
	shadow_in_out out;
	
	float4 position(in_position[vid], 1.0f);
	out.position = log_depth(uniforms.mvpm * position);
	
	return out;
}

//////////////////////////////////////////
// sky box

struct skybox_base_in_out {
	float4 position;
	float3 cube_tex_coord;
};

struct skybox_scatter_in_out {
	float4 position [[position]];
	float3 cube_tex_coord;
	float4 cur_pos;
	float4 prev_pos;
};

struct skybox_gather_in_out {
	float4 position [[position]];
	float3 cube_tex_coord;
	float4 motion_prev;
	float4 motion_now;
	float4 motion_next;
};

struct skybox_base_uniforms_t {
	matrix_float4x4 imvpm;
	matrix_float4x4 m0;
	matrix_float4x4 m1;
};

struct skybox_scatter_uniforms_t {
	matrix_float4x4 imvpm;
	matrix_float4x4 prev_imvpm;
	matrix_float4x4 m1; // unused
};

struct skybox_gather_uniforms_t {
	matrix_float4x4 imvpm;
	matrix_float4x4 next_mvpm;
	matrix_float4x4 prev_mvpm;
};


static void skybox_vs(thread skybox_base_in_out* out,
					  constant skybox_base_uniforms_t* uniforms,
					  const unsigned int vid) {
	const float2 fullscreen_triangle[3] {
		float2(0.0f, 2.0f), float2(-3.0f, -1.0f), float2(3.0f, -1.0f)
	};
	
	float4 pos(fullscreen_triangle[vid], 1.0f, 1.0f);
	out->position = pos;
	out->cube_tex_coord = (uniforms->imvpm * pos).xyz;
}

vertex skybox_scatter_in_out skybox_scatter_vs(constant skybox_scatter_uniforms_t& uniforms [[buffer(0)]],
											   const unsigned int vid [[vertex_id]]) {
	skybox_scatter_in_out out;
	
	skybox_vs((thread skybox_base_in_out*)&out, (constant skybox_base_uniforms_t*)&uniforms, vid);
	
	out.cur_pos = uniforms.imvpm * out.position;
	out.prev_pos = uniforms.prev_imvpm * out.position;
	
	return out;
}

vertex skybox_gather_in_out skybox_gather_vs(constant skybox_gather_uniforms_t& uniforms [[buffer(0)]],
											 const unsigned int vid [[vertex_id]]) {
	skybox_gather_in_out out;
	
	skybox_vs((thread skybox_base_in_out*)&out, (constant skybox_base_uniforms_t*)&uniforms, vid);
	
	const float4 proj_vertex = uniforms.imvpm * out.position;
	out.motion_prev = uniforms.prev_mvpm * proj_vertex;
	out.motion_now = out.position;
	out.motion_next = uniforms.next_mvpm * proj_vertex;
	
	return out;
}

static float4 skybox_fs(const thread skybox_base_in_out* in,
						texturecube<float> skybox_tex) {
	constexpr sampler skybox_smplr {
		coord::normalized,
		filter::linear,
		mag_filter::linear,
		min_filter::linear,
		address::clamp_to_edge
	};
	
	return skybox_tex.sample(skybox_smplr, in->cube_tex_coord);
}

fragment scene_scatter_fragment_out skybox_scatter_fs(const skybox_scatter_in_out in [[stage_in]],
													  texturecube<float> skybox_tex [[texture(0)]]) {
	return {
		skybox_fs((const thread skybox_base_in_out*)&in, skybox_tex),
		encode_3d_motion(in.prev_pos.xyz - in.cur_pos.xyz)
	};
}

fragment scene_gather_fragment_out skybox_gather_fs(const skybox_gather_in_out in [[stage_in]],
													texturecube<float> skybox_tex [[texture(0)]]) {
	return {
		skybox_fs((const thread skybox_base_in_out*)&in, skybox_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		(in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w),
		(in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w)
	};
}

//////////////////////////////////////////
// manual blitting

struct blit_in_out {
	float4 position [[position]];
};

vertex blit_in_out blit_vs(const unsigned int vid [[vertex_id]]) {
	constexpr const float2 fullscreen_triangle[3] {
		float2 { 1.0f, 1.0f },
		float2 { 1.0f, -3.0f },
		float2 { -3.0f, 1.0f }
	};
	blit_in_out out;
	out.position = float4(fullscreen_triangle[vid], 0.0, 1.0);
	return out;
}

fragment half4 blit_fs(const blit_in_out in [[stage_in]],
					   texture2d<half, access::read> img [[texture(0)]]) {
	return img.read(uint2 { uint(in.position.x), uint(in.position.y) });
	//return img.read(uint2 { uint(in.position.x), img.get_height() - uint(in.position.y) - 1u });
}
fragment half4 blit_swizzle_fs(const blit_in_out in [[stage_in]],
							   texture2d<half, access::read> img [[texture(0)]]) {
	const half4 color = img.read(uint2 { uint(in.position.x), uint(in.position.y) });
	return { color.z, color.y, color.x, color.w };
}
