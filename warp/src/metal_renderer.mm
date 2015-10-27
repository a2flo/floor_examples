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
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_program.hpp>
#include <floor/darwin/darwin_helper.hpp>
#include <floor/core/timer.hpp>
#include <floor/core/file_io.hpp>

static id <MTLLibrary> metal_shd_lib;
struct metal_shader_object {
	id <MTLFunction> vertex_program;
	id <MTLFunction> fragment_program;
};
static unordered_map<string, shared_ptr<metal_shader_object>> shader_objects;

static id <MTLRenderPipelineState> render_pipeline_state_scatter;
static id <MTLRenderPipelineState> render_pipeline_state_gather;
static MTLRenderPassDescriptor* render_pass_desc_scatter { nullptr };
static MTLRenderPassDescriptor* render_pass_desc_gather { nullptr };

static id <MTLRenderPipelineState> skybox_pipeline_state_scatter;
static id <MTLRenderPipelineState> skybox_pipeline_state_gather;
static MTLRenderPassDescriptor* skybox_pass_desc_scatter { nullptr };
static MTLRenderPassDescriptor* skybox_pass_desc_gather { nullptr };

static id <MTLRenderPipelineState> shadow_pipeline_state;
static MTLRenderPassDescriptor* shadow_pass_desc { nullptr };

static id <MTLRenderPipelineState> blit_pipeline_state;
static MTLRenderPassDescriptor* blit_pass_desc { nullptr };

static id <MTLRenderPipelineState> warp_gather_pipeline_state;
static MTLRenderPassDescriptor* warp_gather_pass_desc { nullptr };

static id <MTLDepthStencilState> depth_state;
static id <MTLDepthStencilState> passthrough_depth_state;
static id <MTLDepthStencilState> skybox_depth_state;
static id <MTLSamplerState> sampler_state;
static metal_view* view { nullptr };

static struct {
	shared_ptr<compute_image> color[2];
	shared_ptr<compute_image> depth[2];
	shared_ptr<compute_image> motion[4];
	shared_ptr<compute_image> motion_depth[2];
	shared_ptr<compute_image> compute_color;
	
	uint2 dim;
	uint2 dim_multiple;
} scene_fbo;
static struct {
	shared_ptr<compute_image> shadow_image;
	uint2 dim { 2048 };
} shadow_map;
static shared_ptr<compute_image> skybox_tex;
static float3 light_pos;
static matrix4f prev_mvm, prev_prev_mvm;
static matrix4f prev_rmvm, prev_prev_rmvm;
static constexpr const float4 clear_color { 0.215f, 0.412f, 0.6f, 0.0f };
static bool first_frame { true };

static void destroy_textures(bool is_resize) {
	scene_fbo.compute_color = nullptr;
	for(auto& img : scene_fbo.color) {
		img = nullptr;
	}
	for(auto& img : scene_fbo.depth) {
		img = nullptr;
	}
	for(auto& img : scene_fbo.motion) {
		img = nullptr;
	}
	for(auto& img : scene_fbo.motion_depth) {
		img = nullptr;
	}
	
	// only need to destroy this on exit (not on resize!)
	if(!is_resize) shadow_map.shadow_image = nullptr;
}

static void create_textures() {
	scene_fbo.dim = floor::get_physical_screen_size();
	
	// kernel work-group size is { 32, 16 } -> round global size to a multiple of it
	scene_fbo.dim_multiple = scene_fbo.dim.rounded_next_multiple(warp_state.tile_size);
	
	for(size_t i = 0, count = size(scene_fbo.color); i < count; ++i) {
		scene_fbo.color[i] = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
													   COMPUTE_IMAGE_TYPE::IMAGE_2D |
													   COMPUTE_IMAGE_TYPE::BGRA8UI_NORM |
													   COMPUTE_IMAGE_TYPE::READ_WRITE |
													   COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
													   COMPUTE_MEMORY_FLAG::READ_WRITE);
		
		scene_fbo.depth[i] = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
														  COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
														  COMPUTE_IMAGE_TYPE::D32F |
														  COMPUTE_IMAGE_TYPE::READ |
														  COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
														  COMPUTE_MEMORY_FLAG::READ);
		
		for(size_t j = 0; j < 2; ++j) {
			scene_fbo.motion[i * 2 + j] = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
																	   COMPUTE_IMAGE_TYPE::IMAGE_2D |
																	   COMPUTE_IMAGE_TYPE::R32UI |
																	   COMPUTE_IMAGE_TYPE::READ_WRITE |
																	   COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
																	   COMPUTE_MEMORY_FLAG::READ_WRITE);
		}
		
		scene_fbo.motion_depth[i] = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
																 COMPUTE_IMAGE_TYPE::IMAGE_2D |
																 COMPUTE_IMAGE_TYPE::RG16F |
																 COMPUTE_IMAGE_TYPE::READ_WRITE |
																 COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
																 COMPUTE_MEMORY_FLAG::READ_WRITE);
	}
	
	scene_fbo.compute_color = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
														   COMPUTE_IMAGE_TYPE::IMAGE_2D |
														   COMPUTE_IMAGE_TYPE::BGRA8UI_NORM |
														   COMPUTE_IMAGE_TYPE::READ_WRITE,
														   COMPUTE_MEMORY_FLAG::READ_WRITE);
	
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
	
	// create appropriately sized s/w depth buffer
	warp_state.scatter_depth_buffer = warp_state.ctx->create_buffer(sizeof(float) *
																	size_t(scene_fbo.dim.x) * size_t(scene_fbo.dim.y));
}

static void create_skybox() {
	static const char* skybox_filenames[] {
		"skybox/posx.png",
		"skybox/negx.png",
		"skybox/posy.png",
		"skybox/negy.png",
		"skybox/posz.png",
		"skybox/negz.png",
	};
	
	array<SDL_Surface*, size(skybox_filenames)> skybox_surfaces;
	auto surf_iter = begin(skybox_surfaces);
	for(const auto& filename : skybox_filenames) {
		const auto tex = obj_loader::load_texture(floor::data_path(filename));
		if(!tex.first) {
			log_error("failed to load sky box");
			return;
		}
		*surf_iter++ = tex.second.surface;
	}
	
	//
	COMPUTE_IMAGE_TYPE image_type = obj_loader::floor_image_type_format(skybox_surfaces[0]);
	image_type |= (COMPUTE_IMAGE_TYPE::IMAGE_CUBE |
				   COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED |
				   COMPUTE_IMAGE_TYPE::READ);
	
	// need to copy the data to a continuous array
	const uint2 skybox_dim { uint32_t(skybox_surfaces[0]->w), uint32_t(skybox_surfaces[0]->h) };
	const auto skybox_size = image_data_size_from_types(skybox_dim, image_type);
	const auto slice_size = image_slice_data_size_from_types(skybox_dim, image_type);
	auto skybox_pixels = make_unique<uint8_t[]>(skybox_size);
	for(size_t i = 0, count = size(skybox_surfaces); i < count; ++i) {
		memcpy(skybox_pixels.get() + i * slice_size, skybox_surfaces[i]->pixels, slice_size);
	}
	
	skybox_tex = warp_state.ctx->create_image(warp_state.dev,
											  skybox_dim,
											  image_type,
											  skybox_pixels.get(),
											  COMPUTE_MEMORY_FLAG::READ |
											  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE);
}

static void destroy_skybox() {
	skybox_tex = nullptr;
}

static bool resize_handler(EVENT_TYPE type, shared_ptr<event_object>) {
	if(type == EVENT_TYPE::WINDOW_RESIZE) {
		destroy_textures(true);
		create_textures();
		first_frame = true;
		return true;
	}
	return false;
}
static event::handler resize_handler_fnctr(&resize_handler);

bool metal_renderer::init() {
	auto device = ((metal_device*)warp_state.dev.get())->device;
	if(!compile_shaders()) {
		return false;
	}
	
	floor::get_event()->add_internal_event_handler(resize_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
	
	// scene renderer setup
	{
		// scatter
		{
			MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			pipeline_desc.label = @"warp scene pipeline (scatter)";
			pipeline_desc.sampleCount = 1;
			pipeline_desc.vertexFunction = shader_objects["SCENE_SCATTER"]->vertex_program;
			pipeline_desc.fragmentFunction = shader_objects["SCENE_SCATTER"]->fragment_program;
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
			pipeline_desc.vertexFunction = shader_objects["SCENE_GATHER"]->vertex_program;
			pipeline_desc.fragmentFunction = shader_objects["SCENE_GATHER"]->fragment_program;
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
		
		// scatter pass desc
		{
			render_pass_desc_scatter = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc_scatter.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionClear;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_attachment = render_pass_desc_scatter.colorAttachments[1];
			motion_attachment.loadAction = MTLLoadActionClear;
			motion_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
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
			motion_fwd_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
			motion_fwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_bwd_attachment = render_pass_desc_gather.colorAttachments[2];
			motion_bwd_attachment.loadAction = MTLLoadActionClear;
			motion_bwd_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
			motion_bwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_depth_attachment = render_pass_desc_gather.colorAttachments[3];
			motion_depth_attachment.loadAction = MTLLoadActionClear;
			motion_depth_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
			motion_depth_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = render_pass_desc_gather.depthAttachment;
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
			pipeline_desc.vertexFunction = shader_objects["SKYBOX_SCATTER"]->vertex_program;
			pipeline_desc.fragmentFunction = shader_objects["SKYBOX_SCATTER"]->fragment_program;
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
			pipeline_desc.vertexFunction = shader_objects["SKYBOX_GATHER"]->vertex_program;
			pipeline_desc.fragmentFunction = shader_objects["SKYBOX_GATHER"]->fragment_program;
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
		
		// scatter pass desc
		{
			skybox_pass_desc_scatter = [MTLRenderPassDescriptor renderPassDescriptor];
			MTLRenderPassColorAttachmentDescriptor* color_attachment = skybox_pass_desc_scatter.colorAttachments[0];
			color_attachment.loadAction = MTLLoadActionLoad;
			color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			color_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_attachment = skybox_pass_desc_scatter.colorAttachments[1];
			motion_attachment.loadAction = MTLLoadActionLoad;
			motion_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
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
			motion_fwd_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
			motion_fwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_bwd_attachment = skybox_pass_desc_gather.colorAttachments[2];
			motion_bwd_attachment.loadAction = MTLLoadActionLoad;
			motion_bwd_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
			motion_bwd_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassColorAttachmentDescriptor* motion_depth_attachment = render_pass_desc_gather.colorAttachments[3];
			motion_depth_attachment.loadAction = MTLLoadActionLoad;
			motion_depth_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
			motion_depth_attachment.storeAction = MTLStoreActionStore;
			
			MTLRenderPassDepthAttachmentDescriptor* depth_attachment = skybox_pass_desc_gather.depthAttachment;
			depth_attachment.loadAction = MTLLoadActionLoad;
			depth_attachment.clearDepth = 1.0;
			depth_attachment.storeAction = MTLStoreActionStore;
		}
	}
	
	// creates fbo textures/images and sets attachment textures of render_pass_desc_scatter + skybox_pass_desc_scatter (gather changes at runtime)
	create_textures();
	
	// shadow renderer setup
	{
		shadow_map.shadow_image = warp_state.ctx->create_image(warp_state.dev, shadow_map.dim,
															   COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
															   COMPUTE_IMAGE_TYPE::D32F |
															   COMPUTE_IMAGE_TYPE::READ |
															   COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
															   COMPUTE_MEMORY_FLAG::READ);
		
		MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
		pipeline_desc.label = @"warp shadow pipeline";
		pipeline_desc.vertexFunction = shader_objects["SHADOW"]->vertex_program;
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
		pipeline_desc.vertexFunction = shader_objects["BLIT"]->vertex_program;
		pipeline_desc.fragmentFunction = shader_objects["BLIT"]->fragment_program;
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
	
	// warp gather setup (for testing purposes)
	{
		MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
		pipeline_desc.label = @"gather pipeline";
		pipeline_desc.sampleCount = 1;
		pipeline_desc.vertexFunction = shader_objects["WARP_GATHER"]->vertex_program;
		pipeline_desc.fragmentFunction = shader_objects["WARP_GATHER"]->fragment_program;
		pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
		pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
		pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		pipeline_desc.colorAttachments[0].blendingEnabled = false;
		
		NSError* error = nullptr;
		warp_gather_pipeline_state = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
		if(!warp_gather_pipeline_state) {
			log_error("failed to create gather pipeline state: %s",
					  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
			return false;
		}
		
		//
		warp_gather_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
		MTLRenderPassColorAttachmentDescriptor* color_attachment = warp_gather_pass_desc.colorAttachments[0];
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
	
	// need an actual sampler object, because anisotropy can't be specified for constexpr in-shader samplers
	MTLSamplerDescriptor* sampler_desc = [[MTLSamplerDescriptor alloc] init];
	sampler_desc.minFilter = MTLSamplerMinMagFilterLinear;
	sampler_desc.magFilter = MTLSamplerMinMagFilterLinear;
#if !defined(FLOOR_IOS)
	sampler_desc.mipFilter = MTLSamplerMipFilterLinear;
	sampler_desc.maxAnisotropy = 16;
#else // less quality, but quite faster
	sampler_desc.mipFilter = MTLSamplerMipFilterNearest;
	sampler_desc.maxAnisotropy = 1;
#endif
	sampler_desc.normalizedCoordinates = true;
	sampler_desc.lodMinClamp = 0.0f;
	sampler_desc.lodMaxClamp = numeric_limits<float>::max();
	sampler_desc.sAddressMode = MTLSamplerAddressModeRepeat;
	sampler_desc.tAddressMode = MTLSamplerAddressModeRepeat;
	sampler_state = [device newSamplerStateWithDescriptor:sampler_desc];
	
	// sky box
	create_skybox();
	
	return true;
}

void metal_renderer::destroy() {
	destroy_textures(false);
	destroy_skybox();
}

static void render_full_scene(const metal_obj_model& model, const camera& cam, id <MTLCommandBuffer> cmd_buffer);
static void render_kernels(const float& delta, const float& render_delta,
						   const size_t& warp_frame_num);
void metal_renderer::render(const metal_obj_model& model,
							const camera& cam) {
	// time keeping
	static const long double time_den { chrono::high_resolution_clock::time_point::duration::period::den };
	static auto time_keeper = chrono::high_resolution_clock::now();
	const auto now = chrono::high_resolution_clock::now();
	const auto delta = now - time_keeper;
	const auto deltaf = float(((long double)delta.count()) / time_den);
	static float render_delta { 0.0f };
	
	// light handling (TODO: proper light)
	{
		static const float3 light_min { -105.0f, 250.0f, 0.0f }, light_max { 105.0f, 250.0f, 0.0f };
		//static const float3 light_min { -115.0f, 300.0f, 0.0f }, light_max { 115.0f, 0.0f, 0.0f };
		static float light_interp { 0.5f }, light_interp_dir { 1.0f };
		static constexpr const float light_slow_down { 0.0f };
		
		light_interp += (deltaf * light_slow_down) * light_interp_dir;
		if(light_interp > 1.0f) {
			light_interp = 1.0f;
			light_interp_dir *= -1.0f;
		}
		else if(light_interp < 0.0f) {
			light_interp = 0.0f;
			light_interp_dir *= -1.0f;
		}
		light_pos = light_min.interpolated(light_max, light_interp);
	}
	
	//
	@autoreleasepool {
		//
		static const float frame_limit { 1.0f / float(warp_state.render_frame_count) };
		static size_t warp_frame_num = 0;
		bool blit = false;
		const bool run_warp_kernel = ((deltaf < frame_limit && !first_frame) || warp_state.is_frame_repeat);
		if(run_warp_kernel) {
#define USE_WARP_KERNELS 1
#if defined(USE_WARP_KERNELS)
			if(deltaf >= frame_limit) { // need to reset when over the limit
				render_delta = deltaf;
				time_keeper = now;
				warp_frame_num = 0;
			}
			
			if(warp_state.is_warping) {
				render_kernels(deltaf, render_delta, warp_frame_num);
				blit = false;
				++warp_frame_num;
			}
			else blit = true;
#endif
		}
		
		//
		auto mtl_queue = ((metal_queue*)warp_state.dev_queue.get())->get_queue();
		id <MTLCommandBuffer> cmd_buffer = [mtl_queue commandBuffer];
		cmd_buffer.label = @"warp render";
		if(!run_warp_kernel) {
			first_frame = false;
			render_delta = deltaf;
			time_keeper = now;
			warp_frame_num = 0;
			
			render_full_scene(model, cam, cmd_buffer);
			
			blit = warp_state.is_render_full;
		}
		
		// blit to window
		auto drawable = darwin_helper::get_metal_next_drawable(view);
		if(drawable == nil) {
			log_error("drawable is nil!");
			return;
		}
		
		// NOTE: can't use a normal blit encoder step here, because the ca drawable texture apparently isn't allowed to be used like that
#if defined(USE_WARP_KERNELS)
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
			[encoder setFragmentTexture:(blit ?
										 // if gather: this is the previous frame (i.e. if we are at time t and have just rendered I_t, this blits I_t-1)
										 ((metal_image*)scene_fbo.color[warp_state.cur_fbo].get())->get_metal_image() :
										 ((metal_image*)scene_fbo.compute_color.get())->get_metal_image())
								atIndex:0];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; // fullscreen triangle
			[encoder endEncoding];
			[encoder popDebugGroup];
		}
#else
		if(!run_warp_kernel) {
			blit_pass_desc.colorAttachments[0].texture = drawable.texture;
			
			id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:blit_pass_desc];
			encoder.label = @"warp blit encoder";
			[encoder setDepthStencilState:passthrough_depth_state];
			[encoder setCullMode:MTLCullModeBack];
			[encoder setFrontFacingWinding:MTLWindingClockwise];
			
			//
			[encoder pushDebugGroup:@"blit"];
			[encoder setRenderPipelineState:blit_pipeline_state];
			[encoder setFragmentTexture:(blit ?
										 // if gather: this is the previous frame (i.e. if we are at time t and have just rendered I_t, this blits I_t-1)
										 ((metal_image*)scene_fbo.color[warp_state.cur_fbo].get())->get_metal_image() :
										 ((metal_image*)scene_fbo.compute_color.get())->get_metal_image())
								atIndex:0];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; // fullscreen triangle
			[encoder endEncoding];
			[encoder popDebugGroup];
		}
		else {
			warp_gather_pass_desc.colorAttachments[0].texture = drawable.texture;
			
			//
			if(deltaf >= frame_limit) { // need to reset when over the limit
				render_delta = deltaf;
				time_keeper = now;
				warp_frame_num = 0;
			}
			
			//
			float relative_delta = deltaf / render_delta;
			if(warp_state.target_frame_count > 0) {
				const uint32_t in_between_frame_count = ((warp_state.target_frame_count - warp_state.render_frame_count) /
														 warp_state.render_frame_count);
				const float delta_per_frame = 1.0f / float(in_between_frame_count + 1u);
				
				// reached the limit
				if(warp_frame_num >= in_between_frame_count) {
					//return; // TODO: !
				}
				
				relative_delta = float(warp_frame_num + 1u) * delta_per_frame;
			}
			
			//
			blit_pass_desc.colorAttachments[0].texture = drawable.texture;
			
			id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:warp_gather_pass_desc];
			encoder.label = @"gather encoder";
			[encoder setDepthStencilState:passthrough_depth_state];
			[encoder setCullMode:MTLCullModeBack];
			[encoder setFrontFacingWinding:MTLWindingClockwise];
			
			//
			[encoder pushDebugGroup:@"gather"];
			[encoder setRenderPipelineState:warp_gather_pipeline_state];
			
			// current frame (t)
			[encoder setFragmentTexture:((metal_image*)scene_fbo.color[1u - warp_state.cur_fbo].get())->get_metal_image() atIndex:0];
			[encoder setFragmentTexture:((metal_image*)scene_fbo.depth[1u - warp_state.cur_fbo].get())->get_metal_image() atIndex:1];
			// previous frame (t-1)
			[encoder setFragmentTexture:((metal_image*)scene_fbo.color[warp_state.cur_fbo].get())->get_metal_image() atIndex:2];
			[encoder setFragmentTexture:((metal_image*)scene_fbo.depth[warp_state.cur_fbo].get())->get_metal_image() atIndex:3];
			// forward from prev frame, t-1 -> t
			[encoder setFragmentTexture:((metal_image*)scene_fbo.motion[warp_state.cur_fbo * 2].get())->get_metal_image() atIndex:4];
			// backward from cur frame, t -> t-1
			[encoder setFragmentTexture:((metal_image*)scene_fbo.motion[(1u - warp_state.cur_fbo) * 2 + 1].get())->get_metal_image() atIndex:5];
			// packed depth: { fwd t-1 -> t (used here), bwd t-1 -> t-2 (unused here) }
			[encoder setFragmentTexture:((metal_image*)scene_fbo.motion_depth[warp_state.cur_fbo].get())->get_metal_image() atIndex:6];
			// packed depth: { fwd t+1 -> t (unused here), bwd t -> t-1 (used here) }
			[encoder setFragmentTexture:((metal_image*)scene_fbo.motion_depth[1u - warp_state.cur_fbo].get())->get_metal_image() atIndex:7];
			
			[encoder setFragmentBytes:&relative_delta length:sizeof(float) atIndex:0];
			[encoder setFragmentBytes:&warp_state.gather_eps_1 length:sizeof(float) atIndex:1];
			[encoder setFragmentBytes:&warp_state.gather_eps_2 length:sizeof(float) atIndex:2];
			
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; // fullscreen triangle
			[encoder endEncoding];
			[encoder popDebugGroup];
			
			//
			++warp_frame_num;
		}
#endif
		
		[cmd_buffer presentDrawable:drawable];
		[cmd_buffer commit];
	}
}

static void render_kernels(const float& delta, const float& render_delta,
						   const size_t& warp_frame_num) {
//#define WARP_TIMING
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	const auto timing_start = floor_timer2::start();
#endif
	
	//
	float relative_delta = delta / render_delta;
	if(warp_state.target_frame_count > 0) {
		const uint32_t in_between_frame_count = ((warp_state.target_frame_count - warp_state.render_frame_count) /
												 warp_state.render_frame_count);
		const float delta_per_frame = 1.0f / float(in_between_frame_count + 1u);
		
		// reached the limit
		if(warp_frame_num >= in_between_frame_count) {
			return;
		}
		
		relative_delta = float(warp_frame_num + 1u) * delta_per_frame;
	}
	
	// slow fixed delta for debugging/demo purposes
	static float dbg_delta = 0.0f;
	static constexpr const float delta_eps = 0.0025f;
	if(warp_state.is_debug_delta) {
		if(dbg_delta >= (1.0f - delta_eps)) {
			dbg_delta = delta_eps;
		}
		else dbg_delta += delta_eps;
		
		relative_delta = dbg_delta;
	}
	
	if(warp_state.is_scatter) {
		// clear if enabled + always clear the first frame
		if(warp_state.is_clear_frame || (warp_frame_num == 0 && warp_state.is_fixup)) {
			warp_state.dev_queue->execute(warp_state.kernels[KERNEL_SCATTER_CLEAR],
										  scene_fbo.dim_multiple,
										  warp_state.tile_size,
										  scene_fbo.compute_color, clear_color);
		}
		
#if 1
		warp_state.dev_queue->execute(warp_state.kernels[KERNEL_SCATTER_SIMPLE],
									  scene_fbo.dim_multiple,
									  warp_state.tile_size,
									  scene_fbo.color[0],
									  scene_fbo.depth[0],
									  scene_fbo.motion[0],
									  scene_fbo.compute_color,
									  relative_delta);
#else
		const float clear_depth = numeric_limits<float>::max();
		warp_state.scatter_depth_buffer->fill(warp_state.dev_queue, &clear_depth, sizeof(clear_depth));
		
		warp_state.dev_queue->execute(warp_state.kernels[KERNEL_SCATTER_DEPTH_PASS],
									  scene_fbo.dim_multiple,
									  warp_state.tile_size,
									  scene_fbo.depth[0],
									  scene_fbo.motion[0],
									  warp_state.scatter_depth_buffer,
									  relative_delta);
		warp_state.dev_queue->execute(warp_state.kernels[KERNEL_SCATTER_COLOR_DEPTH_TEST],
									  scene_fbo.dim_multiple,
									  warp_state.tile_size,
									  scene_fbo.color[0],
									  scene_fbo.depth[0],
									  scene_fbo.motion[0],
									  scene_fbo.compute_color,
									  warp_state.scatter_depth_buffer,
									  relative_delta);
#endif
	
		if(warp_state.is_fixup) {
			warp_state.dev_queue->execute(warp_state.kernels[KERNEL_SCATTER_FIXUP],
										  scene_fbo.dim_multiple,
										  warp_state.tile_size,
										  scene_fbo.compute_color);
		}
	}
	else {
		warp_state.dev_queue->execute(warp_state.kernels[KERNEL_GATHER],
									  scene_fbo.dim_multiple,
									  warp_state.tile_size,
									  // current frame (t)
									  scene_fbo.color[1u - warp_state.cur_fbo],
									  scene_fbo.depth[1u - warp_state.cur_fbo],
									  // previous frame (t-1)
									  scene_fbo.color[warp_state.cur_fbo],
									  scene_fbo.depth[warp_state.cur_fbo],
									  // forward from prev frame, t-1 -> t
									  scene_fbo.motion[warp_state.cur_fbo * 2],
									  // backward from cur frame, t -> t-1
									  scene_fbo.motion[(1u - warp_state.cur_fbo) * 2 + 1],
									  // packed depth: { fwd t-1 -> t (used here), bwd t-1 -> t-2 (unused here) }
									  scene_fbo.motion_depth[warp_state.cur_fbo],
									  // packed depth: { fwd t+1 -> t (unused here), bwd t -> t-1 (used here) }
									  scene_fbo.motion_depth[1u - warp_state.cur_fbo],
									  scene_fbo.compute_color,
									  relative_delta,
									  warp_state.gather_eps_1,
									  warp_state.gather_eps_2,
									  warp_state.gather_dbg);
	}
	
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	log_debug("warp timing: %f", double(floor_timer2::stop<chrono::microseconds>(timing_start)) / 1000.0);
#endif
}

static void render_full_scene(const metal_obj_model& model, const camera& cam, id <MTLCommandBuffer> cmd_buffer) {
	//////////////////////////////////////////
	// draw shadow map
	
	matrix4f light_bias_mvpm;
	{
		const matrix4f light_pm { matrix4f().perspective(120.0f, 1.0f, 1.0f, 260.0f) };
		const matrix4f light_mvm {
			matrix4f::translation(-light_pos) *
			matrix4f::rotation_deg_named<'x'>(90.0f) // rotate downwards
		};
		const matrix4f light_mvpm { light_mvm * light_pm };
		light_bias_mvpm = matrix4f {
			light_mvpm * matrix4f().scale(0.5f, 0.5f, 0.5f).translate(0.5f, 0.5f, 0.5f)
		};
		
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
							   indexBuffer:((metal_buffer*)obj->indices_metal_vbo.get())->get_metal_buffer()
						 indexBufferOffset:0];
		}
		
		//
		[encoder endEncoding];
		[encoder popDebugGroup];
	}
	
	//////////////////////////////////////////
	// render actual scene
	
	matrix4f pm, mvm, rmvm;
	{
		// set/compute uniforms
		pm = matrix4f().perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
									0.5f, warp_state.view_distance);
		rmvm = (matrix4f::rotation_deg_named<'y'>(cam.get_rotation().y) *
				matrix4f::rotation_deg_named<'x'>(cam.get_rotation().x));
		mvm = matrix4f::translation(cam.get_position()) * rmvm;
		const matrix4f mvpm {
			warp_state.is_scatter ?
			mvm * pm :
			prev_mvm * pm
		};
		const matrix4f next_mvpm { mvm * pm }; // gather
		const matrix4f prev_mvpm { prev_prev_mvm * pm }; // gather
		
		const struct {
			matrix4f mvpm;
			matrix4f m0; // mvm (scatter), next_mvpm (gather)
			matrix4f m1; // prev_mvm (scatter), prev_mvpm (gather)
			matrix4f light_bias_mvpm;
			float3 cam_pos;
			float3 light_pos;
		} uniforms {
			.mvpm = mvpm,
			.m0 = (warp_state.is_scatter ? mvm : next_mvpm),
			.m1 = (warp_state.is_scatter ? prev_mvm : prev_mvpm),
			.light_bias_mvpm = light_bias_mvpm,
			.cam_pos = -cam.get_position(),
			.light_pos = light_pos,
		};
		
		// for gather rendering, this switches every frame
		if(!warp_state.is_scatter) {
			render_pass_desc_gather.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[warp_state.cur_fbo].get())->get_metal_image();
			render_pass_desc_gather.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[warp_state.cur_fbo * 2].get())->get_metal_image();
			render_pass_desc_gather.colorAttachments[2].texture = ((metal_image*)scene_fbo.motion[warp_state.cur_fbo * 2 + 1].get())->get_metal_image();
			render_pass_desc_gather.colorAttachments[3].texture = ((metal_image*)scene_fbo.motion_depth[warp_state.cur_fbo].get())->get_metal_image();
			render_pass_desc_gather.depthAttachment.texture = ((metal_image*)scene_fbo.depth[warp_state.cur_fbo].get())->get_metal_image();
		}
		
		//
		id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:(warp_state.is_scatter ?
																							   render_pass_desc_scatter :
																							   render_pass_desc_gather)];
		encoder.label = @"warp scene encoder";
		[encoder setDepthStencilState:depth_state];
		[encoder setCullMode:MTLCullModeBack];
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		
		//
		[encoder pushDebugGroup:@"scene"];
		if(warp_state.is_scatter) {
			[encoder setRenderPipelineState:render_pipeline_state_scatter];
		}
		else {
			[encoder setRenderPipelineState:render_pipeline_state_gather];
		}
		[encoder setVertexBuffer:((metal_buffer*)model.vertices_buffer.get())->get_metal_buffer() offset:0 atIndex:0];
		[encoder setVertexBuffer:((metal_buffer*)model.tex_coords_buffer.get())->get_metal_buffer() offset:0 atIndex:1];
		[encoder setVertexBuffer:((metal_buffer*)model.normals_buffer.get())->get_metal_buffer() offset:0 atIndex:2];
		[encoder setVertexBuffer:((metal_buffer*)model.binormals_buffer.get())->get_metal_buffer() offset:0 atIndex:3];
		[encoder setVertexBuffer:((metal_buffer*)model.tangents_buffer.get())->get_metal_buffer() offset:0 atIndex:4];
		[encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:5];
		
		[encoder setFragmentSamplerState:sampler_state atIndex:0];
		[encoder setFragmentTexture:((metal_image*)shadow_map.shadow_image.get())->get_metal_image() atIndex:4];
		for(const auto& obj : model.objects) {
			[encoder setFragmentTexture:((metal_image*)model.materials[obj->mat_idx].diffuse)->get_metal_image() atIndex:0];
			[encoder setFragmentTexture:((metal_image*)model.materials[obj->mat_idx].specular)->get_metal_image() atIndex:1];
			[encoder setFragmentTexture:((metal_image*)model.materials[obj->mat_idx].normal)->get_metal_image() atIndex:2];
			[encoder setFragmentTexture:((metal_image*)model.materials[obj->mat_idx].mask)->get_metal_image() atIndex:3];
			
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
								indexCount:uint32_t(obj->index_count)
								 indexType:MTLIndexTypeUInt32
							   indexBuffer:((metal_buffer*)obj->indices_metal_vbo.get())->get_metal_buffer()
						 indexBufferOffset:0];
		}
		
		//
		[encoder endEncoding];
		[encoder popDebugGroup];
	}
	
	//////////////////////////////////////////
	// render sky box
	
	{
		// set/compute uniforms
		const matrix4f imvpm {
			warp_state.is_scatter ?
			(rmvm * pm).inverted() :
			(prev_rmvm * pm).inverted()
		};
		const auto prev_imvpm = (prev_rmvm * pm).inverted(); // scatter
		const matrix4f next_mvpm { rmvm * pm }; // gather
		const matrix4f prev_mvpm { prev_prev_rmvm * pm }; // gather
		
		const struct {
			matrix4f imvpm;
			matrix4f m0; // prev_imvpm (scatter), next_mvpm (gather)
			matrix4f m1; // unused (scatter), prev_mvpm (gather)
		} uniforms {
			.imvpm = imvpm,
			.m0 = (warp_state.is_scatter ? prev_imvpm : next_mvpm),
			.m1 = (warp_state.is_scatter ? matrix4f() : prev_mvpm),
		};
		
		// for gather rendering, this switches every frame
		if(!warp_state.is_scatter) {
			skybox_pass_desc_gather.colorAttachments[0].texture = ((metal_image*)scene_fbo.color[warp_state.cur_fbo].get())->get_metal_image();
			skybox_pass_desc_gather.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion[warp_state.cur_fbo * 2].get())->get_metal_image();
			skybox_pass_desc_gather.colorAttachments[2].texture = ((metal_image*)scene_fbo.motion[warp_state.cur_fbo * 2 + 1].get())->get_metal_image();
			skybox_pass_desc_gather.colorAttachments[3].texture = ((metal_image*)scene_fbo.motion_depth[warp_state.cur_fbo].get())->get_metal_image();
			skybox_pass_desc_gather.depthAttachment.texture = ((metal_image*)scene_fbo.depth[warp_state.cur_fbo].get())->get_metal_image();
		}
		
		//
		id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:(warp_state.is_scatter ?
																							   skybox_pass_desc_scatter :
																							   skybox_pass_desc_gather)];
		encoder.label = @"warp sky box encoder";
		[encoder setDepthStencilState:skybox_depth_state];
		[encoder setCullMode:MTLCullModeBack];
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		
		//
		[encoder pushDebugGroup:@"sky box"];
		if(warp_state.is_scatter) {
			[encoder setRenderPipelineState:skybox_pipeline_state_scatter];
		}
		else {
			[encoder setRenderPipelineState:skybox_pipeline_state_gather];
		}
		[encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:0];
		
		[encoder setFragmentTexture:((metal_image*)skybox_tex.get())->get_metal_image() atIndex:0];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
		
		//
		[encoder endEncoding];
		[encoder popDebugGroup];
	}
	
	// end
	if(!warp_state.is_scatter) {
		// flip state
		warp_state.cur_fbo = 1 - warp_state.cur_fbo;
	}
	
	// remember t-2 and t-1 mvms
	prev_prev_mvm = prev_mvm;
	prev_prev_rmvm = prev_rmvm;
	prev_mvm = mvm;
	prev_rmvm = rmvm;
}

bool metal_renderer::compile_shaders() {
	id <MTLDevice> mtl_dev = ((metal_device*)warp_state.dev.get())->device;
#if defined(FLOOR_IOS)
	metal_shd_lib = [mtl_dev newLibraryWithFile:@"default.metallib" error:nil];
#else
	metal_shd_lib = [mtl_dev newDefaultLibrary];
#endif
	if(!metal_shd_lib) {
		log_error("failed to load default shader lib!");
		return false;
	}
	
	
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"scene_scatter_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"scene_scatter_fs"];
		shader_objects.emplace("SCENE_SCATTER", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"scene_gather_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"scene_gather_fs"];
		shader_objects.emplace("SCENE_GATHER", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"skybox_scatter_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"skybox_scatter_fs"];
		shader_objects.emplace("SKYBOX_SCATTER", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"skybox_gather_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"skybox_gather_fs"];
		shader_objects.emplace("SKYBOX_GATHER", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"shadow_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"shadow_fs"];
		shader_objects.emplace("SHADOW", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"blit_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"blit_fs"];
		shader_objects.emplace("BLIT", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"blit_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"blit_swizzle_fs"];
		shader_objects.emplace("BLIT_SWIZZLE", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"blit_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"warp_gather"];
		shader_objects.emplace("WARP_GATHER", shd);
	}
	return true;
}

#endif
