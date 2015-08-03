
#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

//////////////////////////////////////////
// scene

struct scene_uniforms_t {
	matrix_float4x4 mvpm;
	matrix_float4x4 mvm;
	matrix_float4x4 prev_mvm;
	matrix_float4x4 light_bias_mvpm;
	packed_float3 cam_pos;
	packed_float3 light_pos;
};

struct scene_in_out {
	float4 position [[position]];
	float4 shadow_coord;
	float2 tex_coord;
	float3 view_dir;
	float3 light_dir;
	float3 motion;
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

vertex scene_in_out scene_vs(device const packed_float3* in_position [[buffer(0)]],
							 device const packed_float2* in_tex_coord [[buffer(1)]],
							 device const packed_float3* in_normal [[buffer(2)]],
							 device const packed_float3* in_binormal [[buffer(3)]],
							 device const packed_float3* in_tangent [[buffer(4)]],
							 constant scene_uniforms_t& uniforms [[buffer(5)]],
							 const unsigned int vid [[vertex_id]]) {
	scene_in_out out;
	out.tex_coord = in_tex_coord[vid];
	
	float4 pos(in_position[vid], 1.0f);
	out.shadow_coord = log_depth(uniforms.light_bias_mvpm * pos);
	out.position = uniforms.mvpm * pos;
	
	float4 prev_pos = uniforms.prev_mvm * pos;
	float4 cur_pos = uniforms.mvm * pos;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	float3 vview = uniforms.cam_pos - in_position[vid];
	out.view_dir.x = dot(vview, in_tangent[vid]);
	out.view_dir.y = dot(vview, in_binormal[vid]);
	out.view_dir.z = dot(vview, in_normal[vid]);
	
	float3 vlight = uniforms.light_pos - in_position[vid];
	out.light_dir.x = dot(vlight, in_tangent[vid]);
	out.light_dir.y = dot(vlight, in_binormal[vid]);
	out.light_dir.z = dot(vlight, in_normal[vid]);
	
	return out;
}

static uint32_t encode_motion(thread const float3& motion) {
	constexpr const float range = 64.0; // [-range, range]
	const float3 signs = sign(motion);
	float3 cmotion = clamp(abs(motion), 0.0, range);
	// use log2 scaling
	cmotion = precise::log2(cmotion + 1.0);
	// encode x and z with 10-bit, y with 9-bit
	//cmotion *= 1.0 / precise::log2(range + 1.0);
	cmotion *= 0.16604764621594f; // no constexpr math :(
	cmotion.xz *= 1024.0; // 2^10
	cmotion.y *= 512.0; // 2^9
	return ((signs.x < 0.0 ? 0x80000000u : 0u) |
			(signs.y < 0.0 ? 0x40000000u : 0u) |
			(signs.z < 0.0 ? 0x20000000u : 0u) |
			(clamp(uint32_t(cmotion.x), 0u, 1023u) << 19u) |
			(clamp(uint32_t(cmotion.y), 0u, 511u) << 10u) |
			(clamp(uint32_t(cmotion.z), 0u, 1023u)));
}

struct scene_fragment_out {
	float4 color [[color(0)]];
	uint32_t motion [[color(1)]];
};

fragment scene_fragment_out scene_fs(const scene_in_out in [[stage_in]],
									 const sampler smplr [[sampler(0)]],
									 texture2d<float> diff_tex [[texture(0)]],
									 texture2d<float> spec_tex [[texture(1)]],
									 texture2d<float> norm_tex [[texture(2)]],
									 texture2d<float> mask_tex [[texture(3)]],
									 depth2d<float> shadow_tex [[texture(4)]]) {
	constexpr sampler shadow_smplr {
		coord::normalized,
		filter::linear,
		mag_filter::linear,
		min_filter::linear,
		address::clamp_to_edge,
		compare_func::less_equal,
	};
	
	float mask = mask_tex.sample(smplr, in.tex_coord, level { 0.0f }).x;
	if(mask < 0.5f) discard_fragment();
	
	const gradient2d tex_grad { dfdx(in.tex_coord), dfdy(in.tex_coord) };
	auto diff = diff_tex.sample(smplr, in.tex_coord, tex_grad);
	
	//
	constexpr float fixed_bias = 0.001f;
	constexpr float ambient = 0.2f;
	constexpr float attenuation = 0.9f;
	float lighting = 0.0f;
	float light_vis = 1.0f;
	
	//
	float3 norm_view_dir = normalize(in.view_dir);
	float3 norm_light_dir = normalize(in.light_dir);
	float3 norm_half_dir = normalize(norm_view_dir + norm_light_dir);
	float3 normal = norm_tex.sample(smplr, in.tex_coord, tex_grad).xyz * 2.0f - 1.0f;
	
	float lambert = dot(normal, norm_light_dir);
	if(lambert > 0.0f) {
		// shadow hackery
		float bias_lambert = min(lambert, 0.99995f); // clamp to "(0, 1)", already > 0 here
		float slope = sqrt(1.0f - bias_lambert * bias_lambert) / bias_lambert; // = tan(acos(lambert))
		float shadow_bias = clamp(fixed_bias * slope, 0.0f, fixed_bias * 2.0f);
		
		float3 shadow_coord = in.shadow_coord.xyz / in.shadow_coord.w;
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
		float spec = spec_tex.sample(smplr, in.tex_coord, tex_grad).x;
		float specular = pow(max(dot(norm_half_dir, normal), 0.0f), spec * 10.0f);
		lighting += specular;
		
		// mul with shadow and light attenuation
		lighting *= light_vis * attenuation;
	}
	lighting = max(lighting, ambient);
	
	//
	return {
		float4(diff.xyz * lighting, 1.0f),
		encode_motion(in.motion)
	};
}

//////////////////////////////////////////
// motion only
struct motion_uniforms_t {
	matrix_float4x4 mvpm;
	matrix_float4x4 mvm;
	matrix_float4x4 prev_mvm;
};

struct motion_in_out {
	float4 position [[position]];
	float2 tex_coord;
	float3 motion;
};

vertex motion_in_out motion_only_vs(device const packed_float3* in_position [[buffer(0)]],
									device const packed_float2* in_tex_coord [[buffer(1)]],
									constant motion_uniforms_t& uniforms [[buffer(2)]],
									const unsigned int vid [[vertex_id]]) {
	motion_in_out out;
	out.tex_coord = in_tex_coord[vid];
	
	float4 position(in_position[vid], 1.0f);
	out.position = uniforms.mvpm * position;
	
	float4 prev_pos = uniforms.prev_mvm * position;
	float4 cur_pos = uniforms.mvm * position;
	out.motion = cur_pos.xyz - prev_pos.xyz;
	
	return out;
}

fragment float4 motion_only_fs(const motion_in_out in [[stage_in]],
							   texture2d<float> mask_tex [[texture(0)]]) {
	constexpr sampler smplr {
		coord::normalized,
		filter::linear,
		mip_filter::linear,
		mag_filter::linear,
		min_filter::linear,
		address::repeat,
	};
	
	float mask = mask_tex.sample(smplr, in.tex_coord, level { 0.0f }).x;
	if(mask < 0.5f) discard_fragment();
	
	return float4(in.motion, 0.0f);
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
