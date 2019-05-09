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
#include "nbody_state.hpp"
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>

static VkRenderPass render_pass { nullptr };
static VkPipeline pipeline { nullptr };
static VkPipelineLayout pipeline_layout { nullptr };
static const vulkan_kernel::vulkan_kernel_entry* vs_entry;
static const vulkan_kernel::vulkan_kernel_entry* fs_entry;

static vector<VkFramebuffer> screen_framebuffers;

static array<shared_ptr<vulkan_image>, 2> body_textures {};
static void create_textures(const compute_queue& dev_queue) {
	auto ctx = floor::get_compute_context();
	
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
		
		body_textures[i] = static_pointer_cast<vulkan_image>(ctx->create_image(dev_queue, texture_size,
																			   COMPUTE_IMAGE_TYPE::IMAGE_2D |
																			   COMPUTE_IMAGE_TYPE::RGBA8UI_NORM |
																			   //COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED |
																			   COMPUTE_IMAGE_TYPE::READ,
																			   &pixel_data[0],
																			   COMPUTE_MEMORY_FLAG::READ |
																			   COMPUTE_MEMORY_FLAG::HOST_WRITE /*|
																			   COMPUTE_MEMORY_FLAG::GENERATE_MIP_MAPS*/));
	}
}

bool vulkan_renderer::init(shared_ptr<compute_context> ctx,
						   const compute_device& dev,
						   const compute_queue& dev_queue,
						   shared_ptr<compute_kernel> vs,
						   shared_ptr<compute_kernel> fs) {
	auto vk_ctx = (vulkan_compute*)ctx.get();
	const auto& vk_dev = (const vulkan_device&)dev;
	auto device = vk_dev.device;
	create_textures(dev_queue);
	
	// check vs/fs and get state
	if(!vs) {
		log_error("vertex shader not found");
		return false;
	}
	if(!fs) {
		log_error("fragment shader not found");
		return false;
	}
	
	vs_entry = (const vulkan_kernel::vulkan_kernel_entry*)vs->get_kernel_entry(dev);
	if(!vs_entry) {
		log_error("no vertex shader for this device exists!");
		return false;
	}
	fs_entry = (const vulkan_kernel::vulkan_kernel_entry*)fs->get_kernel_entry(dev);
	if(!fs_entry) {
		log_error("no fragment shader for this device exists!");
		return false;
	}
	
	// create the render pass and sub-passes
	const VkAttachmentReference color_attachment_ref {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	
	const VkAttachmentDescription color_attachment {
		.flags = 0, // no-alias
		.format = vk_ctx->get_screen_format(),
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	
	const VkSubpassDescription sub_pass_info {
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};
	
	const VkRenderPassCreateInfo render_pass_info {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &sub_pass_info,
		.dependencyCount = 0,
		.pDependencies = nullptr,
	};
	VK_CALL_RET(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass),
				"failed to create render pass", false)
	
	// create the pipeline layout
	vector<VkDescriptorSetLayout> desc_set_layouts;
	desc_set_layouts.emplace_back(vk_dev.fixed_sampler_desc_set_layout);
	if(vs_entry->desc_set_layout != nullptr) {
		desc_set_layouts.emplace_back(vs_entry->desc_set_layout);
	}
	if(fs_entry->desc_set_layout != nullptr) {
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
	VK_CALL_RET(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout),
				"failed to create pipeline layout", false)
	
	// create the pipeline
	const VkPipelineShaderStageCreateInfo stages[2] {
		vs_entry->stage_info,
		fs_entry->stage_info,
	};
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
		.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		.primitiveRestartEnable = false,
	};
	const auto screen_size = floor::get_physical_screen_size();
	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)screen_size.x,
		.height = (float)screen_size.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor_rect {
		.offset = { 0, 0 },
		.extent = { screen_size.x, screen_size.y },
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
		.polygonMode = VK_POLYGON_MODE_POINT,
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
		.depthTestEnable = false,
		.depthWriteEnable = false,
		.depthCompareOp = VK_COMPARE_OP_ALWAYS,
		.depthBoundsTestEnable = false,
		.stencilTestEnable = false,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 0.0f,
	};
	const VkPipelineColorBlendAttachmentState color_blend_attachment_state {
		.blendEnable = true,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	const VkPipelineColorBlendStateCreateInfo color_blend_state {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = false,
		.logicOp = VK_LOGIC_OP_CLEAR,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment_state,
		.blendConstants = { 1.0f, 1.0f, 1.0f, 1.0f },
	};
	/*const VkPipelineDynamicStateCreateInfo dynamic_state { // TODO/NOTE: optional
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		// TODO: VK_DYNAMIC_STATE_VIEWPORT when doing resizing, or: recreate all
		.dynamicStateCount = 0,
		.pDynamicStates = nullptr,
	};*/
	const VkGraphicsPipelineCreateInfo gfx_pipeline_info {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stageCount = uint32_t(size(stages)),
		.pStages = stages,
		.pVertexInputState = &vertex_input_state,
		.pInputAssemblyState = &input_assembly_state,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &raster_state,
		.pMultisampleState = &multisample_state,
		.pDepthStencilState = &depth_stencil_state,
		.pColorBlendState = &color_blend_state,
		.pDynamicState = /*&dynamic_state*/ nullptr,
		.layout = pipeline_layout,
		.renderPass = render_pass,
		.subpass = 0,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0,
	};
	VK_CALL_RET(vkCreateGraphicsPipelines(device, nullptr, 1, &gfx_pipeline_info, nullptr, &pipeline),
				"failed to create pipeline", false)
	
	// create framebuffers from the created render pass + swapchain image views
	VkFramebufferCreateInfo framebuffer_create_info {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = render_pass,
		.attachmentCount = 1,
		.pAttachments = nullptr,
		.width = screen_size.x,
		.height = screen_size.y,
		.layers = 1,
	};
	screen_framebuffers.resize(vk_ctx->get_swapchain_image_count());
	for(uint32_t i = 0, count = vk_ctx->get_swapchain_image_count(); i < count; ++i) {
		framebuffer_create_info.pAttachments = &vk_ctx->get_swapchain_image_view(i);
		VK_CALL_RET(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &screen_framebuffers[i]),
					"failed to create framebuffer", false)
	}
	
	log_debug("pipeline initialized");
	
	return true;
}

void vulkan_renderer::destroy(shared_ptr<compute_context> ctx floor_unused,
							  const compute_device& dev floor_unused) {
	for(auto& tex : body_textures) {
		tex = nullptr;
	}
	
	// TODO: clean up renderer stuff
}

void vulkan_renderer::render(shared_ptr<compute_context> ctx,
							 const compute_device& dev,
							 const compute_queue& dev_queue,
							 shared_ptr<compute_buffer> position_buffer) {
	auto vk_ctx = (vulkan_compute*)ctx.get();
	const auto& vk_queue = (const vulkan_queue&)dev_queue;
	const auto& vk_dev = (const vulkan_device&)dev;
	
	//
	auto drawable_ret = vk_ctx->acquire_next_image();
	if(!drawable_ret.first) {
		return;
	}
	auto& drawable = drawable_ret.second;
	auto screen_framebuffer = screen_framebuffers[drawable.index];
	
	//
	auto cmd_buffer = vk_queue.make_command_buffer("nbody render");
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer")
	
	//
	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)drawable.image_size.x,
		.height = (float)drawable.image_size.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(cmd_buffer.cmd_buffer, 0, 1, &viewport);
	const VkRect2D render_area {
		.offset = { 0, 0 },
		.extent = { drawable.image_size.x, drawable.image_size.y },
	};
	vkCmdSetScissor(cmd_buffer.cmd_buffer, 0, 1, &render_area);
	
	const VkClearValue clear_value {
		.color = {
			.float32 = { 0.0f, 0.0f, 0.0f, 0.0f },
		}
	};
	
	// render
	const VkRenderPassBeginInfo pass_begin_info {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = render_pass,
		.framebuffer = screen_framebuffer,
		.renderArea = render_area,
		.clearValueCount = 1,
		.pClearValues = &clear_value,
	};
	vkCmdBeginRenderPass(cmd_buffer.cmd_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	
	vkCmdBindPipeline(cmd_buffer.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	
	// bind/update descs
	const auto write_desc_count = 4;
	vector<VkWriteDescriptorSet> write_descs(write_desc_count);
	vector<uint32_t> dyn_offsets;
	uint32_t write_idx = 0;
	
	// fixed sampler set
	{
		auto& write_desc = write_descs[write_idx++];
		write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc.pNext = nullptr;
		write_desc.dstSet = vk_dev.fixed_sampler_desc_set;
		write_desc.dstBinding = 0;
		write_desc.dstArrayElement = 0;
		write_desc.descriptorCount = (uint32_t)vk_dev.fixed_sampler_set.size();
		write_desc.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		write_desc.pImageInfo = vk_dev.fixed_sampler_image_info.data();
		write_desc.pBufferInfo = nullptr;
		write_desc.pTexelBufferView = nullptr;
	}
	
	// vs arg #0
	{
		auto& write_desc = write_descs[write_idx++];
		write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc.pNext = nullptr;
		write_desc.dstSet = vs_entry->desc_set;
		write_desc.dstBinding = 0;
		write_desc.dstArrayElement = 0;
		write_desc.descriptorCount = 1;
		write_desc.descriptorType = vs_entry->desc_types[0];
		write_desc.pImageInfo = nullptr;
		write_desc.pBufferInfo = ((vulkan_buffer*)position_buffer.get())->get_vulkan_buffer_info();
		write_desc.pTexelBufferView = nullptr;
		
		// always offset 0 for now
		dyn_offsets.emplace_back(0);
	}
	
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
	
	// vs arg #1
	shared_ptr<compute_buffer> constant_buffer = make_shared<vulkan_buffer>(vk_queue, sizeof(uniforms), (void*)&uniforms,
																			COMPUTE_MEMORY_FLAG::READ |
																			COMPUTE_MEMORY_FLAG::HOST_WRITE);
	{
		auto& write_desc = write_descs[write_idx++];
		write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc.pNext = nullptr;
		write_desc.dstSet = vs_entry->desc_set;
		write_desc.dstBinding = 1;
		write_desc.dstArrayElement = 0;
		write_desc.descriptorCount = 1;
		write_desc.descriptorType = vs_entry->desc_types[1];
		write_desc.pImageInfo = nullptr;
		write_desc.pBufferInfo = ((vulkan_buffer*)constant_buffer.get())->get_vulkan_buffer_info();
		write_desc.pTexelBufferView = nullptr;
		
		// always offset 0 for now
		dyn_offsets.emplace_back(0);
	}
	
	// fs arg #0
	{
		auto& write_desc = write_descs[write_idx++];
		write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc.pNext = nullptr;
		write_desc.dstSet = fs_entry->desc_set;
		write_desc.dstBinding = 0;
		write_desc.dstArrayElement = 0;
		write_desc.descriptorCount = 1;
		write_desc.descriptorType = fs_entry->desc_types[0];
		write_desc.pImageInfo = ((vulkan_image*)body_textures[0].get())->get_vulkan_image_info();
		write_desc.pBufferInfo = nullptr;
		write_desc.pTexelBufferView = nullptr;
	}
	
	vkUpdateDescriptorSets(vk_dev.device,
						   write_desc_count, write_descs.data(),
						   0, nullptr);
	const VkDescriptorSet desc_sets[3] {
		vk_dev.fixed_sampler_desc_set,
		vs_entry->desc_set,
		fs_entry->desc_set,
	};
	vkCmdBindDescriptorSets(cmd_buffer.cmd_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							pipeline_layout,
							0,
							(uint32_t)size(desc_sets),
							desc_sets,
							(uint32_t)dyn_offsets.size(),
							dyn_offsets.data());
	
	vkCmdDraw(cmd_buffer.cmd_buffer, nbody_state.body_count, 1, 0, 0);
	
	vkCmdEndRenderPass(cmd_buffer.cmd_buffer);
	
	VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer")
	vk_queue.submit_command_buffer(cmd_buffer, true);
	
	vk_ctx->present_image(drawable);
}

#endif
