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
#include <floor/darwin/darwin_helper.hpp>
#include <floor/core/timer.hpp>
#include <floor/core/file_io.hpp>

static id <MTLLibrary> metal_shd_lib;
struct metal_shader_object {
	id <MTLFunction> vertex_program;
	id <MTLFunction> fragment_program;
};
static unordered_map<string, shared_ptr<metal_shader_object>> shader_objects;

static id <MTLRenderPipelineState> render_pipeline_state;
static MTLRenderPassDescriptor* render_pass_desc { nullptr };

static id <MTLRenderPipelineState> shadow_pipeline_state;
static MTLRenderPassDescriptor* shadow_pass_desc { nullptr };

static id <MTLRenderPipelineState> blit_pipeline_state;
static MTLRenderPassDescriptor* blit_pass_desc { nullptr };

static id <MTLDepthStencilState> depth_state;
static id <MTLDepthStencilState> passthrough_depth_state;
static id <MTLSamplerState> sampler_state;
static metal_view* view { nullptr };

static struct {
	shared_ptr<compute_image> color;
	shared_ptr<compute_image> depth;
	shared_ptr<compute_image> motion;
	shared_ptr<compute_image> compute_color;
	
	uint2 dim;
	uint2 dim_multiple;
} scene_fbo;
static struct {
	shared_ptr<compute_image> shadow_image;
	uint2 dim { 2048 };
} shadow_map;
static float3 light_pos;
static matrix4f prev_mvm;
static constexpr const float4 clear_color { 0.215f, 0.412f, 0.6f, 0.0f };
static bool first_frame { true };

static void destroy_textures(bool is_resize) {
	scene_fbo.color = nullptr;
	scene_fbo.depth = nullptr;
	scene_fbo.compute_color = nullptr;
	scene_fbo.motion = nullptr;
	
	// only need to destroy this on exit (not on resize!)
	if(!is_resize) shadow_map.shadow_image = nullptr;
}

static void create_textures() {
	scene_fbo.dim = floor::get_physical_screen_size();
	
	// kernel work-group size is { 32, 16 } -> round global size to a multiple of it
	scene_fbo.dim_multiple = scene_fbo.dim.rounded_next_multiple(uint2 { 32, 16 });
	
	scene_fbo.color = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
												   COMPUTE_IMAGE_TYPE::IMAGE_2D |
												   COMPUTE_IMAGE_TYPE::BGRA8UI_NORM |
												   COMPUTE_IMAGE_TYPE::READ_WRITE |
												   COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
												   COMPUTE_MEMORY_FLAG::READ_WRITE);
	
	scene_fbo.depth = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
												   COMPUTE_IMAGE_TYPE::IMAGE_DEPTH |
												   COMPUTE_IMAGE_TYPE::D32F |
												   COMPUTE_IMAGE_TYPE::READ |
												   COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
												   COMPUTE_MEMORY_FLAG::READ);
	
	scene_fbo.compute_color = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
														   COMPUTE_IMAGE_TYPE::IMAGE_2D |
														   COMPUTE_IMAGE_TYPE::BGRA8UI_NORM |
														   COMPUTE_IMAGE_TYPE::READ_WRITE |
														   COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
														   COMPUTE_MEMORY_FLAG::READ_WRITE);
	
	scene_fbo.motion = warp_state.ctx->create_image(warp_state.dev, scene_fbo.dim,
													COMPUTE_IMAGE_TYPE::IMAGE_2D |
													COMPUTE_IMAGE_TYPE::R32UI |
													COMPUTE_IMAGE_TYPE::READ_WRITE |
													COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER,
													COMPUTE_MEMORY_FLAG::READ_WRITE);
	
	if(render_pass_desc != nil) {
		render_pass_desc.colorAttachments[0].texture = ((metal_image*)scene_fbo.color.get())->get_metal_image();
		render_pass_desc.colorAttachments[1].texture = ((metal_image*)scene_fbo.motion.get())->get_metal_image();
		render_pass_desc.depthAttachment.texture = ((metal_image*)scene_fbo.depth.get())->get_metal_image();
	}
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
		MTLRenderPipelineDescriptor* pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
		pipeline_desc.label = @"warp scene pipeline";
		pipeline_desc.sampleCount = 1;
		pipeline_desc.vertexFunction = shader_objects["SCENE"]->vertex_program;
		pipeline_desc.fragmentFunction = shader_objects["SCENE"]->fragment_program;
		pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
		pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		pipeline_desc.colorAttachments[0].blendingEnabled = false;
		pipeline_desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
		pipeline_desc.colorAttachments[1].blendingEnabled = false;
		
		NSError* error = nullptr;
		render_pipeline_state = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
		if(!render_pipeline_state) {
			log_error("failed to create scene pipeline state: %s",
					  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
			return false;
		}
		
		//
		render_pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
		MTLRenderPassColorAttachmentDescriptor* color_attachment = render_pass_desc.colorAttachments[0];
		color_attachment.loadAction = MTLLoadActionClear;
		color_attachment.clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		color_attachment.storeAction = MTLStoreActionStore;
		
		MTLRenderPassColorAttachmentDescriptor* motion_attachment = render_pass_desc.colorAttachments[1];
		motion_attachment.loadAction = MTLLoadActionClear;
		motion_attachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 0.0f);
		motion_attachment.storeAction = MTLStoreActionStore;
		
		MTLRenderPassDepthAttachmentDescriptor* depth_attachment = render_pass_desc.depthAttachment;
		depth_attachment.loadAction = MTLLoadActionClear;
		depth_attachment.clearDepth = 1.0;
		depth_attachment.storeAction = MTLStoreActionStore;
	}
	
	// creates fbo textures/images and sets attachment textures of render_pass_desc
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
		if(!render_pipeline_state) {
			log_error("failed to create scene pipeline state: %s",
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
	
	// used by both scene and shadow renderer
	MTLDepthStencilDescriptor* depth_state_desc = [[MTLDepthStencilDescriptor alloc] init];
	depth_state_desc.depthCompareFunction = MTLCompareFunctionLess;
	depth_state_desc.depthWriteEnabled = YES;
	depth_state = [device newDepthStencilStateWithDescriptor:depth_state_desc];
	
	depth_state_desc.depthCompareFunction = MTLCompareFunctionAlways;
	depth_state_desc.depthWriteEnabled = NO;
	passthrough_depth_state = [device newDepthStencilStateWithDescriptor:depth_state_desc];
	
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
	sampler_desc.mipFilter = MTLSamplerMipFilterLinear;
	sampler_desc.maxAnisotropy = 16;
	sampler_desc.normalizedCoordinates = true;
	sampler_desc.lodMinClamp = 0.0f;
	sampler_desc.lodMaxClamp = numeric_limits<float>::max();
	sampler_desc.sAddressMode = MTLSamplerAddressModeRepeat;
	sampler_desc.tAddressMode = MTLSamplerAddressModeRepeat;
	sampler_state = [device newSamplerStateWithDescriptor:sampler_desc];
	
	return true;
}

void metal_renderer::destroy() {
	destroy_textures(false);
}

static void render_full_scene(const metal_obj_model& model, const camera& cam, id <MTLCommandBuffer> cmd_buffer);
static void render_kernels(const camera& cam,
						   const float& delta, const float& render_delta,
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
		auto mtl_queue = ((metal_queue*)warp_state.dev_queue.get())->get_queue();
		
		id <MTLCommandBuffer> cmd_buffer = [mtl_queue commandBuffer];
		cmd_buffer.label = @"warp render";
		
		//
		//static constexpr const float frame_limit { 0.0f };
		static constexpr const float frame_limit { 1.0f / 10.0f };
		static size_t warp_frame_num = 0;
		bool blit = false;
		if(deltaf < frame_limit && !first_frame) {
			if(warp_state.is_warping) {
				render_kernels(cam, deltaf, render_delta, warp_frame_num);
				blit = false;
				++warp_frame_num;
			}
			else blit = true;
		}
		else {
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
										 ((metal_image*)scene_fbo.color.get())->get_metal_image() :
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

static void render_kernels(const camera& cam,
						   const float& delta, const float& render_delta,
						   const size_t& warp_frame_num) {
//#define WARP_TIMING
#if defined(WARP_TIMING)
	warp_state.dev_queue->finish();
	const auto timing_start = floor_timer2::start();
#endif
	
	// clear if enabled + always clear the first frame
	if(warp_state.is_clear_frame || (warp_frame_num == 0 && warp_state.is_fixup)) {
		warp_state.dev_queue->execute(warp_state.clear_kernel,
									  scene_fbo.dim_multiple,
									  uint2 { 32, 16 },
									  scene_fbo.compute_color, clear_color);
	}
	
	warp_state.dev_queue->execute(warp_state.warp_kernel,
								  scene_fbo.dim_multiple,
								  uint2 { 32, 16 },
								  scene_fbo.color, scene_fbo.depth, scene_fbo.motion, scene_fbo.compute_color,
								  delta / render_delta,
								  (!warp_state.is_single_frame ?
								   float4 { -1.0f } :
								   float4 { cam.get_single_frame_direction(), 1.0f }
								   ));
	
	if(warp_state.is_fixup) {
		warp_state.dev_queue->execute(warp_state.fixup_kernel,
									  scene_fbo.dim_multiple,
									  uint2 { 32, 16 },
									  scene_fbo.compute_color);
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
		const matrix4f light_mproj { matrix4f().perspective(120.0f, 1.0f, 1.0f, 260.0f) };
		const matrix4f light_mview {
			matrix4f().translate(-light_pos.x, -light_pos.y, -light_pos.z) *
			matrix4f().rotate_x(90.0f) // rotate downwards
		};
		const matrix4f light_mvpm { light_mview * light_mproj };
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
	
	{
		// set/compute uniforms
		const matrix4f mproj { matrix4f().perspective(warp_state.fov, float(floor::get_width()) / float(floor::get_height()),
													  0.5f, warp_state.view_distance) };
		const matrix4f rot_mat { matrix4f().rotate_y(cam.get_rotation().y) * matrix4f().rotate_x(cam.get_rotation().x) };
		const matrix4f mview { matrix4f().translate(cam.get_position().x, cam.get_position().y, cam.get_position().z) * rot_mat };
		
		const struct {
			matrix4f mvpm;
			matrix4f mvm;
			matrix4f prev_mvm;
			matrix4f light_bias_mvpm;
			float3 cam_pos;
			float3 light_pos;
		} uniforms {
			.mvpm = mview * mproj,
			.mvm = mview,
			.prev_mvm = prev_mvm,
			.light_bias_mvpm = light_bias_mvpm,
			.cam_pos = -cam.get_position(),
			.light_pos = light_pos,
		};
		
		// set new previous mvm
		prev_mvm = mview;
		
		//
		id <MTLRenderCommandEncoder> encoder = [cmd_buffer renderCommandEncoderWithDescriptor:render_pass_desc];
		encoder.label = @"warp scene encoder";
		[encoder setDepthStencilState:depth_state];
		[encoder setCullMode:MTLCullModeBack];
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		
		//
		[encoder pushDebugGroup:@"scene"];
		[encoder setRenderPipelineState:render_pipeline_state];
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
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"scene_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"scene_fs"];
		shader_objects.emplace("SCENE", shd);
	}
	{
		auto shd = make_shared<metal_shader_object>();
		shd->vertex_program = [metal_shd_lib newFunctionWithName:@"motion_only_vs"];
		shd->fragment_program = [metal_shd_lib newFunctionWithName:@"motion_only_fs"];
		shader_objects.emplace("MOTION_ONLY", shd);
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
	return true;
}

#endif
