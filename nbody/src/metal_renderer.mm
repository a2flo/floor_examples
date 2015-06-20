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

#if !defined(FLOOR_NO_METAL)
#include "nbody_state.hpp"
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/darwin/darwin_helper.hpp>

//
/*@protocol MTLFunctionSPI <MTLFunction>
@property(readonly) long long lineNumber;
@property(copy) NSString *filePath;
@end

@interface _MTLLibrary : NSObject <MTLLibrary> {
	NSMutableDictionary *_functionDictionary;
}
@property(readonly, retain, nonatomic) NSMutableDictionary *functionDictionary;
@end*/

// renderer
static id <MTLRenderPipelineState> pipeline_state;
static id <MTLDepthStencilState> depth_state;

static id <MTLLibrary> metal_shd_lib;
struct metal_shader_object {
	id <MTLFunction> vertex_program;
	id <MTLFunction> fragment_program;
};

static unordered_map<string, shared_ptr<metal_shader_object>> shader_objects;

static metal_view* view { nullptr };
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
	view = darwin_helper::create_metal_view(floor::get_window(), device);
	if(view == nullptr) {
		log_error("failed to create metal view!");
		return false;
	}
	
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
		auto drawable = darwin_helper::get_metal_next_drawable(view);
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
	id <MTLDevice> mtl_dev = ((metal_device*)dev.get())->device;
#if defined(FLOOR_IOS)
	metal_shd_lib = [mtl_dev newLibraryWithFile:@"default.metallib" error:nil];
#else
	metal_shd_lib = [mtl_dev newDefaultLibrary];
#endif
	if(!metal_shd_lib) {
		log_error("failed to load default shader lib!");
		return false;
	}
	
	/*auto path = [NSString stringWithUTF8String:"/Users/flo/sync/floor_examples/nbody/src/metal_shaders.metal"];
	_MTLLibrary* lib = ([metal_shd_lib respondsToSelector:@selector(baseObject)] ?
						[metal_shd_lib performSelector:@selector(baseObject)] :
						metal_shd_lib);
	for(id key in [lib functionDictionary]) {
		id <MTLFunctionSPI> func = [[lib functionDictionary] objectForKey:key];
		func.filePath = path;
	}*/
	
	auto shd = make_shared<metal_shader_object>();
	shd->vertex_program = [metal_shd_lib newFunctionWithName:@"lighting_vertex"];
	shd->fragment_program = [metal_shd_lib newFunctionWithName:@"lighting_fragment"];
	shader_objects.emplace("SPRITE", shd);
	return true;
}

#endif
