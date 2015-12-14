
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
	half3 view_dir;
	half3 light_dir;
};

struct scene_scatter_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
	half3 view_dir;
	half3 light_dir;
	float3 motion;
};

struct scene_gather_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
	half3 view_dir;
	half3 light_dir;
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
	half4 color [[color(0)]];
	uint32_t motion [[color(1)]];
};

struct scene_gather_fragment_out {
	half4 color [[color(0)]];
	uint32_t motion_forward [[color(1)]];
	uint32_t motion_backward [[color(2)]];
	half2 motion_depth [[color(3)]];
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

// also used for forward-only
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
	//cmotion = precise::log2(cmotion + 1.0);
	cmotion = fast::log2(cmotion + 1.0);
	// encode x and z with 10-bit, y with 9-bit
	//cmotion *= 1.0 / precise::log2(range + 1.0);
	cmotion *= 0.16604764621594f; // no constexpr math :(
	cmotion.xz *= 1024.0f; // 2^10
	cmotion.y *= 512.0f; // 2^9
	cmotion.xz = clamp(cmotion.xz, 0.0f, 1023.0f);
	cmotion.y = clamp(cmotion.y, 0.0f, 511.0f);
	const uint3 ui_cmotion = (uint3)cmotion;
	return ((signs.x < 0.0f ? 0x80000000u : 0u) |
			(signs.y < 0.0f ? 0x40000000u : 0u) |
			(signs.z < 0.0f ? 0x20000000u : 0u) |
			(ui_cmotion.x << 19u) |
			(ui_cmotion.y << 10u) |
			(ui_cmotion.z));
}

static uint32_t encode_2d_motion(thread const float2& motion) {
	// could use this if it wasn't broken on intel gpus ...
	//return pack_float_to_snorm2x16(motion);
	float2 cmotion = clamp(motion * 32767.0f, -32767.0f, 32767.0f); // +/- 2^15 - 1, fit into 16 bits
	//return as_type<uint>(short2(cmotion)); // this is also broken
	uint2 umotion = as_type<uint2>(int2(cmotion)) & 0xFFFFu;
	return (umotion.x | (umotion.y << 16u));
}

static half4 scene_fs(const thread scene_base_in_out* in,
					  const sampler smplr,
					  texture2d<half> diff_tex,
					  texture2d<half> spec_tex,
					  texture2d<half> norm_tex,
					  texture2d<half> mask_tex,
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
	half3 norm_view_dir = normalize(in->view_dir);
	half3 norm_light_dir = normalize(in->light_dir);
	half3 norm_half_dir = normalize(norm_view_dir + norm_light_dir);
	half3 normal = norm_tex.sample(smplr, in->tex_coord, tex_grad).xyz * 2.0f - 1.0f;
	
	half lambert = dot(normal, norm_light_dir);
	if(lambert > 0.0f) {
		// shadow hackery
		half bias_lambert = min(lambert, 0.99995h); // clamp to "(0, 1)", already > 0 here
		half slope = sqrt(1.0f - bias_lambert * bias_lambert) / bias_lambert; // = tan(acos(lambert))
		half shadow_bias = clamp(fixed_bias * slope, 0.0f, fixed_bias * 2.0f);
		
		float3 shadow_coord = in->shadow_coord.xyz / in->shadow_coord.w;
		shadow_coord.y = 1.0f - shadow_coord.y; // why metal, why?
#if 1
		if(in->shadow_coord.w > 0.0f) {
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
		half spec = spec_tex.sample(smplr, in->tex_coord, tex_grad).x;
		half specular = pow(max(dot(norm_half_dir, normal), 0.0h), spec * 10.0h);
		lighting += specular;
		
		// mul with shadow and light attenuation
		lighting *= light_vis * attenuation;
	}
	lighting = max(lighting, ambient);
	
	return half4(diff.xyz * lighting, 1.0f);
}

fragment scene_scatter_fragment_out scene_scatter_fs(const scene_scatter_in_out in [[stage_in]],
													 const sampler smplr [[sampler(0)]],
													 texture2d<half> diff_tex [[texture(0)]],
													 texture2d<half> spec_tex [[texture(1)]],
													 texture2d<half> norm_tex [[texture(2)]],
													 texture2d<half> mask_tex [[texture(3)]],
													 depth2d<float> shadow_tex [[texture(4)]]) {
	return {
		scene_fs((const thread scene_base_in_out*)&in, smplr, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
		encode_3d_motion(in.motion)
	};
}

fragment scene_gather_fragment_out scene_gather_fs(const scene_gather_in_out in [[stage_in]],
												   const sampler smplr [[sampler(0)]],
												   texture2d<half> diff_tex [[texture(0)]],
												   texture2d<half> spec_tex [[texture(1)]],
												   texture2d<half> norm_tex [[texture(2)]],
												   texture2d<half> mask_tex [[texture(3)]],
												   depth2d<float> shadow_tex [[texture(4)]]) {
	return {
		scene_fs((const thread scene_base_in_out*)&in, smplr, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		half2 {
			(half)((in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w)),
			(half)((in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w))
		}
	};
}

fragment scene_scatter_fragment_out /* reuse */ scene_gather_fwd_fs(const scene_gather_in_out in [[stage_in]],
																	const sampler smplr [[sampler(0)]],
																	texture2d<half> diff_tex [[texture(0)]],
																	texture2d<half> spec_tex [[texture(1)]],
																	texture2d<half> norm_tex [[texture(2)]],
																	texture2d<half> mask_tex [[texture(3)]],
																	depth2d<float> shadow_tex [[texture(4)]]) {
	return {
		scene_fs((const thread scene_base_in_out*)&in, smplr, diff_tex, spec_tex, norm_tex, mask_tex, shadow_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
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
	constexpr const float2 fullscreen_triangle[3] {
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

// also used for forward-only
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

static half4 skybox_fs(const thread skybox_base_in_out* in,
						texturecube<half> skybox_tex) {
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
													  texturecube<half> skybox_tex [[texture(0)]]) {
	return {
		skybox_fs((const thread skybox_base_in_out*)&in, skybox_tex),
		encode_3d_motion(in.prev_pos.xyz - in.cur_pos.xyz)
	};
}

fragment scene_gather_fragment_out skybox_gather_fs(const skybox_gather_in_out in [[stage_in]],
													texturecube<half> skybox_tex [[texture(0)]]) {
	return {
		skybox_fs((const thread skybox_base_in_out*)&in, skybox_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w)),
		encode_2d_motion((in.motion_prev.xy / in.motion_prev.w) - (in.motion_now.xy / in.motion_now.w)),
		half2 {
			(half)((in.motion_next.z / in.motion_next.w) - (in.motion_now.z / in.motion_now.w)),
			(half)((in.motion_prev.z / in.motion_prev.w) - (in.motion_now.z / in.motion_now.w))
		}
	};
}

fragment scene_scatter_fragment_out /* reuse */ skybox_gather_fwd_fs(const skybox_gather_in_out in [[stage_in]],
																	 texturecube<half> skybox_tex [[texture(0)]]) {
	return {
		skybox_fs((const thread skybox_base_in_out*)&in, skybox_tex),
		encode_2d_motion((in.motion_next.xy / in.motion_next.w) - (in.motion_now.xy / in.motion_now.w))
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

//////////////////////////////////////////
// gather test

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define SCREEN_ORIGIN_LEFT_TOP 1
#define TILE_SIZE_X 32
#define TILE_SIZE_Y 16
#define DEFAULT_DEPTH_TYPE depth_type::normalized

// depth buffer types
enum class depth_type {
	// normalized in [0, 1], default for opengl and metal
	normalized,
	// z/w depth
	z_div_w,
	// log depth, computed in software
	// NOTE/TODO: not supported yet
	log,
	// linear depth [0, far-plane]
	linear,
};

namespace warp_camera {
	// 1 / screen size in fp
	static constant constexpr const float2 inv_screen_size { 1.0f / float(SCREEN_WIDTH), 1.0f / float(SCREEN_HEIGHT) };
	// [near, far] plane, needed for depth correction
	static constant constexpr const float2 near_far_plane { 0.5f, 500.0f };
	
	// linearizes the input depth value according to the depth type and returns the real world depth value
	template <depth_type type = DEFAULT_DEPTH_TYPE>
	static float linearize_depth(thread const float& depth) {
		if(type == depth_type::normalized) {
			// reading from the actual depth buffer which is normalized in [0, 1]
			const float2 near_far_projection {
				-(near_far_plane.y + near_far_plane.x) / (near_far_plane.x - near_far_plane.y),
				(2.0f * near_far_plane.y * near_far_plane.x) / (near_far_plane.x - near_far_plane.y),
			};
			// special case: clear/full depth, assume this comes from a normalized sky box
			if(depth == 1.0f) return 1.0f;
			return near_far_projection.y / (depth - near_far_projection.x);
		}
		else if(type == depth_type::z_div_w) {
			// depth is written as z/w in shader -> need to perform a small adjustment to account for near/far plane to get the real world depth
			// (note that this error is almost imperceptible and could just be ignored)
			return depth + near_far_plane.x - (depth * (near_far_plane.x / near_far_plane.y));
		}
		else if(type == depth_type::log) {
			// TODO: implement this
			return 0.0f;
		}
		else if(type == depth_type::linear) {
			// already linear, just pass through
			return depth;
		}
	}
};

// decodes the encoded input 2D motion vector
// format: [16-bit y][16-bit x]
static float2 decode_2d_motion(const uint32_t encoded_motion) {
	const ushort2 us16 {
		(uint16_t)(encoded_motion & 0xFFFFu),
		(uint16_t)((encoded_motion >> 16u) & 0xFFFFu)
	};
	// map [-32767, 32767] -> [-0.5, 0.5]
	// if the origin is at the top left, the y component points in the opposite/wrong direction
	return float2(as_type<short2>(us16)) * float2 { 0.5f / 32767.0f, -0.5f / 32767.0f };
}

// NOTE: reusing blit vertex program / output

fragment half4 warp_gather(const blit_in_out in [[stage_in]],
						   texture2d<half, access::sample> img_color [[texture(0)]],
						   depth2d<float, access::sample> img_depth [[texture(1)]],
						   texture2d<half, access::sample> img_color_prev [[texture(2)]],
						   depth2d<float, access::sample> img_depth_prev [[texture(3)]],
						   texture2d<uint32_t, access::sample> img_motion_forward [[texture(4)]],
						   texture2d<uint32_t, access::sample> img_motion_backward [[texture(5)]],
						   // packed <forward depth: fwd t-1 -> t (used here), backward depth: bwd t-1 -> t-2 (unused here)>
						   texture2d<half, access::sample> img_motion_depth_forward [[texture(6)]],
						   // packed <forward depth: t+1 -> t (unused here), backward depth: t -> t-1 (used here)>
						   texture2d<half, access::sample> img_motion_depth_backward [[texture(7)]],
						   constant float& relative_delta,
						   constant float& epsilon_1,
						   constant float& epsilon_2) {
	constexpr sampler smplr {
		coord::normalized,
		filter::nearest,
		mag_filter::nearest,
		min_filter::nearest,
		address::clamp_to_edge
	};
	
	// iterate
	const float2 screen_coord = in.position.xy / in.position.w;
	const float2 p_init = (screen_coord + 0.5f) * warp_camera::inv_screen_size; // start at pixel center (this is p_t+alpha)
	// dual init, opposing init
	float2 p_fwd = p_init + relative_delta * decode_2d_motion(img_motion_backward.sample(smplr, p_init).x);
	float2 p_bwd = p_init + (1.0f - relative_delta) * decode_2d_motion(img_motion_forward.sample(smplr, p_init).x);
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(img_motion_forward.sample(smplr, p_fwd).x);
		p_fwd = p_init - relative_delta * motion;
	}
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(img_motion_backward.sample(smplr, p_bwd).x);
		p_bwd = p_init - (1.0f - relative_delta) * motion;
	}
	
	//
	const auto color_fwd = img_color_prev.sample(smplr, p_fwd);
	const auto color_bwd = img_color.sample(smplr, p_bwd);
	
	// read final motion vector + depth (packed)
	const auto motion_fwd = decode_2d_motion(img_motion_forward.sample(smplr, p_fwd).x);
	const auto motion_bwd = decode_2d_motion(img_motion_backward.sample(smplr, p_bwd).x);
	const auto depth_fwd = img_motion_depth_forward.sample(smplr, p_fwd).x;
	const auto depth_bwd = img_motion_depth_backward.sample(smplr, p_bwd).y;
	
	// compute screen space error
	const auto err_vec_fwd = p_fwd + relative_delta * motion_fwd - p_init;
	const auto err_fwd = (dot(err_vec_fwd, err_vec_fwd) +
						  // account for out-of-bound access (-> large error so any checks will fail)
						  (any(p_fwd < 0.0f) || any(p_fwd > 1.0f) ? 1e10f : 0.0f));
	const auto err_vec_bwd = p_bwd + (1.0f - relative_delta) * motion_bwd - p_init;
	const auto err_bwd = (dot(err_vec_bwd, err_vec_bwd) +
						  (any(p_bwd < 0.0f) || any(p_bwd > 1.0f) ? 1e10f : 0.0f));
	// TODO: should have a more tangible epsilon, e.g. max pixel offset -> (max_offset / screen_size).max_element()
	const float epsilon_1_sq { epsilon_1 * epsilon_1 };
	
	// NOTE: scene depth type is dependent on the renderer (-> use the default), motion depth is always z/w
	// -> need to linearize both to properly add + compare them
	const auto z_fwd = (warp_camera::linearize_depth(img_depth_prev.sample(smplr, p_fwd)) +
						relative_delta * warp_camera::linearize_depth<depth_type::z_div_w>(depth_fwd));
	const auto z_bwd = (warp_camera::linearize_depth(img_depth.sample(smplr, p_bwd)) +
						(1.0f - relative_delta) * warp_camera::linearize_depth<depth_type::z_div_w>(depth_bwd));
	const auto depth_diff = abs(z_fwd - z_bwd);
	//constexpr const float epsilon_2 { 4.0f }; // aka "max depth difference between fwd and bwd"
	
	const bool fwd_valid = (err_fwd < epsilon_1_sq);
	const bool bwd_valid = (err_bwd < epsilon_1_sq);
	half4 color;
	{
		const auto proj_color_fwd = mix(color_fwd, img_color.sample(smplr, p_fwd + motion_fwd), (half)relative_delta);
		const auto proj_color_bwd = mix(img_color_prev.sample(smplr, p_bwd + motion_bwd), color_bwd, (half)relative_delta);
		if(fwd_valid && bwd_valid) {
			if(depth_diff < epsilon_2) {
				// case 1: both fwd and bwd are valid
				if(err_fwd < err_bwd) {
					color = proj_color_fwd;
				}
				else {
					color = proj_color_bwd;
				}
			}
			else {
				// case 2: select the one closer to the camera (occlusion)
				if(z_fwd < z_bwd) {
					// depth from other frame
					const auto z_fwd_other = (img_depth.sample(smplr, p_fwd + motion_fwd) +
											  (1.0f - relative_delta) * img_motion_depth_backward.sample(smplr,
																										 p_fwd + motion_fwd).y);
					color = (abs(z_fwd - z_fwd_other) < epsilon_2 ? proj_color_fwd : color_fwd);
				}
				else { // bwd < fwd
					const auto z_bwd_other = (img_depth_prev.sample(smplr, p_bwd + motion_bwd) +
											  relative_delta * img_motion_depth_forward.sample(smplr,
																							   p_bwd + motion_bwd).x);
					color = (abs(z_bwd - z_bwd_other) < epsilon_2 ? proj_color_bwd : color_bwd);
				}
			}
		}
		else if(fwd_valid) {
			color = color_fwd;
		}
		else if(bwd_valid) {
			color = color_bwd;
		}
		// case 3 / else: both are invalid -> just do a linear interpolation between the two
		else {
			color = mix(color_fwd, color_bwd, (half)relative_delta);
		}
	}
	
	return color;
}

kernel void warp_gather_kernel(texture2d<half, access::sample> img_color [[texture(0)]],
							   depth2d<float, access::sample> img_depth [[texture(1)]],
							   texture2d<half, access::sample> img_color_prev [[texture(2)]],
							   depth2d<float, access::sample> img_depth_prev [[texture(3)]],
							   texture2d<uint32_t, access::sample> img_motion_forward [[texture(4)]],
							   texture2d<uint32_t, access::sample> img_motion_backward [[texture(5)]],
							   // packed <forward depth: fwd t-1 -> t (used here), backward depth: bwd t-1 -> t-2 (unused here)>
							   texture2d<half, access::sample> img_motion_depth_forward [[texture(6)]],
							   // packed <forward depth: t+1 -> t (unused here), backward depth: t -> t-1 (used here)>
							   texture2d<half, access::sample> img_motion_depth_backward [[texture(7)]],
							   texture2d<half, access::write> img_out_color [[texture(8)]],
							   constant float& relative_delta,
							   constant float& epsilon_1,
							   constant float& epsilon_2,
							   constant uint32_t& dbg_val,
							   const uint2 global_id [[thread_position_in_grid]]) {
	constexpr sampler smplr {
		coord::normalized,
		filter::nearest,
		mag_filter::nearest,
		min_filter::nearest,
		address::clamp_to_edge
	};
	
	// iterate
	const float2 p_init = (float2(global_id) + 0.5f) * warp_camera::inv_screen_size; // start at pixel center (this is p_t+alpha)
	// dual init, opposing init
	float2 p_fwd = p_init + relative_delta * decode_2d_motion(img_motion_backward.sample(smplr, p_init).x);
	float2 p_bwd = p_init + (1.0f - relative_delta) * decode_2d_motion(img_motion_forward.sample(smplr, p_init).x);
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(img_motion_forward.sample(smplr, p_fwd).x);
		p_fwd = p_init - relative_delta * motion;
	}
	for(uint32_t i = 0; i < 3; ++i) {
		const auto motion = decode_2d_motion(img_motion_backward.sample(smplr, p_bwd).x);
		p_bwd = p_init - (1.0f - relative_delta) * motion;
	}
	
	//
	const auto color_fwd = img_color_prev.sample(smplr, p_fwd);
	const auto color_bwd = img_color.sample(smplr, p_bwd);
	
	// read final motion vector + depth (packed)
	const auto motion_fwd = decode_2d_motion(img_motion_forward.sample(smplr, p_fwd).x);
	const auto motion_bwd = decode_2d_motion(img_motion_backward.sample(smplr, p_bwd).x);
	const auto depth_fwd = img_motion_depth_forward.sample(smplr, p_fwd).x;
	const auto depth_bwd = img_motion_depth_backward.sample(smplr, p_bwd).y;
	
	// compute screen space error
	const auto err_vec_fwd = p_fwd + relative_delta * motion_fwd - p_init;
	const auto err_fwd = (dot(err_vec_fwd, err_vec_fwd) +
						  // account for out-of-bound access (-> large error so any checks will fail)
						  (any(p_fwd < 0.0f) || any(p_fwd > 1.0f) ? 1e10f : 0.0f));
	const auto err_vec_bwd = p_bwd + (1.0f - relative_delta) * motion_bwd - p_init;
	const auto err_bwd = (dot(err_vec_bwd, err_vec_bwd) +
						  (any(p_bwd < 0.0f) || any(p_bwd > 1.0f) ? 1e10f : 0.0f));
	// TODO: should have a more tangible epsilon, e.g. max pixel offset -> (max_offset / screen_size).max_element()
	const float epsilon_1_sq { epsilon_1 * epsilon_1 };
	
	// NOTE: scene depth type is dependent on the renderer (-> use the default), motion depth is always z/w
	// -> need to linearize both to properly add + compare them
	const auto z_fwd = (warp_camera::linearize_depth(img_depth_prev.sample(smplr, p_fwd)) +
						relative_delta * warp_camera::linearize_depth<depth_type::z_div_w>(depth_fwd));
	const auto z_bwd = (warp_camera::linearize_depth(img_depth.sample(smplr, p_bwd)) +
						(1.0f - relative_delta) * warp_camera::linearize_depth<depth_type::z_div_w>(depth_bwd));
	const auto depth_diff = abs(z_fwd - z_bwd);
	//constexpr const float epsilon_2 { 4.0f }; // aka "max depth difference between fwd and bwd"
	
	const bool fwd_valid = (err_fwd < epsilon_1_sq);
	const bool bwd_valid = (err_bwd < epsilon_1_sq);
	half4 color;
	{
		const auto proj_color_fwd = mix(color_fwd, img_color.sample(smplr, p_fwd + motion_fwd), (half)relative_delta);
		const auto proj_color_bwd = mix(img_color_prev.sample(smplr, p_bwd + motion_bwd), color_bwd, (half)relative_delta);
		if(fwd_valid && bwd_valid) {
			if(depth_diff < epsilon_2) {
				// case 1: both fwd and bwd are valid
				if(err_fwd < err_bwd) {
					color = proj_color_fwd;
				}
				else {
					color = proj_color_bwd;
				}
			}
			else {
				// case 2: select the one closer to the camera (occlusion)
				if(z_fwd < z_bwd) {
					// depth from other frame
					const auto z_fwd_other = (img_depth.sample(smplr, p_fwd + motion_fwd) +
											  (1.0f - relative_delta) * img_motion_depth_backward.sample(smplr,
																										 p_fwd + motion_fwd).y);
					color = (abs(z_fwd - z_fwd_other) < epsilon_2 ? proj_color_fwd : color_fwd);
				}
				else { // bwd < fwd
					const auto z_bwd_other = (img_depth_prev.sample(smplr, p_bwd + motion_bwd) +
											  relative_delta * img_motion_depth_forward.sample(smplr,
																							   p_bwd + motion_bwd).x);
					color = (abs(z_bwd - z_bwd_other) < epsilon_2 ? proj_color_bwd : color_bwd);
				}
			}
		}
		else if(fwd_valid) {
			color = color_fwd;
		}
		else if(bwd_valid) {
			color = color_bwd;
		}
		// case 3 / else: both are invalid -> just do a linear interpolation between the two
		else {
			color = mix(color_fwd, color_bwd, (half)relative_delta);
		}
	}
	
	img_out_color.write(color, global_id);
}
