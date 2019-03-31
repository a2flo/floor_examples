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

#include "vulkan_renderer.hpp"

#if !defined(FLOOR_NO_VULKAN)
#include "warp_state.hpp"
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>

static constexpr const uint32_t render_swap_count = 2;
static struct {
	VkRenderPass scatter;
	VkRenderPass gather;
	VkRenderPass gather_forward;
	VkRenderPass scatter_skybox;
	VkRenderPass gather_skybox;
	VkRenderPass gather_forward_skybox;
	VkRenderPass shadow;
	VkRenderPass blit;
	
	array<VkFramebuffer, render_swap_count> scatter_framebuffers;
	array<VkFramebuffer, render_swap_count> gather_framebuffers;
	array<VkFramebuffer, render_swap_count> gather_forward_framebuffers;
	VkFramebuffer shadow_framebuffer;
	vector<VkFramebuffer> screen_framebuffers;
	
	vector<VkClearValue> scatter_clear_values;
	vector<VkClearValue> gather_clear_values;
	vector<VkClearValue> gather_forward_clear_values;
	vector<VkClearValue> skybox_scatter_clear_values;
	vector<VkClearValue> skybox_gather_clear_values;
	vector<VkClearValue> skybox_gather_forward_clear_values;
	vector<VkClearValue> shadow_clear_values;
	vector<VkClearValue> blit_clear_values;
} passes;

struct pipeline_object {
	VkPipeline pipeline { nullptr };
	VkPipelineLayout layout { nullptr };
};
static array<pipeline_object, vulkan_renderer::warp_pipeline_count()> pipelines;

vulkan_kernel::vulkan_kernel_entry* vulkan_renderer::get_shader_entry(const WARP_SHADER& shader) const {
	return (shader < __MAX_WARP_SHADER ?
			(vulkan_kernel::vulkan_kernel_entry*)shader_entries[shader] :
			nullptr);
};

void vulkan_renderer::create_textures(const COMPUTE_IMAGE_TYPE color_format) {
	common_renderer::create_textures(color_format);
	
	// TODO: update fbo attachments
}

// NOTE: assuming the first attachment is the main color attachment
static VkRenderPass make_render_pass(VkDevice device, vector<compute_image*> attachments, vector<VkClearValue>& clear_values,
									 VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR) {
	vector<VkAttachmentDescription> attachment_desc;
	vector<VkAttachmentReference> color_attachment_refs;
	VkAttachmentReference depth_attachment_ref;
	bool has_depth_attachment { false };
	
	static const VkClearValue clear_color {
		.color = {
			.float32 = { 0.0f, 0.0f, 0.0f, 0.0f },
		}
	};
	static const VkClearValue clear_depth {
		.depthStencil = {
			.depth = 1.0f,
			.stencil = 0,
		}
	};
	
	uint32_t attachment_idx = 0;
	for(const auto& attachment : attachments) {
		const auto image_type = attachment->get_image_type();
		const auto is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type);
		const auto layout = (!is_depth ?
							 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
							 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		if(is_depth) {
			if(has_depth_attachment) {
				log_error("can't have more than one depth attachment");
			}
			has_depth_attachment = true;
			depth_attachment_ref.attachment = attachment_idx++;
			depth_attachment_ref.layout = layout;
			clear_values.emplace_back(clear_depth);
		}
		else {
			color_attachment_refs.emplace_back(VkAttachmentReference {
				.attachment = attachment_idx++,
				.layout = layout,
			});
			clear_values.emplace_back(clear_color);
		}
		
		attachment_desc.emplace_back(VkAttachmentDescription {
			.flags = 0, // no-alias
			.format = ((vulkan_image*)attachment)->get_vulkan_format(),
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = load_op,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = layout,
			.finalLayout = layout,
		});
	}
	
	const VkSubpassDescription sub_pass_info {
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = (uint32_t)color_attachment_refs.size(),
		.pColorAttachments = (!color_attachment_refs.empty() ? color_attachment_refs.data() : nullptr),
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = (has_depth_attachment ? &depth_attachment_ref : nullptr),
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};
	
	const VkRenderPassCreateInfo render_pass_info {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = (uint32_t)attachment_desc.size(),
		.pAttachments = attachment_desc.data(),
		.subpassCount = 1,
		.pSubpasses = &sub_pass_info,
		.dependencyCount = 0,
		.pDependencies = nullptr,
	};
	VkRenderPass render_pass { nullptr };
	VK_CALL_RET(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass),
				"failed to create render pass", nullptr);
	return render_pass;
}

bool vulkan_renderer::make_pipeline(vulkan_device* vk_dev,
									VkRenderPass render_pass,
									const WARP_PIPELINE pipeline_id,
									const WARP_SHADER vertex_shader,
									const WARP_SHADER fragment_shader,
									const uint2& render_size,
									const uint32_t color_attachment_count,
									const bool has_depth_attachment,
									const VkCompareOp depth_compare_op) {
	//log_debug("creating pipeline #%u", pipeline_id);
	auto device = vk_dev->device;
	
	auto vs_entry = get_shader_entry(vertex_shader);
	auto fs_entry = get_shader_entry(fragment_shader);
	
	auto& pipeline = pipelines[pipeline_id];
	
	// create the pipeline layout
	vector<VkDescriptorSetLayout> desc_set_layouts {
		vk_dev->fixed_sampler_desc_set_layout,
		vs_entry->desc_set_layout
	};
	if(fs_entry != nullptr) {
		desc_set_layouts.emplace_back(fs_entry->desc_set_layout);
	}
	const VkPipelineLayoutCreateInfo pipeline_layout_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = uint32_t(desc_set_layouts.size()),
		.pSetLayouts = desc_set_layouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};
	VK_CALL_RET(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline.layout),
				"failed to create pipeline layout #" + to_string(pipeline_id), false);
	
	// create the pipeline
	const VkPipelineVertexInputStateCreateInfo vertex_input_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		// unnecessary when using SSBOs
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};
	const VkPipelineInputAssemblyStateCreateInfo input_assembly_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = false,
	};
	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)render_size.x,
		.height = (float)render_size.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor_rect {
		.offset = { 0, 0 },
		.extent = { render_size.x, render_size.y },
	};
	const VkPipelineViewportStateCreateInfo viewport_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor_rect,
	};
	const VkPipelineRasterizationStateCreateInfo raster_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthClampEnable = false,
		.rasterizerDiscardEnable = false,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = false,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f,
	};
	const VkPipelineMultisampleStateCreateInfo multisample_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = false,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = false,
		.alphaToOneEnable = false,
	};
	const VkPipelineDepthStencilStateCreateInfo depth_stencil_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = true,
		.depthWriteEnable = true,
		.depthCompareOp = depth_compare_op,
		.depthBoundsTestEnable = false,
		.stencilTestEnable = false,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 0.0f,
	};
	const VkPipelineColorBlendAttachmentState color_blend_attachment_state {
		.blendEnable = false,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states(color_attachment_count, color_blend_attachment_state);
	const VkPipelineColorBlendStateCreateInfo color_blend_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = false,
		.logicOp = VK_LOGIC_OP_CLEAR,
		.attachmentCount = color_attachment_count,
		.pAttachments = (color_attachment_count > 0 ? color_blend_attachment_states.data() : nullptr),
		.blendConstants = { 1.0f, 1.0f, 1.0f, 1.0f },
	};
	array<VkPipelineShaderStageCreateInfo, 2> stages;
	stages[0] = vs_entry->stage_info;
	if(fs_entry != nullptr) {
		stages[1] = fs_entry->stage_info;
	}
	const VkGraphicsPipelineCreateInfo gfx_pipeline_info {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stageCount = (fs_entry != nullptr ? 2 : 1),
		.pStages = &stages[0],
		.pVertexInputState = &vertex_input_state,
		.pInputAssemblyState = &input_assembly_state,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &raster_state,
		.pMultisampleState = &multisample_state,
		.pDepthStencilState = (has_depth_attachment ? &depth_stencil_state : nullptr),
		.pColorBlendState = &color_blend_state,
		.pDynamicState = nullptr,
		.layout = pipeline.layout,
		.renderPass = render_pass,
		.subpass = 0,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0,
	};
	VK_CALL_RET(vkCreateGraphicsPipelines(device, nullptr, 1, &gfx_pipeline_info, nullptr, &pipeline.pipeline),
				"failed to create pipeline #" + to_string(pipeline_id), false);
	return true;
}

bool vulkan_renderer::init() {
	if(!common_renderer::init()) {
		return false;
	}
	
	auto vk_ctx = (vulkan_compute*)warp_state.ctx.get();
	auto vk_dev = (vulkan_device*)warp_state.dev.get();
	auto device = vk_dev->device;
	
	// fun times
	clip = matrix4f {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	
	// creates fbo textures/images
	create_textures();
	
	// create the render passes and sub-passes
	// NOTE: this isn't actually storing any references to the images, these are only
	// needed because we want to create the render passes based on their info
	passes.scatter = make_render_pass(device, {
		scene_fbo.color[0].get(),
		scene_fbo.motion[0].get(),
		scene_fbo.depth[0].get(),
	}, passes.scatter_clear_values);
	passes.gather = make_render_pass(device, {
		scene_fbo.color[0].get(),
		scene_fbo.motion[0].get(),
		scene_fbo.motion[1].get(),
		scene_fbo.motion_depth[0].get(),
		scene_fbo.depth[0].get(),
	}, passes.gather_clear_values);
	passes.gather_forward = make_render_pass(device, {
		scene_fbo.color[0].get(),
		scene_fbo.motion[0].get(),
		scene_fbo.depth[0].get(),
	}, passes.gather_forward_clear_values);
	
	// NOTE: skybox clear values are not actually used, since we're loading the color with LOAD_OP_LOAD
	passes.scatter_skybox = make_render_pass(device, {
		scene_fbo.color[0].get(),
		scene_fbo.motion[0].get(),
		scene_fbo.depth[0].get(),
	}, passes.skybox_scatter_clear_values, VK_ATTACHMENT_LOAD_OP_LOAD);
	passes.gather_skybox = make_render_pass(device, {
		scene_fbo.color[0].get(),
		scene_fbo.motion[0].get(),
		scene_fbo.motion[1].get(),
		scene_fbo.motion_depth[0].get(),
		scene_fbo.depth[0].get(),
	}, passes.skybox_gather_clear_values, VK_ATTACHMENT_LOAD_OP_LOAD);
	passes.gather_forward_skybox = make_render_pass(device, {
		scene_fbo.color[0].get(),
		scene_fbo.motion[0].get(),
		scene_fbo.depth[0].get(),
	}, passes.skybox_gather_forward_clear_values, VK_ATTACHMENT_LOAD_OP_LOAD);
	
	passes.shadow = make_render_pass(device, {
		shadow_map.shadow_image.get(),
	}, passes.shadow_clear_values);
	
	passes.blit = make_render_pass(device, {
		// NOTE: again, no ref to this is stored, but it uses the same format as the swapchain images
		scene_fbo.color[0].get(),
	}, passes.blit_clear_values);
	
	// create pipelines for all shaders
	const auto screen_size = floor::get_physical_screen_size();
	if(!make_pipeline(vk_dev, passes.scatter, SCENE_SCATTER, SCENE_SCATTER_VS, SCENE_SCATTER_FS, screen_size, 2, true)) return false;
	if(!make_pipeline(vk_dev, passes.gather, SCENE_GATHER, SCENE_GATHER_VS, SCENE_GATHER_FS, screen_size, 4, true)) return false;
	if(!make_pipeline(vk_dev, passes.gather_forward, SCENE_GATHER_FWD, SCENE_GATHER_FWD_VS, SCENE_GATHER_FWD_FS, screen_size, 2, true)) return false;
	if(!make_pipeline(vk_dev, passes.scatter_skybox, SKYBOX_SCATTER, SKYBOX_SCATTER_VS, SKYBOX_SCATTER_FS, screen_size, 2, true, VK_COMPARE_OP_LESS_OR_EQUAL)) return false;
	if(!make_pipeline(vk_dev, passes.gather_skybox, SKYBOX_GATHER, SKYBOX_GATHER_VS, SKYBOX_GATHER_FS, screen_size, 4, true, VK_COMPARE_OP_LESS_OR_EQUAL)) return false;
	if(!make_pipeline(vk_dev, passes.gather_forward_skybox, SKYBOX_GATHER_FWD, SKYBOX_GATHER_FWD_VS, SKYBOX_GATHER_FWD_FS, screen_size, 2, true, VK_COMPARE_OP_LESS_OR_EQUAL)) return false;
	if(!make_pipeline(vk_dev, passes.shadow, SHADOW, SHADOW_VS, __MAX_WARP_SHADER, shadow_map.dim, 0, true)) return false;
	if(!make_pipeline(vk_dev, passes.blit, BLIT, BLIT_VS, BLIT_FS, screen_size, 1, false)) return false;
	if(!make_pipeline(vk_dev, passes.blit, BLIT_SWIZZLE, BLIT_SWIZZLE_VS, BLIT_SWIZZLE_FS, screen_size, 1, false)) return false;
	
	// create framebuffers from the created render passes + swapchain image views
	// NOTE: scene and skybox framebuffers (render passes) are compatible, so we only need to create them once for both
	{
		VkFramebufferCreateInfo framebuffer_create_info {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = passes.scatter,
			.attachmentCount = 3,
			.pAttachments = nullptr,
			.width = screen_size.x,
			.height = screen_size.y,
			.layers = 1,
		};
		for(uint32_t i = 0; i < render_swap_count; ++i) {
			const VkImageView attachments[] {
				((vulkan_image*)scene_fbo.color[i].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.motion[i].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.depth[i].get())->get_vulkan_image_view(),
			};
			framebuffer_create_info.pAttachments = attachments;
			VK_CALL_RET(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &passes.scatter_framebuffers[i]),
						"failed to create scatter framebuffer", false);
		}
	}
	{
		VkFramebufferCreateInfo framebuffer_create_info {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = passes.gather,
			.attachmentCount = 5,
			.pAttachments = nullptr,
			.width = screen_size.x,
			.height = screen_size.y,
			.layers = 1,
		};
		for(uint32_t i = 0; i < render_swap_count; ++i) {
			const VkImageView attachments[] {
				((vulkan_image*)scene_fbo.color[i].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.motion[i * 2].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.motion[i * 2 + 1].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.motion_depth[i].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.depth[i].get())->get_vulkan_image_view(),
			};
			framebuffer_create_info.pAttachments = attachments;
			VK_CALL_RET(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &passes.gather_framebuffers[i]),
						"failed to create gather framebuffer", false);
		}
	}
	{
		VkFramebufferCreateInfo framebuffer_create_info {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = passes.gather_forward,
			.attachmentCount = 3,
			.pAttachments = nullptr,
			.width = screen_size.x,
			.height = screen_size.y,
			.layers = 1,
		};
		for(uint32_t i = 0; i < render_swap_count; ++i) {
			const VkImageView attachments[] {
				((vulkan_image*)scene_fbo.color[i].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.motion[i].get())->get_vulkan_image_view(),
				((vulkan_image*)scene_fbo.depth[i].get())->get_vulkan_image_view(),
			};
			framebuffer_create_info.pAttachments = attachments;
			VK_CALL_RET(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &passes.gather_forward_framebuffers[i]),
						"failed to create gather forward framebuffer", false);
		}
	}
	{
		const VkFramebufferCreateInfo framebuffer_create_info {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = passes.shadow,
			.attachmentCount = 1,
			.pAttachments = &((vulkan_image*)shadow_map.shadow_image.get())->get_vulkan_image_view(),
			.width = shadow_map.dim.x,
			.height = shadow_map.dim.y,
			.layers = 1,
		};
		VK_CALL_RET(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &passes.shadow_framebuffer),
					"failed to create shadow framebuffer", false);
	}
	{
		VkFramebufferCreateInfo framebuffer_create_info {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = passes.blit,
			.attachmentCount = 1,
			.pAttachments = nullptr,
			.width = screen_size.x,
			.height = screen_size.y,
			.layers = 1,
		};
		passes.screen_framebuffers.resize(vk_ctx->get_swapchain_image_count());
		for(uint32_t i = 0, count = vk_ctx->get_swapchain_image_count(); i < count; ++i) {
			framebuffer_create_info.pAttachments = &vk_ctx->get_swapchain_image_view(i);
			VK_CALL_RET(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &passes.screen_framebuffers[i]),
						"failed to create screen framebuffer", false);
		}
	}
	
	return true;
}

void vulkan_renderer::destroy() {
	// TODO: clean up renderer stuff
}

void vulkan_renderer::render(const floor_obj_model& model, const camera& cam) {
	auto vk_ctx = (vulkan_compute*)warp_state.ctx.get();
	auto vk_queue = (vulkan_queue*)warp_state.dev_queue.get();
	
	//
	auto drawable_ret = vk_ctx->acquire_next_image();
	if(!drawable_ret.first) {
		return;
	}
	auto& drawable = drawable_ret.second;
	auto screen_framebuffer = passes.screen_framebuffers[drawable.index];
	
	//
	common_renderer::render(model, cam);
	
	// blitting
	auto blit_cmd_buffer = vk_queue->make_command_buffer("blit");
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(blit_cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer");
	vector<shared_ptr<compute_buffer>> retained_blit_buffers; // TODO: remove/fix
	{
		const auto& pipeline = pipelines[BLIT];
		const VkRect2D render_area {
			.offset = { 0, 0 },
			.extent = { scene_fbo.dim.x, scene_fbo.dim.y },
		};
		const VkRenderPassBeginInfo pass_begin_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = passes.blit,
			.framebuffer = screen_framebuffer,
			.renderArea = render_area,
			.clearValueCount = (uint32_t)passes.blit_clear_values.size(),
			.pClearValues = passes.blit_clear_values.data(),
		};
		vkCmdBeginRenderPass(blit_cmd_buffer.cmd_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		
		const vector<vulkan_kernel::multi_draw_entry> blit_draw_info {
			{ .vertex_count = 3 }
		};
		((vulkan_kernel*)shaders[BLIT_VS])->multi_draw(warp_state.dev_queue.get(), &blit_cmd_buffer,
													   pipeline.pipeline,
													   pipeline.layout,
													   get_shader_entry(BLIT_VS),
													   get_shader_entry(BLIT_FS),
													   retained_blit_buffers,
													   blit_draw_info,
													   // if gather: this is the previous frame (i.e. if we are at time t and have just rendered I_t, this blits I_t-1)
													   blit_frame ?
													   scene_fbo.color[warp_state.cur_fbo] :
													   scene_fbo.compute_color);
		
		vkCmdEndRenderPass(blit_cmd_buffer.cmd_buffer);
	}
	
	VK_CALL_RET(vkEndCommandBuffer(blit_cmd_buffer.cmd_buffer), "failed to end command buffer");
	vk_queue->submit_command_buffer(blit_cmd_buffer, true);
	
	vk_ctx->present_image(drawable);
}

void vulkan_renderer::render_full_scene(const floor_obj_model& model, const camera& cam) {
	auto vk_queue = (vulkan_queue*)warp_state.dev_queue.get();
	
	// this updates all uniforms
	common_renderer::render_full_scene(model, cam);
	// updated above, still need the actual here
	const auto cur_fbo = 1 - warp_state.cur_fbo;
	
	// TODO: remove this once constant buffers are properly tracked/killed inside vulkan_kernel
	vector<shared_ptr<compute_buffer>> retained_buffers;
	
	//
	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)scene_fbo.dim.x,
		.height = (float)scene_fbo.dim.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D render_area {
		.offset = { 0, 0 },
		.extent = { scene_fbo.dim.x, scene_fbo.dim.y },
	};
	
	// our draw info is static, so only create it once
	static const vector<vulkan_kernel::multi_draw_indexed_entry> scene_draw_info {{
		.index_buffer = model.indices_buffer.get(),
		.index_count = model.index_count
	}};
	static const vector<vulkan_kernel::multi_draw_entry> skybox_draw_info {{
		.vertex_count = 3
	}};
	
	//////////////////////////////////////////
	// draw shadow map
	
	{
		auto cmd_buffer = vk_queue->make_command_buffer("shadow map");
		const VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};
		VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
					"failed to begin command buffer");
		
		// make the attachment writable (again)
		((vulkan_image*)shadow_map.shadow_image.get())->transition_write(cmd_buffer.cmd_buffer);
		
		const VkViewport shadow_viewport {
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)shadow_map.dim.x,
			.height = (float)shadow_map.dim.y,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		const VkRect2D shadow_render_area {
			.offset = { 0, 0 },
			.extent = { shadow_map.dim.x, shadow_map.dim.y },
		};
		vkCmdSetViewport(cmd_buffer.cmd_buffer, 0, 1, &shadow_viewport);
		vkCmdSetScissor(cmd_buffer.cmd_buffer, 0, 1, &shadow_render_area);
		
		const auto& pipeline = pipelines[SHADOW];
		const VkRenderPassBeginInfo pass_begin_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = passes.shadow,
			.framebuffer = passes.shadow_framebuffer,
			.renderArea = shadow_render_area,
			.clearValueCount = (uint32_t)passes.shadow_clear_values.size(),
			.pClearValues = passes.shadow_clear_values.data(),
		};
		vkCmdBeginRenderPass(cmd_buffer.cmd_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		
		// hijack the vertex shader (kernel) for now
		((vulkan_kernel*)shaders[SHADOW_VS])->multi_draw_indexed(warp_state.dev_queue.get(), &cmd_buffer,
																 pipeline.pipeline,
																 pipeline.layout,
																 get_shader_entry(SHADOW_VS),
																 nullptr,
																 retained_buffers,
																 scene_draw_info,
																 model.vertices_buffer,
																 light_mvpm_buffer /* TODO: light_mvpm */);
		
		vkCmdEndRenderPass(cmd_buffer.cmd_buffer);
		
		VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer");
		vk_queue->submit_command_buffer(cmd_buffer, true); // TODO: don't block
	}
	
	//////////////////////////////////////////
	// render actual scene
	
	{
		auto cmd_buffer = vk_queue->make_command_buffer("scene");
		const VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};
		VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
					"failed to begin command buffer");
		
		// make the attachment writable (again)
		((vulkan_image*)scene_fbo.color[cur_fbo].get())->transition_write(cmd_buffer.cmd_buffer);
		
		vkCmdSetViewport(cmd_buffer.cmd_buffer, 0, 1, &viewport);
		vkCmdSetScissor(cmd_buffer.cmd_buffer, 0, 1, &render_area);
		
		const auto& pipeline = pipelines[SCENE_GATHER]; // TODO: others
		const VkRenderPassBeginInfo pass_begin_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = passes.gather,
			.framebuffer = passes.gather_framebuffers[cur_fbo],
			.renderArea = render_area,
			.clearValueCount = (uint32_t)passes.gather_clear_values.size(),
			.pClearValues = passes.gather_clear_values.data(),
		};
		vkCmdBeginRenderPass(cmd_buffer.cmd_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		
		// hijack the vertex shader (kernel) for now
		((vulkan_kernel*)shaders[SCENE_GATHER_VS])->multi_draw_indexed(warp_state.dev_queue.get(), &cmd_buffer,
																	   pipeline.pipeline,
																	   pipeline.layout,
																	   get_shader_entry(SCENE_GATHER_VS),
																	   get_shader_entry(SCENE_GATHER_FS),
																	   retained_buffers,
																	   scene_draw_info,
																	   // vertex shader
																	   model.vertices_buffer,
																	   model.tex_coords_buffer,
																	   model.normals_buffer,
																	   model.binormals_buffer,
																	   model.tangents_buffer,
																	   model.materials_buffer,
																	   scene_uniforms_buffer,
																	   // fragment shader
																	   model.diffuse_textures,
																	   model.specular_textures,
																	   model.normal_textures,
																	   model.mask_textures,
																	   shadow_map.shadow_image);
		
		vkCmdEndRenderPass(cmd_buffer.cmd_buffer);
		
		VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer");
		vk_queue->submit_command_buffer(cmd_buffer, true); // TODO: don't block
	}
	
	//////////////////////////////////////////
	// render sky box
	
	{
		auto cmd_buffer = vk_queue->make_command_buffer("sky box");
		const VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};
		VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
					"failed to begin command buffer");
		vkCmdSetViewport(cmd_buffer.cmd_buffer, 0, 1, &viewport);
		vkCmdSetScissor(cmd_buffer.cmd_buffer, 0, 1, &render_area);
		
		const auto& pipeline = pipelines[SKYBOX_GATHER]; // TODO: others
		const VkRenderPassBeginInfo pass_begin_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = passes.gather_skybox,
			.framebuffer = passes.gather_framebuffers[cur_fbo],
			.renderArea = render_area,
			// NOTE: we aren't clearing anything in here, but load the current color instead
			.clearValueCount = 0,
			.pClearValues = nullptr,
		};
		vkCmdBeginRenderPass(cmd_buffer.cmd_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		
		// hijack the vertex shader (kernel) for now
		((vulkan_kernel*)shaders[SKYBOX_GATHER_VS])->multi_draw(warp_state.dev_queue.get(), &cmd_buffer,
																pipeline.pipeline,
																pipeline.layout,
																get_shader_entry(SKYBOX_GATHER_VS),
																get_shader_entry(SKYBOX_GATHER_FS),
																retained_buffers,
																skybox_draw_info,
																skybox_uniforms_buffer,
																skybox_tex);
		
		vkCmdEndRenderPass(cmd_buffer.cmd_buffer);
		
		VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer");
		vk_queue->submit_command_buffer(cmd_buffer, true); // TODO: don't block
	}
}

bool vulkan_renderer::compile_shaders(const string add_cli_options) {
	return common_renderer::compile_shaders(add_cli_options +
											// a total hack until I implement run-time samplers (samplers are otherwise clamp-to-edge)
											" -DFLOOR_VULKAN_ADDRESS_MODE=vulkan_image::sampler::REPEAT"
											// we want to use the "bind everything" method and draw the scene with 1 draw call
											" -DWARP_IMAGE_ARRAY_SUPPORT");
}

#endif
