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
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_program.hpp>
#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/darwin/darwin_helper.hpp>
#include <floor/core/timer.hpp>
#include <floor/core/file_io.hpp>
#include <libwarp/libwarp.h>

static id <MTLRenderPipelineState> render_pipeline_state_scatter;
static id <MTLRenderPipelineState> render_pipeline_state_gather;
static id <MTLRenderPipelineState> render_pipeline_state_gather_fwd;
static MTLRenderPassDescriptor* render_pass_desc_scatter { nullptr };
static MTLRenderPassDescriptor* render_pass_desc_gather { nullptr };
static MTLRenderPassDescriptor* render_pass_desc_gather_fwd { nullptr };

static id <MTLRenderPipelineState> skybox_pipeline_state_scatter;
static id <MTLRenderPipelineState> skybox_pipeline_state_gather;
static id <MTLRenderPipelineState> skybox_pipeline_state_gather_fwd;
static MTLRenderPassDescriptor* skybox_pass_desc_scatter { nullptr };
static MTLRenderPassDescriptor* skybox_pass_desc_gather { nullptr };
static MTLRenderPassDescriptor* skybox_pass_desc_gather_fwd { nullptr };

static id <MTLRenderPipelineState> shadow_pipeline_state;
static MTLRenderPassDescriptor* shadow_pass_desc { nullptr };

static id <MTLRenderPipelineState> blit_pipeline_state;
static MTLRenderPassDescriptor* blit_pass_desc { nullptr };

static id <MTLRenderPipelineState> warp_gather_pipeline_state;
static MTLRenderPassDescriptor* warp_gather_pass_desc { nullptr };

static id <MTLDepthStencilState> depth_state;
static id <MTLDepthStencilState> passthrough_depth_state;
static id <MTLDepthStencilState> skybox_depth_state;
static metal_view* view { nullptr };

void metal_renderer::create_textures(const COMPUTE_IMAGE_TYPE color_format) {
	common_renderer::create_textures(color_format);
	
	if(render_pass_desc_scatter != nil) {
		render_pass_desc_scatter.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[0].get())->get_metal_image();
		render_pass_desc_scatter.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[0].get())->get_metal_image();
		render_pass_desc_scatter.depthAttachment.texture = ((metal_image*)scene_fbo.depth[0].get())->get_metal_image();
	}
	if(skybox_pass_desc_scatter != nil) {
		skybox_pass_desc_scatter.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[0].get())->get_metal_image();
		skybox_pass_desc_scatter.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[0].get())->get_metal_image();
		skybox_pass_desc_scatter.depthAttachment.texture = ((metal_image*)scene_fbo.depth[0].get())->get_metal_image();
	}
	if(render_pass_desc_gather_fwd != nil) {
		render_pass_desc_gather_fwd.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[0].get())->get_metal_image();
		render_pass_desc_gather_fwd.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[0].get())->get_metal_image();
		render_pass_desc_gather_fwd.depthAttachment.texture = ((metal_image*)scene_fbo.depth[0].get())->get_metal_image();
	}
	if(skybox_pass_desc_gather_fwd != nil) {
		skybox_pass_desc_gather_fwd.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[0].get())->get_metal_image();
		skybox_pass_desc_gather_fwd.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[0].get())->get_metal_image();
		skybox_pass_desc_gather_fwd.depthAttachment.texture = ((metal_image*)scene_fbo.depth[0].get())->get_metal_image();
	}
}

bool metal_renderer::init() {
	if(!common_renderer::init()) {
		return false;
	}
	
	auto device = ((const metal_device&)*warp_state.dev).device;
	
	const auto get_shader_entry = [this](const WARP_SHADER& shader) {
		return (__bridge id<MTLFunction>)((const metal_kernel::metal_kernel_entry*)shader_entries[shader])->kernel;
	};
	
	// scene renderer setup
	{
		// scatter
		{
			MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			pipeline_desc.label = @"warp scene pipeline (scatter)";
			pipeline_desc.sampleCount = 1;
			pipeline_desc.vertexFunction = get_shader_entry(SCENE_SCATTER_VS);
			pipeline_desc.fragmentFunction = get_shader_entry(SCENE_SCATTER_FS);
			pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
			pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
			pipeline_desc.colorAttachments[0].blendingEnabled = false;
			pipeline_desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[1].blendingEnabled = false;
			
			NSError* error = nullptr;
			render_pipeline_state_scatter = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
			if(!render_pipeline_state_scatter) {
				log_error("failed to create (scatter) scene pipeline state: %s",
						  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
				return false;
			}
		}
		// gather
		{
			MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			pipeline_desc.label = @"warp scene pipeline (gather)";
			pipeline_desc.sampleCount = 1;
			pipeline_desc.vertexFunction = get_shader_entry(SCENE_GATHER_VS);
			pipeline_desc.fragmentFunction = get_shader_entry(SCENE_GATHER_FS);
			pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
			pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
			pipeline_desc.colorAttachments[0].blendingEnabled = false;
			pipeline_desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[1].blendingEnabled = false;
			pipeline_desc.colorAttachments[2].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[2].blendingEnabled = false;
			pipeline_desc.colorAttachments[3].pixelFormat = MTLPixelFormatRG16Float;
			pipeline_desc.colorAttachments[3].blendingEnabled = false;
			
			NSError* error = nullptr;
			render_pipeline_state_gather = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
			if(!render_pipeline_state_gather) {
				log_error("failed to create (gather) scene pipeline state: %s",
						  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
				return false;
			}
		}
		// gather forward-only
		{
			MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			pipeline_desc.label = @"warp scene pipeline (gather forward-only)";
			pipeline_desc.sampleCount = 1;
			pipeline_desc.vertexFunction = get_shader_entry(SCENE_GATHER_FWD_VS);
			pipeline_desc.fragmentFunction = get_shader_entry(SCENE_GATHER_FWD_FS);
			pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
			pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
			pipeline_desc.colorAttachments[0].blendingEnabled = false;
			pipeline_desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[1].blendingEnabled = false;
			
			NSError* error = nullptr;
			render_pipeline_state_gather_fwd = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
			if(!render_pipeline_state_gather_fwd) {
				log_error("failed to create (gather forward-only) scene pipeline state: %s",
						  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
				return false;
			}
		}
		
		// scatter pass desc
		{
			render_pass_desc_scatter = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc_scatter.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionClear;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_attachment = render_pass_desc_scatter.colorAttachments[1];
			motion_attachment.loadAction = MTLLoadActionClear;
			motion_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = render_pass_desc_scatter.depthAttachment;
			depth_attachment.loadAction = MTLLoadActionClear;
			depth_attachment.clearDepth = 1.0;
			depth_attachment.storeAction = MTLStoreActionStore;
		}
		
		// gather pass desc
		{
			render_pass_desc_gather = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc_gather.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionClear;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_fwd_attachment = render_pass_desc_gather.colorAttachments[1];
			motion_fwd_attachment.loadAction = MTLLoadActionClear;
			motion_fwd_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_fwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_bwd_attachment = render_pass_desc_gather.colorAttachments[2];
			motion_bwd_attachment.loadAction = MTLLoadActionClear;
			motion_bwd_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_bwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_depth_attachment = render_pass_desc_gather.colorAttachments[3];
			motion_depth_attachment.loadAction = MTLLoadActionClear;
			motion_depth_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_depth_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = render_pass_desc_gather.depthAttachment;
			depth_attachment.loadAction = MTLLoadActionClear;
			depth_attachment.clearDepth = 1.0;
			depth_attachment.storeAction = MTLStoreActionStore;
		}
		
		// gather forward-only pass desc
		{
			render_pass_desc_gather_fwd = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc_gather_fwd.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionClear;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_attachment = render_pass_desc_gather_fwd.colorAttachments[1];
			motion_attachment.loadAction = MTLLoadActionClear;
			motion_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = render_pass_desc_gather_fwd.depthAttachment;
			depth_attachment.loadAction = MTLLoadActionClear;
			depth_attachment.clearDepth = 1.0;
			depth_attachment.storeAction = MTLStoreActionStore;
		}
	}
	
	// sky box renderer setup
	{
		// scatter
		{
			MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			pipeline_desc.label = @"warp skybox pipeline (scatter)";
			pipeline_desc.sampleCount = 1;
			pipeline_desc.vertexFunction = get_shader_entry(SKYBOX_SCATTER_VS);
			pipeline_desc.fragmentFunction = get_shader_entry(SKYBOX_SCATTER_FS);
			pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
			pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
			pipeline_desc.colorAttachments[0].blendingEnabled = false;
			pipeline_desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[1].blendingEnabled = false;
			
			NSError* error = nullptr;
			skybox_pipeline_state_scatter = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
			if(!skybox_pipeline_state_scatter) {
				log_error("failed to create (scatter) skybox pipeline state: %s",
						  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
				return false;
			}
		}
		// gather
		{
			MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			pipeline_desc.label = @"warp skybox pipeline (gather)";
			pipeline_desc.sampleCount = 1;
			pipeline_desc.vertexFunction = get_shader_entry(SKYBOX_GATHER_VS);
			pipeline_desc.fragmentFunction = get_shader_entry(SKYBOX_GATHER_FS);
			pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
			pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
			pipeline_desc.colorAttachments[0].blendingEnabled = false;
			pipeline_desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[1].blendingEnabled = false;
			pipeline_desc.colorAttachments[2].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[2].blendingEnabled = false;
			pipeline_desc.colorAttachments[3].pixelFormat = MTLPixelFormatRG16Float;
			pipeline_desc.colorAttachments[3].blendingEnabled = false;
			
			NSError* error = nullptr;
			skybox_pipeline_state_gather = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
			if(!skybox_pipeline_state_gather) {
				log_error("failed to create (gather) skybox pipeline state: %s",
						  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
				return false;
			}
		}
		// gather forward-only
		{
			MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			pipeline_desc.label = @"warp skybox pipeline (gather forward-only)";
			pipeline_desc.sampleCount = 1;
			pipeline_desc.vertexFunction = get_shader_entry(SKYBOX_GATHER_FWD_VS);
			pipeline_desc.fragmentFunction = get_shader_entry(SKYBOX_GATHER_FWD_FS);
			pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
			pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
			pipeline_desc.colorAttachments[0].blendingEnabled = false;
			pipeline_desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
			pipeline_desc.colorAttachments[1].blendingEnabled = false;
			
			NSError* error = nullptr;
			skybox_pipeline_state_gather_fwd = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
			if(!skybox_pipeline_state_gather_fwd) {
				log_error("failed to create (gather forward-only) skybox pipeline state: %s",
						  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
				return false;
			}
		}
		
		// scatter pass desc
		{
			skybox_pass_desc_scatter = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = skybox_pass_desc_scatter.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionLoad;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_attachment = skybox_pass_desc_scatter.colorAttachments[1];
			motion_attachment.loadAction = MTLLoadActionLoad;
			motion_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = skybox_pass_desc_scatter.depthAttachment;
			depth_attachment.loadAction = MTLLoadActionLoad;
			depth_attachment.clearDepth = 1.0;
			depth_attachment.storeAction = MTLStoreActionStore;
		}
		
		// gather pass desc
		{
			skybox_pass_desc_gather = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = skybox_pass_desc_gather.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionLoad;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_fwd_attachment = skybox_pass_desc_gather.colorAttachments[1];
			motion_fwd_attachment.loadAction = MTLLoadActionLoad;
			motion_fwd_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_fwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_bwd_attachment = skybox_pass_desc_gather.colorAttachments[2];
			motion_bwd_attachment.loadAction = MTLLoadActionLoad;
			motion_bwd_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_bwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_depth_attachment = render_pass_desc_gather.colorAttachments[3];
			motion_depth_attachment.loadAction = MTLLoadActionLoad;
			motion_depth_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_depth_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = skybox_pass_desc_gather.depthAttachment;
			depth_attachment.loadAction = MTLLoadActionLoad;
			depth_attachment.clearDepth = 1.0;
			depth_attachment.storeAction = MTLStoreActionStore;
		}
		
		// gather forward-only pass desc
		{
			skybox_pass_desc_gather_fwd = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = skybox_pass_desc_gather_fwd.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionLoad;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_attachment = skybox_pass_desc_gather_fwd.colorAttachments[1];
			motion_attachment.loadAction = MTLLoadActionLoad;
			motion_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			motion_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = skybox_pass_desc_gather_fwd.depthAttachment;
			depth_attachment.loadAction = MTLLoadActionLoad;
			depth_attachment.clearDepth = 1.0;
			depth_attachment.storeAction = MTLStoreActionStore;
		}
	}
	
	// creates fbo textures/images and sets attachment textures of render_pass_desc_scatter + skybox_pass_desc_scatter (gather changes at runtime)
	create_textures();
	
	// shadow renderer setup
	{
		MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
		pipeline_desc.label = @"warp shadow pipeline";
		pipeline_desc.vertexFunction = get_shader_entry(SHADOW_VS);
		pipeline_desc.fragmentFunction = nil; // only rendering z/depth
		pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
		
		NSError* error = nullptr;
		shadow_pipeline_state = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
		if(!shadow_pipeline_state) {
			log_error("failed to create shadow pipeline state: %s",
					  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
			return false;
		}
		
		//
		shadow_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
		
		MTLRenderPassDepthAttachmentDescriptor* depth_attachment = shadow_pass_desc.depthAttachment;
		depth_attachment.loadAction = MTLLoadActionClear;
		depth_attachment.clearDepth = 1.0;
		depth_attachment.storeAction = MTLStoreActionStore;
		depth_attachment.texture = ((metal_image*)shadow_map.shadow_image.get())->get_metal_image();
		[((metal_image*)shadow_map.shadow_image.get())->get_metal_image() setLabel:@"shadow map"];
	}
	
	// blit setup
	{
		MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
		pipeline_desc.label = @"warp blit pipeline";
		pipeline_desc.sampleCount = 1;
		pipeline_desc.vertexFunction = get_shader_entry(BLIT_VS);
		pipeline_desc.fragmentFunction = get_shader_entry(BLIT_FS);
		pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
		pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
		pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		pipeline_desc.colorAttachments[0].blendingEnabled = false;
		
		NSError* error = nullptr;
		blit_pipeline_state = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
		if(!blit_pipeline_state) {
			log_error("failed to create blit pipeline state: %s",
					  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
			return false;
		}
		
		//
		blit_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
		MTLRenderPassColorAttachmentDescriptor* color_attachment = blit_pass_desc.colorAttachments[0];
		color_attachment.loadAction = MTLLoadActionDontCare; // don't care b/c drawing the whole screen
		color_attachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
		color_attachment.storeAction = MTLStoreActionStore;
		color_attachment.texture = nil;
	}
	
	// used by scene, shadow and skybox renderer
	MTLDepthStencilDescriptor* depth_state_desc = [[MTLDepthStencilDescriptor alloc] init];
	depth_state_desc.depthCompareFunction = MTLCompareFunctionLess;
	depth_state_desc.depthWriteEnabled = YES;
	depth_state = [device newDepthStencilStateWithDescriptor:depth_state_desc];
	
	depth_state_desc.depthCompareFunction = MTLCompareFunctionAlways;
	depth_state_desc.depthWriteEnabled = NO;
	passthrough_depth_state = [device newDepthStencilStateWithDescriptor:depth_state_desc];
	
	depth_state_desc.depthCompareFunction = MTLCompareFunctionLessEqual;
	depth_state_desc.depthWriteEnabled = YES;
	skybox_depth_state = [device newDepthStencilStateWithDescriptor:depth_state_desc];
	
	// since sdl doesn't have metal support (yet), we need to create a metal view ourselves
	view = darwin_helper::create_metal_view(floor::get_window(), device);
	if(view == nullptr) {
		log_error("failed to create metal view!");
		return false;
	}
	
	return true;
}

void metal_renderer::render(const floor_obj_model& model,
							const camera& cam) {
	@autoreleasepool {
		auto mtl_queue = ((metal_queue*)warp_state.dev_queue.get())->get_queue();
		id <MTLCommandBuffer> cmd_buffer = [mtl_queue commandBuffer];
		cmd_buffer.label = @"warp render";
		render_cmd_buffer = (__bridge void*)cmd_buffer;
		
		// blit to window
		auto drawable = darwin_helper::get_metal_next_drawable(view, cmd_buffer);
		if(drawable == nil) {
			log_error("drawable is nil!");
			return;
		}
		
		common_renderer::render(model, cam);
		
		// NOTE: can't use a normal blit encoder step here, because the ca drawable texture apparently isn't allowed to be used like that
		{
			blit_pass_desc.colorAttachments[0].texture = drawable.texture;
			
			id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:blit_pass_desc];
			encoder.label = @"warp blit encoder";
			[encoder setDepthStencilState:passthrough_depth_state];
			[encoder setCullMode:MTLCullModeBack];
			[encoder setFrontFacingWinding:MTLWindingClockwise];
			
			//
			[encoder pushDebugGroup:@"blit"];
			[encoder setRenderPipelineState:blit_pipeline_state];
			[encoder setFragmentTexture:(blit_frame ?
										 // if gather: this is the previous frame (i.e. if we are at time t and have just rendered I_t, this blits I_t-1)
										 ((metal_image*)scene_fbo.color[warp_state.cur_fbo].get())->get_metal_image() :
										 ((metal_image*)scene_fbo.compute_color.get())->get_metal_image())
								atIndex:0];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; // fullscreen triangle
			[encoder endEncoding];
			[encoder popDebugGroup];
		}
		
		[cmd_buffer presentDrawable:drawable];
		[cmd_buffer commit];
	}
}

void metal_renderer::render_full_scene(const floor_obj_model& model, const camera& cam) {
	// this updates all uniforms
	common_renderer::render_full_scene(model, cam);
	// updated above, still need the actual here
	const auto cur_fbo = 1 - warp_state.cur_fbo;
	
	auto cmd_buffer = (__bridge id<MTLCommandBuffer>)render_cmd_buffer;
	
	//////////////////////////////////////////
	// draw shadow map
	
	{
		id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:shadow_pass_desc];
		encoder.label = @"warp shadow encoder";
		[encoder setDepthStencilState:depth_state];
		[encoder setCullMode:MTLCullModeBack];
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		
		//
		[encoder pushDebugGroup:@"shadow"];
		[encoder setRenderPipelineState:shadow_pipeline_state];
		[encoder setVertexBuffer:((metal_buffer*)model.vertices_buffer.get())->get_metal_buffer() offset:0 atIndex:0];
		[encoder setVertexBytes:&light_mvpm length:sizeof(matrix4f) atIndex:1];
		
		for(const auto& obj : model.objects) {
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
								indexCount:uint32_t(obj->index_count)
								 indexType:MTLIndexTypeUInt32
							   indexBuffer:((metal_buffer*)obj->indices_floor_vbo.get())->get_metal_buffer()
						 indexBufferOffset:0];
		}
		
		//
		[encoder endEncoding];
		[encoder popDebugGroup];
	}
	
	//////////////////////////////////////////
	// render actual scene
	
	{
		// for bidirectional gather rendering, this switches every frame
		if(!warp_state.is_scatter && !warp_state.is_gather_forward) {
			render_pass_desc_gather.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[cur_fbo].get())->get_metal_image();
			render_pass_desc_gather.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[cur_fbo * 2].get())->get_metal_image();
			render_pass_desc_gather.colorAttachments[2].texture = ((metal_image*)scene_fbo.motion[cur_fbo * 2 + 1].get())->get_metal_image();
			render_pass_desc_gather.colorAttachments[3].texture = ((metal_image*)scene_fbo.motion_depth[cur_fbo].get())->get_metal_image();
			render_pass_desc_gather.depthAttachment.texture = ((metal_image*)scene_fbo.depth[cur_fbo].get())->get_metal_image();
		}
		
		//
		id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:(warp_state.is_scatter ?
																							   render_pass_desc_scatter :
																							   (warp_state.is_gather_forward ?
																								render_pass_desc_gather_fwd :
																								render_pass_desc_gather))];
		encoder.label = @"warp scene encoder";
		[encoder setDepthStencilState:depth_state];
		[encoder setCullMode:MTLCullModeBack];
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		
		//
		[encoder pushDebugGroup:@"scene"];
		if(warp_state.is_scatter) {
			[encoder setRenderPipelineState:render_pipeline_state_scatter];
		}
		else if(warp_state.is_gather_forward) {
			[encoder setRenderPipelineState:render_pipeline_state_gather_fwd];
		}
		else {
			[encoder setRenderPipelineState:render_pipeline_state_gather];
		}
		[encoder setVertexBuffer:((metal_buffer*)model.vertices_buffer.get())->get_metal_buffer() offset:0 atIndex:0];
		[encoder setVertexBuffer:((metal_buffer*)model.tex_coords_buffer.get())->get_metal_buffer() offset:0 atIndex:1];
		[encoder setVertexBuffer:((metal_buffer*)model.normals_buffer.get())->get_metal_buffer() offset:0 atIndex:2];
		[encoder setVertexBuffer:((metal_buffer*)model.binormals_buffer.get())->get_metal_buffer() offset:0 atIndex:3];
		[encoder setVertexBuffer:((metal_buffer*)model.tangents_buffer.get())->get_metal_buffer() offset:0 atIndex:4];
		[encoder setVertexBuffer:((metal_buffer*)model.materials_buffer.get())->get_metal_buffer() offset:0 atIndex:5];
		[encoder setVertexBytes:&scene_uniforms length:sizeof(scene_uniforms) atIndex:6];
		
		[encoder setFragmentTexture:((metal_image*)shadow_map.shadow_image.get())->get_metal_image() atIndex:4];
		for(const auto& obj : model.objects) {
			[encoder setFragmentTexture:((metal_image*)model.diffuse_textures[obj->mat_idx])->get_metal_image() atIndex:0];
			[encoder setFragmentTexture:((metal_image*)model.specular_textures[obj->mat_idx])->get_metal_image() atIndex:1];
			[encoder setFragmentTexture:((metal_image*)model.normal_textures[obj->mat_idx])->get_metal_image() atIndex:2];
			[encoder setFragmentTexture:((metal_image*)model.mask_textures[obj->mat_idx])->get_metal_image() atIndex:3];
			
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
								indexCount:uint32_t(obj->index_count)
								 indexType:MTLIndexTypeUInt32
							   indexBuffer:((metal_buffer*)obj->indices_floor_vbo.get())->get_metal_buffer()
						 indexBufferOffset:0];
		}
		
		//
		[encoder endEncoding];
		[encoder popDebugGroup];
	}
	
	//////////////////////////////////////////
	// render sky box
	
	{
		// for bidirectional gather rendering, this switches every frame
		if(!warp_state.is_scatter && !warp_state.is_gather_forward) {
			skybox_pass_desc_gather.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[cur_fbo].get())->get_metal_image();
			skybox_pass_desc_gather.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[cur_fbo * 2].get())->get_metal_image();
			skybox_pass_desc_gather.colorAttachments[2].texture = ((metal_image*)scene_fbo.motion[cur_fbo * 2 + 1].get())->get_metal_image();
			skybox_pass_desc_gather.colorAttachments[3].texture = ((metal_image*)scene_fbo.motion_depth[cur_fbo].get())->get_metal_image();
			skybox_pass_desc_gather.depthAttachment.texture = ((metal_image*)scene_fbo.depth[cur_fbo].get())->get_metal_image();
		}
		
		//
		id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:(warp_state.is_scatter ?
																							   skybox_pass_desc_scatter :
																							   (warp_state.is_gather_forward ?
																								skybox_pass_desc_gather_fwd :
																								skybox_pass_desc_gather))];
		encoder.label = @"warp sky box encoder";
		[encoder setDepthStencilState:skybox_depth_state];
		[encoder setCullMode:MTLCullModeBack];
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		
		//
		[encoder pushDebugGroup:@"sky box"];
		if(warp_state.is_scatter) {
			[encoder setRenderPipelineState:skybox_pipeline_state_scatter];
		}
		else if(warp_state.is_gather_forward) {
			[encoder setRenderPipelineState:skybox_pipeline_state_gather_fwd];
		}
		else {
			[encoder setRenderPipelineState:skybox_pipeline_state_gather];
		}
		[encoder setVertexBytes:&skybox_uniforms length:sizeof(skybox_uniforms) atIndex:0];
		
		[encoder setFragmentTexture:((metal_image*)skybox_tex.get())->get_metal_image() atIndex:0];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
		
		//
		[encoder endEncoding];
		[encoder popDebugGroup];
	}
}

bool metal_renderer::compile_shaders(const string add_cli_options) {
	return common_renderer::compile_shaders(add_cli_options +
											// a total hack until I implement run-time samplers (samplers are otherwise clamp-to-edge)
											" -DFLOOR_METAL_ADDRESS_MODE=metal_image::sampler::ADDRESS_MODE::REPEAT");
}

#endif
