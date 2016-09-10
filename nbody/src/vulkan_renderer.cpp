/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

static VkRenderPass render_pass { nullptr };
static VkPipeline pipeline { nullptr };
static VkPipelineLayout pipeline_layout { nullptr };
static const vulkan_kernel::vulkan_kernel_entry* vs_entry;
static const vulkan_kernel::vulkan_kernel_entry* fs_entry;

static VkSurfaceKHR screen_surface { nullptr };
static VkSwapchainKHR screen_swapchain { nullptr };
static VkFormat screen_format { VK_FORMAT_UNDEFINED };
static vector<VkImage> screen_swapchain_images;
static vector<VkImageView> screen_swapchain_image_views;
static vector<VkFramebuffer> screen_framebuffers;
static uint32_t screen_image_index { 0 };
static uint2 screen_size;

static array<shared_ptr<vulkan_image>, 2> body_textures {};
static void create_textures(shared_ptr<compute_device> dev) {
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
		
		body_textures[i] = static_pointer_cast<vulkan_image>(ctx->create_image(dev, texture_size,
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

static bool create_screen(shared_ptr<compute_device> dev,
						  shared_ptr<compute_queue> dev_queue) {
	auto ctx = (vulkan_compute*)floor::get_compute_context().get();
	auto device = (vulkan_device*)dev.get();
	auto vk_queue = (vulkan_queue*)dev_queue.get();
	screen_size = floor::get_physical_screen_size();
	
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if(!SDL_GetWindowWMInfo(floor::get_window(), &wm_info)) {
		log_error("failed to retrieve window info: %s", SDL_GetError());
		return false;
	}
	
#if defined(__WINDOWS__)
	const VkWin32SurfaceCreateInfoKHR surf_create_info {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.hinstance = GetModuleHandle(nullptr),
		.hwnd = wm_info.info.win.window,
	};
	VK_CALL_RET(vkCreateWin32SurfaceKHR(ctx->get_vulkan_context(), &surf_create_info, nullptr, &screen_surface),
				"failed to create xlib surface", false);
#else
	const VkXlibSurfaceCreateInfoKHR surf_create_info {
		.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.dpy = wm_info.info.x11.display,
		.window = wm_info.info.x11.window,
	};
	VK_CALL_RET(vkCreateXlibSurfaceKHR(ctx->get_vulkan_context(), &surf_create_info, nullptr, &screen_surface),
				"failed to create xlib surface", false);
#endif
	// TODO: vkDestroySurfaceKHR
	// TODO: vkGetPhysicalDeviceXlibPresentationSupportKHR/vkGetPhysicalDeviceWin32PresentationSupportKHR
	
	// verify if surface is actually usable
	VkBool32 supported = false;
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device, vk_queue->get_family_index(),
													 screen_surface, &supported),
				"failed to query surface presentability", false);
	if(!supported) {
		log_error("surface is not presentable");
		return false;
	}
	
	//
	uint32_t format_count = 0;
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, screen_surface, &format_count, nullptr),
				"failed to query presentable surface formats count", false);
	if(format_count == 0) {
		log_error("surface doesn't support any formats");
		return false;
	}
	vector<VkSurfaceFormatKHR> formats(format_count);
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, screen_surface, &format_count, formats.data()),
				"failed to query presentable surface formats", false);
	screen_format = formats[0].format;
	VkColorSpaceKHR screen_color_space = formats[0].colorSpace;
	for(const auto& format : formats) {
		// use VK_FORMAT_B8G8R8A8_UNORM if we can
		if(format.format == VK_FORMAT_B8G8R8A8_UNORM) {
			screen_format = VK_FORMAT_B8G8R8A8_UNORM;
			screen_color_space = format.colorSpace;
			break;
		}
	}
	
	// swap chain
	VkSurfaceCapabilitiesKHR surface_caps;
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, screen_surface, &surface_caps),
				"failed to query surface capabilities", false);
	VkExtent2D surface_size = surface_caps.currentExtent;
	if(surface_size.width == 0xFFFFFFFFu) {
		surface_size.width = screen_size.x;
		surface_size.height = screen_size.y;
	}
	
	// choose present mode (vsync is always supported)
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	if(!floor::get_vsync()) {
		uint32_t mode_count = 0;
		VK_CALL_RET(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, screen_surface, &mode_count, nullptr),
					"failed to query surface present mode count", false);
		vector<VkPresentModeKHR> present_modes(mode_count);
		VK_CALL_RET(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, screen_surface, &mode_count, present_modes.data()),
					"failed to query surface present modes", false);
		if(find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != present_modes.end()) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}
	
	const VkSwapchainCreateInfoKHR swapchain_create_info {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = screen_surface,
		.minImageCount = surface_caps.minImageCount,
		.imageFormat = screen_format,
		.imageColorSpace = screen_color_space,
		.imageExtent = surface_size,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		// TODO: handle separate present queue (must be VK_SHARING_MODE_CONCURRENT then + specify queues)
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		// TODO: VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR?
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		// TODO: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR?
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = present_mode,
		// TODO: true for better perf, but can't exec frag shaders on clipped pixels
		.clipped = false,
		.oldSwapchain = nullptr,
	};
	VK_CALL_RET(vkCreateSwapchainKHR(device->device, &swapchain_create_info, nullptr, &screen_swapchain),
				"failed to create swapchain", false);
	// TODO: vkDestroySwapchainKHR
	
	// get all swapchain images + create views
	uint32_t swapchain_image_count = 0;
	VK_CALL_RET(vkGetSwapchainImagesKHR(device->device, screen_swapchain, &swapchain_image_count, nullptr),
				"failed to query swapchain image count", false);
	screen_swapchain_images.resize(swapchain_image_count);
	screen_swapchain_image_views.resize(swapchain_image_count);
	screen_framebuffers.resize(swapchain_image_count); // NOTE: create later (need render pass)
	VK_CALL_RET(vkGetSwapchainImagesKHR(device->device, screen_swapchain, &swapchain_image_count, screen_swapchain_images.data()),
				"failed to retrieve swapchain images", false);
	
	VkImageViewCreateInfo image_view_create_info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = nullptr,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = screen_format,
		// actually want RGBA here (not BGRA)
		.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	for(uint32_t i = 0; i < swapchain_image_count; ++i) {
		image_view_create_info.image = screen_swapchain_images[i];
		VK_CALL_RET(vkCreateImageView(device->device, &image_view_create_info, nullptr, &screen_swapchain_image_views[i]),
					"image view creation failed", false);
	}
	
	return true;
}

bool vulkan_renderer::init(shared_ptr<compute_device> dev,
						   shared_ptr<compute_queue> dev_queue,
						   shared_ptr<compute_kernel> vs,
						   shared_ptr<compute_kernel> fs) {
	auto device = ((vulkan_device*)dev.get())->device;
	if(!create_screen(dev, dev_queue)) {
		return false;
	}
	create_textures(dev);
	
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
		.format = screen_format,
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
				"failed to create render pass", false);
	
	// create the pipeline layout
	vector<VkDescriptorSetLayout> desc_set_layouts;
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
		.pSetLayouts = (!desc_set_layouts.empty() ? desc_set_layouts.data() : nullptr),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};
	VK_CALL_RET(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout),
				"failed to create pipeline layout", false);
	
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
				"failed to create pipeline", false);
	
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
	for(uint32_t i = 0, count = (uint32_t)screen_swapchain_image_views.size(); i < count; ++i) {
		framebuffer_create_info.pAttachments = &screen_swapchain_image_views[i];
		VK_CALL_RET(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &screen_framebuffers[i]),
					"failed to create framebuffer", false);
	}
	
	log_debug("pipeline initialized");
	
	return true;
}

void vulkan_renderer::render(shared_ptr<compute_device> dev,
							 shared_ptr<compute_queue> dev_queue,
							 shared_ptr<compute_buffer> position_buffer) {
	auto vk_queue = (vulkan_queue*)dev_queue.get();
	auto vk_device = (vulkan_device*)dev.get();
	
	//
	const VkSemaphoreCreateInfo sema_create_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	VkSemaphore sema { nullptr };
	VK_CALL_RET(vkCreateSemaphore(vk_device->device, &sema_create_info, nullptr, &sema),
				"failed to create semaphore");
	
	VK_CALL_RET(vkAcquireNextImageKHR(vk_device->device, screen_swapchain, UINT64_MAX, sema,
									  nullptr, &screen_image_index),
				"failed to acquire next presentable image");
	
	//
	auto cmd_buffer = vk_queue->make_command_buffer("nbody render");
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer");
	
	// get image / image view + transition
	auto& screen_image = screen_swapchain_images[screen_image_index];
	auto& screen_framebuffer = screen_framebuffers[screen_image_index];
	
	const VkImageMemoryBarrier image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = screen_image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	vkCmdPipelineBarrier(cmd_buffer.cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
	
	//
	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)screen_size.x,
		.height = (float)screen_size.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(cmd_buffer.cmd_buffer, 0, 1, &viewport);
	const VkRect2D render_area {
		.offset = { 0, 0 },
		.extent = { screen_size.x, screen_size.y },
	};
	vkCmdSetScissor(cmd_buffer.cmd_buffer, 0, 1, &render_area);
	
#if 0 // for testing purposes
	const float4 clear_color { 1.0f, 0.0f, 0.0f, 1.0f };
	const VkImageSubresourceRange range {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1,
	};
	vkCmdClearColorImage(cmd_buffer.cmd_buffer, screen_image, VK_IMAGE_LAYOUT_GENERAL,
						 (const VkClearColorValue*)&clear_color, 1, &range);
#else
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
	const auto write_desc_count = 2;
	vector<VkWriteDescriptorSet> write_descs(write_desc_count);
	vector<uint32_t> dyn_offsets;
	
	// arg #0
	{
		auto& write_desc = write_descs[0];
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
	
	// arg #1
	shared_ptr<compute_buffer> constant_buffer = make_shared<vulkan_buffer>(vk_device, sizeof(uniforms), (void*)&uniforms,
																			COMPUTE_MEMORY_FLAG::READ |
																			COMPUTE_MEMORY_FLAG::HOST_WRITE);
	{
		auto& write_desc = write_descs[1];
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
	
	// only need to set vertex shader stuff here (fragment shader doesn't use any descs right now, b/c of missing image support)
	vkUpdateDescriptorSets(vk_device->device,
						   write_desc_count, write_descs.data(),
						   0, nullptr);
	vkCmdBindDescriptorSets(cmd_buffer.cmd_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							pipeline_layout,
							0,
							1,
							&vs_entry->desc_set,
							(uint32_t)dyn_offsets.size(),
							dyn_offsets.data());
	
	vkCmdDraw(cmd_buffer.cmd_buffer, nbody_state.body_count, 1, 0, 0);
	
	vkCmdEndRenderPass(cmd_buffer.cmd_buffer);
#endif
	
	// transition to present mode
	const VkImageMemoryBarrier present_image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = screen_image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	vkCmdPipelineBarrier(cmd_buffer.cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						 0, 0, nullptr, 0, nullptr, 1, &present_image_barrier);
	
	VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer");
	vk_queue->submit_command_buffer(cmd_buffer, true, &sema, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	
	// present
	const VkPresentInfoKHR present_info {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.swapchainCount = 1,
		.pSwapchains = &screen_swapchain,
		.pImageIndices = &screen_image_index,
		.pResults = nullptr,
	};
	VK_CALL_RET(vkQueuePresentKHR((VkQueue)vk_queue->get_queue_ptr(), &present_info),
				"failed to present");
	
	// cleanup
	vkDestroySemaphore(vk_device->device, sema, nullptr);
}

#endif
