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
#include "hlbvh_state.hpp"
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

static metal_view* view { nullptr };
static MTLRenderPassDescriptor* render_pass_desc { nullptr };

static struct {
	shared_ptr<compute_image> depth;
	
	uint2 dim;
} scene_fbo;


void metal_renderer::destroy() {
	scene_fbo.depth = nullptr;
}

static void create_textures() {
	scene_fbo.dim = floor::get_physical_screen_size();
	
	scene_fbo.depth = hlbvh_state.ctx->create_image(*hlbvh_state.dev_queue, scene_fbo.dim,
													COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
													COMPUTE_IMAGE_TYPE::D32F |
													COMPUTE_IMAGE_TYPE::READ_WRITE |
													COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
													COMPUTE_MEMORY_FLAG::READ_WRITE);
	
	if(render_pass_desc != nil) {
		render_pass_desc.depthAttachment.texture = ((metal_image*)scene_fbo.depth.get())->get_metal_image();
	}
}

bool metal_renderer::init(shared_ptr<compute_kernel> vs,
						  shared_ptr<compute_kernel> fs) {
	auto device = ((const metal_device&)*hlbvh_state.dev).device;
	
	// check vs/fs and get state
	if(!vs) {
		log_error("vertex shader not found");
		return false;
	}
	if(!fs) {
		log_error("fragment shader not found");
		return false;
	}
	
	// since sdl doesn't have metal support (yet), we need to create a metal view ourselves
	view = darwin_helper::create_metal_view(floor::get_window(), device, {});
	if(view == nullptr) {
		log_error("failed to create metal view!");
		return false;
	}
	
	//
	const auto vs_entry = (const metal_kernel::metal_kernel_entry*)vs->get_kernel_entry(*hlbvh_state.dev);
	if(!vs_entry) {
		log_error("no vertex shader for this device exists!");
		return false;
	}
	const auto fs_entry = (const metal_kernel::metal_kernel_entry*)fs->get_kernel_entry(*hlbvh_state.dev);
	if(!fs_entry) {
		log_error("no fragment shader for this device exists!");
		return false;
	}
	
	//
	MTLRenderPipelineDescriptor* pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
	pipelineStateDescriptor.label = @"hlbvh pipeline";
	pipelineStateDescriptor.sampleCount = 1;
	pipelineStateDescriptor.vertexFunction = (__bridge id<MTLFunction>)vs_entry->kernel;
	pipelineStateDescriptor.fragmentFunction = (__bridge id<MTLFunction>)fs_entry->kernel;
	pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
	pipelineStateDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
	pipelineStateDescriptor.colorAttachments[0].pixelFormat = darwin_helper::get_metal_pixel_format(view);
	pipelineStateDescriptor.colorAttachments[0].blendingEnabled = false;
	
	NSError* error = nullptr;
	pipeline_state = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
	if(!pipeline_state) {
		log_error("failed to create pipeline state: $",
				  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
		return false;
	}
	
	//
	MTLDepthStencilDescriptor* depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
	depthStateDesc.depthCompareFunction = MTLCompareFunctionLess;
	depthStateDesc.depthWriteEnabled = YES;
	depth_state = [device newDepthStencilStateWithDescriptor:depthStateDesc];
	
	//
	render_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
	MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc.colorAttachments[0];
	// color_attachment.texture set later
	color_attachment.loadAction = MTLLoadActionClear;
	color_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
	color_attachment.storeAction = MTLStoreActionDontCare;
	
	MTLRenderPassDepthAttachmentDescriptor* depth_attachment = render_pass_desc.depthAttachment;
	depth_attachment.loadAction = MTLLoadActionClear;
	depth_attachment.clearDepth = 1.0;
	depth_attachment.storeAction = MTLStoreActionDontCare;
	
	//
	create_textures();
	
	return true;
}

void metal_renderer::render(const vector<unique_ptr<animation>>& models,
							const vector<uint32_t>& collisions,
							const bool cam_mode,
							const camera& cam) {
	@autoreleasepool {
		//
		auto mtl_queue = ((metal_queue*)hlbvh_state.dev_queue.get())->get_queue();
		
		id <MTLCommandBuffer> cmd_buffer = [mtl_queue commandBuffer];
		cmd_buffer.label = @"hlbvh render";
		
		auto drawable = darwin_helper::get_metal_next_drawable(view, cmd_buffer);
		if(drawable == nil) {
			log_error("drawable is nil!");
			return;
		}
		render_pass_desc.colorAttachments[0].texture = drawable.texture;
		
		//
		const matrix4f mproj {
			matrix4f().perspective(72.0f, float(floor::get_width()) / float(floor::get_height()),
								   0.25f, hlbvh_state.max_distance)
		};
		matrix4f mview;
		if(!cam_mode) {
			mview = hlbvh_state.cam_rotation.to_matrix4() * matrix4f().translate(0.0f, 0.0f, -hlbvh_state.distance);
		}
		else {
			const auto rmvm = (matrix4f::rotation_deg_named<'y'>(cam.get_rotation().y) *
							   matrix4f::rotation_deg_named<'x'>(cam.get_rotation().x));
			mview = matrix4f::translation(cam.get_position() * float3 { 1.0f, -1.0f, 1.0f }) * rmvm;
		}
		struct __attribute__((packed)) {
			matrix4f mvpm;
			float4 repl_color;
			float4 default_color;
			float3 light_dir;
		} uniforms {
			.mvpm = mview * mproj,
			.repl_color = {},
			.default_color = {},
			.light_dir = { 1.0f, 0.0f, 0.0f },
		};
		static_assert(sizeof(uniforms) == 108, "invalid uniforms size");
		
		//
		id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:render_pass_desc];
		encoder.label = @"hlbvh encoder";
		[encoder setRenderPipelineState:pipeline_state];
		[encoder setDepthStencilState:depth_state];
		[encoder setCullMode:MTLCullModeBack];
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		
		//
		[encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
		for(uint32_t i = 0; i < (uint32_t)models.size(); ++i) {
			const auto& mdl = models[i];
			const auto cur_frame = (const floor_obj_model*)mdl->frames[mdl->cur_frame].get();
			const auto next_frame = (const floor_obj_model*)mdl->frames[mdl->next_frame].get();
			
			static constexpr const float4 default_color { 0.9f, 0.9f, 0.9f, 1.0f };
			static constexpr const float4 collision_color { 1.0f, 0.0f, 0.0f, 1.0f };
			if(!hlbvh_state.triangle_vis) {
				uniforms.default_color = (collisions[i] == 0 ? default_color : collision_color);
			}
			else {
				uniforms.default_color = default_color;
			}
			
			[encoder pushDebugGroup:@"model"];
			[encoder setVertexBuffer:((metal_buffer*)cur_frame->vertices_buffer.get())->get_metal_buffer() offset:0 atIndex:0];
			[encoder setVertexBuffer:((metal_buffer*)next_frame->vertices_buffer.get())->get_metal_buffer() offset:0 atIndex:1];
			[encoder setVertexBuffer:((metal_buffer*)cur_frame->normals_buffer.get())->get_metal_buffer() offset:0 atIndex:2];
			[encoder setVertexBuffer:((metal_buffer*)next_frame->normals_buffer.get())->get_metal_buffer() offset:0 atIndex:3];
			[encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:4];
			[encoder setVertexBytes:&mdl->step length:sizeof(float) atIndex:5];
			if(hlbvh_state.triangle_vis) {
				[encoder setVertexBuffer:((metal_buffer*)mdl->colliding_vertices.get())->get_metal_buffer() offset:0 atIndex:6];
			}
			
			for(const auto& obj : cur_frame->objects) {
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
									indexCount:uint32_t(obj->index_count)
									 indexType:MTLIndexTypeUInt32
								   indexBuffer:((metal_buffer*)obj->indices_floor_vbo.get())->get_metal_buffer()
							 indexBufferOffset:0];
			}
			[encoder popDebugGroup];
		}
		
		//
		[encoder endEncoding];
		
		[cmd_buffer presentDrawable:drawable];
		[cmd_buffer commit];
	}
}

#endif
