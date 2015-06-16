
#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

constant float4 gradients[4] {
	float4(1.0, 0.2, 0.0, 1.0),
	float4(1.0, 1.0, 0.0, 1.0),
	float4(1.0, 1.0, 1.0, 1.0),
	float4(0.5, 1.0, 1.0, 1.0)
};

struct uniforms_t {
	matrix_float4x4 mvpm;
	matrix_float4x4 mvm;
	float2 mass_minmax;
};

struct vertex_t {
	packed_float4 position;
};

struct ShdInOut {
	float4 position [[position]];
	float size [[point_size]];
	half4 color;
};

static half4 compute_gradient(thread const float& interpolator) {
	float4 color = float4(0.0);
	// built-in step function can not be trusted -> branch instead ...
	if(interpolator < 0.33333) {
		float interp = smoothstep(0.0, 0.33333, interpolator);
		color += mix(gradients[0], gradients[1], interp);
	}
	else if(interpolator < 0.66666) {
		float interp = smoothstep(0.33333, 0.66666, interpolator);
		color += mix(gradients[1], gradients[2], interp);
	}
	else if(interpolator <= 1.0) {
		float interp = smoothstep(0.66666, 1.0, interpolator);
		color += mix(gradients[2], gradients[3], interp);
	}
	return half4(color);
}

vertex ShdInOut lighting_vertex(device const vertex_t* vertex_array [[buffer(0)]],
								constant uniforms_t& uniforms [[buffer(1)]],
								unsigned int vid [[vertex_id]]) {
	ShdInOut out;
	
	const float4 in_vertex = vertex_array[vid].position;
	float size = 128.0 / (1.0 - float3(uniforms.mvm * float4(in_vertex.xyz, 1.0)).z);
	float mass_scale = (in_vertex.w - uniforms.mass_minmax.x) / (uniforms.mass_minmax.y - uniforms.mass_minmax.x);
	mass_scale *= mass_scale; // ^2
	//mass_scale *= mass_scale; // ^4
	//mass_scale *= mass_scale; // ^8
	size *= mass_scale;
	out.size = clamp(size, 2.0, 255.0);
	out.position = uniforms.mvpm * float4(in_vertex.xyz, 1.0);
	out.color = compute_gradient((in_vertex.w - uniforms.mass_minmax.x) / (uniforms.mass_minmax.y - uniforms.mass_minmax.x));
	return out;
}

fragment half4 lighting_fragment(ShdInOut in [[stage_in]],
								 texture2d<half> tex [[texture(0)]],
								 float2 coord [[point_coord]]) {
	constexpr sampler smplr {
		coord::normalized,
		filter::linear,
		address::clamp_to_edge,
	};
	return tex.sample(smplr, coord) * in.color;
}
