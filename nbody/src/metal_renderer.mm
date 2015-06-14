/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include "metal_renderer.hpp"
#include "nbody_state.hpp"
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#if !defined(FLOOR_IOS)
#import <AppKit/AppKit.h>
#define UI_VIEW_CLASS NSView
#else
#import <UIKit/UIKit.h>
#define UI_VIEW_CLASS UIView
#endif
#import <QuartzCore/CAMetalLayer.h>

//
@interface metal_view : UI_VIEW_CLASS <NSCoding>
@property (nonatomic) CAMetalLayer* metal_layer;
@end

@implementation metal_view
@synthesize metal_layer = _metal_layer;

// override to signal this is a CAMetalLayer
+ (Class)layerClass {
	return [CAMetalLayer class];
}

// override to signal this is a CAMetalLayer
- (CALayer*)makeBackingLayer {
	return [CAMetalLayer layer];
}

- (instancetype)initWithFrame:(CGRect)frame withDevice:(id <MTLDevice>)device {
	self = [super initWithFrame:frame];
	//log_debug("frame: %f %f, %f %f", frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
	if(self) {
#if !defined(FLOOR_IOS)
		[self setWantsLayer:true];
#endif
		_metal_layer = (CAMetalLayer*)self.layer;
		_metal_layer.device = device;
		_metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
#if defined(FLOOR_IOS)
		_metal_layer.opaque = true;
		_metal_layer.backgroundColor = nil;
#endif
		_metal_layer.framebufferOnly = true; // note: must be false if used for compute processing
		log_debug("render device: %s", [[device name] UTF8String]);
	}
	return self;
}
@end

// renderer
static id <MTLRenderPipelineState> pipeline_state;
static id <MTLDepthStencilState> depth_state;

static id <MTLLibrary> metal_shd_lib;
struct metal_shader_object {
	id <MTLFunction> vertex_program;
	id <MTLFunction> fragment_program;
};

static unordered_map<string, shared_ptr<metal_shader_object>> shader_objects;

static UI_VIEW_CLASS* window_view { nullptr };
static metal_view* view { nullptr };
static CAMetalLayer* layer { nullptr };
static MTLRenderPassDescriptor* render_pass_desc { nullptr };

struct uniforms_t {
	matrix4f mvpm;
	matrix4f mvm;
	float2 mass_minmax;
};
static id <MTLBuffer> uniforms_buffer;

static array<id <MTLTexture>, 2> body_textures {};
static void create_textures(id <MTLDevice> device) {
	// create/generate an opengl texture and bind it
	for(size_t i = 0; i < body_textures.size(); ++i) {
		// create texture
		static constexpr uint2 texture_size { 64, 64 };
		static constexpr float2 texture_sizef { texture_size };
		array<uint32_t, texture_size.x * texture_size.y> pixel_data;
		for(uint32_t y = 0; y < texture_size.y; ++y) {
			for(uint32_t x = 0; x < texture_size.x; ++x) {
				float2 dir = (float2(x, y) / texture_sizef) * 2.0f - 1.0f;
#if 1 // smoother, less of a center point
				float fval = dir.dot();
#else
				float fval = dir.length();
#endif
				uint32_t val = 255u - uint8_t(const_math::clamp(fval, 0.0f, 1.0f) * 255.0f);
				if(i == 0) {
					pixel_data[y * texture_size.x + x] = val + (val << 8u) + (val << 16u) + (val << 24u);
				}
				else {
					uint32_t alpha_val = (val > 16u ? 0xFFu : val);
					pixel_data[y * texture_size.x + x] = val + (val << 8u) + (val << 16u) + (alpha_val << 24u);
				}
			}
		}
		
		MTLTextureDescriptor* tex_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																							width:texture_size.x
																						   height:texture_size.y
																						mipmapped:true]; // breaks if false?
#if !defined(FLOOR_IOS)
		//tex_desc.resourceOptions = MTLResourceStorageModePrivate; // TODO: how to upload to gpu if enabled?
		//tex_desc.storageMode = MTLStorageModePrivate;
		tex_desc.textureUsage = MTLTextureUsageShaderRead;
#endif
		body_textures[i] = [device newTextureWithDescriptor:tex_desc];
		[body_textures[i] replaceRegion:MTLRegionMake2D(0, 0, texture_size.x, texture_size.y)
							mipmapLevel:0
							  withBytes:&pixel_data[0]
							bytesPerRow:texture_size.x * 4];
	}
}

bool metal_renderer::init(shared_ptr<compute_device> dev) {
	auto device = ((metal_device*)dev.get())->device;
	create_textures(device);
	if(!compile_shaders(dev)) return false;
	
	//
	uniforms_buffer = [device newBufferWithLength:sizeof(uniforms_t) options:MTLResourceCPUCacheModeWriteCombined];
	
	//
	MTLRenderPipelineDescriptor* pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
	pipelineStateDescriptor.label = @"nbody pipeline";
	pipelineStateDescriptor.sampleCount = 1;
	pipelineStateDescriptor.vertexFunction = shader_objects["SPRITE"]->vertex_program;
	pipelineStateDescriptor.fragmentFunction = shader_objects["SPRITE"]->fragment_program;
	pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
	pipelineStateDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
	pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	pipelineStateDescriptor.colorAttachments[0].blendingEnabled = true;
	pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
	pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
	pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceColor;
	pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceColor;
	pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
	pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
	
	NSError* error = nullptr;
	pipeline_state = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
	if(!pipeline_state) {
		log_error("failed to create pipeline state: %s",
				  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
		return false;
	}
	
	//
	MTLDepthStencilDescriptor* depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
	depthStateDesc.depthCompareFunction = MTLCompareFunctionAlways;
	depthStateDesc.depthWriteEnabled = NO;
	depth_state = [device newDepthStencilStateWithDescriptor:depthStateDesc];
	
	// since sdl doesn't have metal support (yet), we need to create a metal view ourselves
	SDL_SysWMinfo info;
	if(!SDL_GetWindowWMInfo(floor::get_window(), &info)) {
		log_error("failed to retrieve window info: %s", SDL_GetError());
		return false;
	}
	CGRect frame { { 0.0f, 0.0f }, { float(floor::get_width()), float(floor::get_height()) } };
#if !defined(FLOOR_IOS)
	frame.size.width /= [[info.info.cocoa.window screen] backingScaleFactor];
	frame.size.height /= [[info.info.cocoa.window screen] backingScaleFactor];
#else
	frame.size.width /= [[info.info.uikit.window screen] scale];
	frame.size.height /= [[info.info.uikit.window screen] scale];
#endif
	view = [[metal_view alloc] initWithFrame:frame withDevice:device];
#if !defined(FLOOR_IOS)
	[[info.info.cocoa.window contentView] addSubview:view];
#else
	[info.info.uikit.window addSubview:view];
#endif
	
	//
	render_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
	MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc.colorAttachments[0];
	// color_attachment.texture set later
	color_attachment.loadAction = MTLLoadActionClear;
	color_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
	color_attachment.storeAction = MTLStoreActionStore;
	
	return true;
}

void metal_renderer::render(shared_ptr<compute_queue> dev_queue,
							shared_ptr<compute_buffer> position_buffer) {
	@autoreleasepool {
		auto drawable = [view.metal_layer nextDrawable];
		if(drawable == nil) {
			log_error("drawable is nil!");
			return;
		}
		render_pass_desc.colorAttachments[0].texture = drawable.texture;
		
		//
		const matrix4f mproj { matrix4f().perspective(90.0f, float(floor::get_width()) / float(floor::get_height()),
													  0.25f, nbody_state.max_distance) };
		const matrix4f mview { nbody_state.cam_rotation.to_matrix4() * matrix4f().translate(0.0f, 0.0f, -nbody_state.distance) };
		const uniforms_t uniforms {
			.mvpm = mview * mproj,
			.mvm = mview,
			.mass_minmax = nbody_state.mass_minmax
		};
		memcpy([uniforms_buffer contents], &uniforms, sizeof(uniforms_t));
		
		//
		auto mtl_queue = ((metal_queue*)dev_queue.get())->get_queue();
		
		id <MTLCommandBuffer> commandBuffer = [mtl_queue commandBuffer];
		commandBuffer.label = @"nbody render";
		
		//
		id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:render_pass_desc];
		renderEncoder.label = @"nbody RenderEncoder";
		[renderEncoder setDepthStencilState:depth_state];
		
		//
		[renderEncoder pushDebugGroup:@"bodies"];
		[renderEncoder setRenderPipelineState:pipeline_state];
		[renderEncoder setVertexBuffer:((metal_buffer*)position_buffer.get())->get_metal_buffer() offset:0 atIndex:0];
		[renderEncoder setVertexBuffer:uniforms_buffer offset:0 atIndex:1];
		[renderEncoder setFragmentTexture:body_textures[0] atIndex:0];
		
		//
		[renderEncoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:nbody_state.body_count];
		
		//
		[renderEncoder endEncoding];
		[renderEncoder popDebugGroup];
		
		[commandBuffer presentDrawable:drawable];
		[commandBuffer commit];
		//[commandBuffer waitUntilCompleted];
	}
}

bool metal_renderer::compile_shaders(shared_ptr<compute_device> dev) {
	static const char metal_test_shader[] { u8R"RAWSTR(
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
	)RAWSTR"};
	
	id <MTLDevice> mtl_dev = ((metal_device*)dev.get())->device;
	
	MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
#if !defined(FLOOR_IOS)
	[options setLanguageVersion:MTLLanguageVersion1_1];
#endif
	
	NSError* err = nullptr;
	metal_shd_lib = [mtl_dev newLibraryWithSource:[NSString stringWithUTF8String:metal_test_shader]
										  options:options
											error:&err];
	if(!metal_shd_lib) {
		log_error("failed to create shader lib: %s", (err != nullptr ? [[err localizedDescription] UTF8String] : "unknown error"));
		return false;
	}
	
	auto shd = make_shared<metal_shader_object>();
	shd->vertex_program = [metal_shd_lib newFunctionWithName:@"lighting_vertex"];
	shd->fragment_program = [metal_shd_lib newFunctionWithName:@"lighting_fragment"];
	shader_objects.emplace("SPRITE", shd);
	return true;
}
