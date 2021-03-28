/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/darwin/darwin_helper.hpp>

// renderer
static id <MTLRenderPipelineState> pipeline_state;
static id <MTLDepthStencilState> depth_state;

static MTLRenderPassDescriptor* render_pass_desc { nullptr };

static array<shared_ptr<metal_image>, 2> body_textures {};
static void create_textures(const compute_queue& dev_queue) {
	auto ctx = dev_queue.get_device().context;
	
	// create/generate an opengl texture and bind it
	for(size_t i = 0; i < body_textures.size(); ++i) {
		// create texture
		static constexpr uint2 texture_size { 64, 64 };
		static constexpr float2 texture_sizef { texture_size };
		array<ushort4, texture_size.x * texture_size.y> pixel_data;
		for(uint32_t y = 0; y < texture_size.y; ++y) {
			for(uint32_t x = 0; x < texture_size.x; ++x) {
				float2 dir = (float2(x, y) / texture_sizef) * 2.0f - 1.0f;
#if 1 // smoother, less of a center point
				float fval = dir.dot();
#else
				float fval = dir.length();
#endif
				const auto val = 1.0f - const_math::clamp(fval, 0.0f, 1.0f);
				const auto alpha_val = (val > 0.25f ? 1.0f : val);
				const auto half_val = soft_f16::float_to_half(val);
				const auto half_alpha_val = soft_f16::float_to_half(alpha_val);
				pixel_data[y * texture_size.x + x] = { half_val, half_val, half_val, half_alpha_val };
			}
		}
		
		body_textures[i] = static_pointer_cast<metal_image>(ctx->create_image(dev_queue, texture_size,
																			  COMPUTE_IMAGE_TYPE::IMAGE_2D |
																			  COMPUTE_IMAGE_TYPE::RGBA16F |
																			  COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED |
																			  COMPUTE_IMAGE_TYPE::READ,
																			  &pixel_data[0],
																			  COMPUTE_MEMORY_FLAG::READ |
																			  COMPUTE_MEMORY_FLAG::HOST_WRITE |
																			  COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS));
	}
}

bool metal_renderer::init(const compute_context& ctx, const compute_queue& dev_queue, shared_ptr<compute_kernel> vs, shared_ptr<compute_kernel> fs) {
	const auto& dev = dev_queue.get_device();
	auto device = ((const metal_device&)dev).device;
	create_textures(dev_queue);
	
	const auto render_pixel_format = ((const metal_compute&)ctx).get_metal_renderer_pixel_format();

	// check vs/fs and get state
	if(!vs) {
		log_error("vertex shader not found");
		return false;
	}
	if(!fs) {
		log_error("fragment shader not found");
		return false;
	}
	
	const auto vs_entry = (const metal_kernel::metal_kernel_entry*)vs->get_kernel_entry(dev);
	if(!vs_entry) {
		log_error("no vertex shader for this device exists!");
		return false;
	}
	const auto fs_entry = (const metal_kernel::metal_kernel_entry*)fs->get_kernel_entry(dev);
	if(!fs_entry) {
		log_error("no fragment shader for this device exists!");
		return false;
	}
	
	// create/init pipeline
	MTLRenderPipelineDescriptor* pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
	pipelineStateDescriptor.label = @"nbody pipeline";
	pipelineStateDescriptor.sampleCount = 1;
	pipelineStateDescriptor.vertexFunction = (__bridge id<MTLFunction>)vs_entry->kernel;
	pipelineStateDescriptor.fragmentFunction = (__bridge id<MTLFunction>)fs_entry->kernel;
	pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
	pipelineStateDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
	pipelineStateDescriptor.colorAttachments[0].pixelFormat = render_pixel_format;
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
		log_error("failed to create pipeline state: $",
				  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
		return false;
	}
	
	//
	MTLDepthStencilDescriptor* depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
	depthStateDesc.depthCompareFunction = MTLCompareFunctionAlways;
	depthStateDesc.depthWriteEnabled = NO;
	depth_state = [device newDepthStencilStateWithDescriptor:depthStateDesc];
	
	//
	render_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
	MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc.colorAttachments[0];
	// color_attachment.texture set later
	color_attachment.loadAction = MTLLoadActionClear;
	color_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
	color_attachment.storeAction = MTLStoreActionStore;
	
	return true;
}

void metal_renderer::render(const compute_context& ctx, const compute_queue& dev_queue, shared_ptr<compute_buffer> position_buffer) {
	@autoreleasepool {
		//
		auto mtl_queue = ((const metal_queue&)dev_queue).get_queue();
		
		id <MTLCommandBuffer> cmd_buffer = [mtl_queue commandBuffer];
		cmd_buffer.label = @"nbody render";
		
		auto drawable = ((const metal_compute&)ctx).get_metal_next_drawable(cmd_buffer);
		if(drawable == nil) {
			log_error("drawable is nil!");
			return;
		}
		render_pass_desc.colorAttachments[0].texture = drawable.texture;
		
		//
		const matrix4f mproj { matrix4f().perspective(90.0f, float(floor::get_width()) / float(floor::get_height()),
													  0.25f, nbody_state.max_distance) };
		const matrix4f mview { nbody_state.cam_rotation.to_matrix4() * matrix4f().translate(0.0f, 0.0f, -nbody_state.distance) };
		const struct {
			matrix4f mvpm;
			matrix4f mvm;
			float2 mass_minmax;
		} uniforms {
			.mvpm = mview * mproj,
			.mvm = mview,
			.mass_minmax = nbody_state.mass_minmax
		};
		
		//
		id <MTLRenderCommandEncoder> renderEncoder = [cmd_buffer renderCommandEncoderWithDescriptor:render_pass_desc];
		renderEncoder.label = @"nbody RenderEncoder";
		[renderEncoder setDepthStencilState:depth_state];
		
		//
		[renderEncoder pushDebugGroup:@"bodies"];
		[renderEncoder setRenderPipelineState:pipeline_state];
		[renderEncoder setVertexBuffer:((metal_buffer*)position_buffer.get())->get_metal_buffer() offset:0 atIndex:0];
		[renderEncoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
		[renderEncoder setFragmentTexture:body_textures[0]->get_metal_image() atIndex:0];
		
		//
		[renderEncoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:nbody_state.body_count];
		
		//
		[renderEncoder endEncoding];
		[renderEncoder popDebugGroup];
		
		[cmd_buffer presentDrawable:drawable];
		[cmd_buffer commit];
	}
}

#endif
