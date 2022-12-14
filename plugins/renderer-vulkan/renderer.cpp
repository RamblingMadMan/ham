/*
 * Ham Runtime Plugins
 * Copyright (C) 2022 Keith Hammond
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "renderer.hpp"

#include "ham/renderer-object.h"
#include "ham/plugin.h"
#include "ham/log.h"
#include "ham/fs.h"
#include "ham/functional.h"

using namespace ham::typedefs;

HAM_C_API_BEGIN

static inline bool ham_renderer_on_load_vulkan(){ return true; }
static inline void ham_renderer_on_unload_vulkan(){}

HAM_PLUGIN(
	ham_renderer_vulkan,
	HAM_RENDERER_VULKAN_PLUGIN_UUID,
	HAM_RENDERER_VULKAN_PLUGIN_NAME,
	HAM_VERSION,
	"Vulkan Rendering",
	"Keith Hammond",
	"LGPLv3+",
	HAM_RENDERER_PLUGIN_CATEGORY,
	"Rendering using Vulkan 1.1",

	ham_renderer_on_load_vulkan,
	ham_renderer_on_unload_vulkan
)

//
// Devices
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_phys_device(ham_renderer_vulkan *r){
	ham_u32 num_devices = 0;
	VkResult dev_res = r->vk_fns.vkEnumeratePhysicalDevices(r->vk_inst, &num_devices, nullptr);
	if(dev_res != VK_SUCCESS){
		ham_logapierrorf("Error in vkEnumeratePhysicalDevices: %s", ham::vk::result_to_str(dev_res));
		return false;
	}
	else if(num_devices == 0){
		ham_logapierrorf("Failed to find VkPhysicalDevices: no devices");
		return false;
	}

	ham::std_vector<VkPhysicalDevice> phys_devs(num_devices);

	dev_res = r->vk_fns.vkEnumeratePhysicalDevices(r->vk_inst, &num_devices, phys_devs.data());
	if(dev_res != VK_SUCCESS){
		ham_logapierrorf("Error in vkEnumeratePhysicalDevices: %s", ham::vk::result_to_str(dev_res));
		return false;
	}

	VkPhysicalDeviceProperties selected_props{}, props{};
	VkPhysicalDeviceFeatures selected_feats{}, feats{};

	VkPhysicalDevice selected = nullptr;
	ham_u32 selected_graphics = 0;
	ham_u32 selected_present = 0;

	for(ham_usize i = 0; i < phys_devs.size(); i++){
		const auto dev = phys_devs[i];

		r->vk_fns.vkGetPhysicalDeviceProperties(dev, &props);
		r->vk_fns.vkGetPhysicalDeviceFeatures(dev, &feats);

		if(selected && selected_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
			if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
				// TODO: not just compare framebuffer total size

				const ham_usize largest_fb =
					props.limits.maxFramebufferWidth *
					props.limits.maxFramebufferHeight *
					props.limits.maxFramebufferLayers;

				const ham_usize best_large_fb =
					selected_props.limits.maxFramebufferWidth *
					selected_props.limits.maxFramebufferHeight *
					selected_props.limits.maxFramebufferLayers;

				if(largest_fb < best_large_fb) continue;
			}
			else if(props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
				// try to be discrete
				continue;
			}
		}

		ham_u32 num_queues = 0;
		r->vk_fns.vkGetPhysicalDeviceQueueFamilyProperties(dev, &num_queues, nullptr);

		if(num_queues == 0){
			continue;
		}

		VkSurfaceCapabilitiesKHR caps;
		r->vk_fns.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, r->vk_surface, &caps);

		ham::std_vector<VkQueueFamilyProperties> family_props(num_queues);
		r->vk_fns.vkGetPhysicalDeviceQueueFamilyProperties(dev, &num_queues, family_props.data());

		ham_u32 graphics_family = (ham_u32)-1;
		ham_u32 present_family = (ham_u32)-1;

		for(ham_u32 i = 0; i < num_queues; i++){
			const auto &props = family_props[i];
			if(props.queueFlags & VK_QUEUE_GRAPHICS_BIT){
				graphics_family = i;

				VkBool32 present_support = false;
				r->vk_fns.vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, r->vk_surface, &present_support);

				if(present_support){
					present_family = i;
				}
			}
		}

		if(graphics_family == (ham_u32)-1){
			ham_logapidebugf("No graphics support found for device: %s", props.deviceName);
			continue;
		}
		else if(present_family == (ham_u32)-1){
			ham_logapidebugf("No present support found for device: %s", props.deviceName);
			continue;
		}

		selected = dev;
		selected_props = props;
		selected_feats = feats;
		selected_graphics = graphics_family;
		selected_present = present_family;
	}

	if(!selected){
		ham_logapierrorf("No physical device with VK_QUEUE_GRAPHICS_BIT queue and present support found");
		return false;
	}

	r->vk_phys_dev = selected;
	r->vk_graphics_family = selected_graphics;
	r->vk_present_family = selected_present;
	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_logical_device(ham_renderer_vulkan *r){
	const float graphics_priority = 1.f;

	VkDeviceQueueCreateInfo queue_infos[2];

	auto &&graphics_queue_info = queue_infos[0];
	graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphics_queue_info.pNext = nullptr;
	graphics_queue_info.flags = 0;
	graphics_queue_info.queueFamilyIndex = r->vk_graphics_family;
	graphics_queue_info.queueCount = 1;
	graphics_queue_info.pQueuePriorities = &graphics_priority;

	if(r->vk_graphics_family != r->vk_present_family){
		auto &&present_queue_info = queue_infos[1];
		present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		present_queue_info.pNext = nullptr;
		present_queue_info.queueFamilyIndex = r->vk_present_family;
		present_queue_info.queueCount = 1;
		present_queue_info.pQueuePriorities = &graphics_priority;
	}

	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dyn_state_features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
		.pNext = nullptr,
		.extendedDynamicState = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 features;
	memset(&features, 0, sizeof(features));

	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext = &dyn_state_features;

	const char *dev_exts[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDeviceCreateInfo dev_info;
	dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dev_info.pNext = &features;
	dev_info.flags = 0;
	dev_info.queueCreateInfoCount = 1 + (r->vk_graphics_family != r->vk_present_family);
	dev_info.pQueueCreateInfos = queue_infos;
	dev_info.enabledLayerCount = 0;
	dev_info.ppEnabledLayerNames = nullptr;
	dev_info.enabledExtensionCount = (ham_u32)std::size(dev_exts);
	dev_info.ppEnabledExtensionNames = dev_exts;
	dev_info.pEnabledFeatures = nullptr;

	const auto res = r->vk_fns.vkCreateDevice(r->vk_phys_dev, &dev_info, nullptr, &r->vk_dev);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkCreateDevice: %s", ham::vk::result_to_str(res));
		return false;
	}

	return true;
}

//
// Swapchain
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_swapchain_init(ham_renderer_vulkan *r){
	if(r->vk_swapchain){
		ham::logapiwarn("Swapchain already initialized");
		return true;
	}

	ham::vk::swapchain_info swapchain_info;
	if(!ham::vk::get_swapchain_info(r->vk_fns, r->vk_phys_dev, r->vk_surface, &swapchain_info)){
		ham_logapierrorf("Error getting swapchain info");
		return false;
	}

	ham::logapidebug(
		"Got swapchain with extent {}x{}",
		swapchain_info.capabilities.currentExtent.width,
		swapchain_info.capabilities.currentExtent.height
	);

	VkSurfaceFormatKHR desired_format;
	desired_format.format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
	desired_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	VkSurfaceFormatKHR best_format;
	best_format.format = VK_FORMAT_B8G8R8A8_SRGB;
	best_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	for(const auto &format : swapchain_info.surface_formats){
		ham::logapidebug("Found swapchain format: {} {}", ham_vk_format_str(format.format), ham_vk_color_space_str(format.colorSpace));

		if(format.format == desired_format.format && format.colorSpace == desired_format.colorSpace){
			best_format = desired_format;
			break;
		}
	}

	VkPresentModeKHR desired_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	VkPresentModeKHR best_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

	for(auto mode : swapchain_info.present_modes){
		if(mode == desired_mode){
			best_mode = desired_mode;
			break;
		}
		else if(mode > best_mode && mode <= desired_mode){
			best_mode = mode;
		}
	}

	const ham_u32 image_count = (swapchain_info.capabilities.maxImageCount > 0) ?
		ham_min(swapchain_info.capabilities.minImageCount + 1, swapchain_info.capabilities.maxImageCount) :
		swapchain_info.capabilities.minImageCount + 1;

	const ham_u32 queue_fams[] = { r->vk_graphics_family, r->vk_present_family };

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.surface = r->vk_surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = best_format.format;
	create_info.imageColorSpace = best_format.colorSpace;
	create_info.imageExtent = swapchain_info.capabilities.currentExtent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = 1 + (r->vk_graphics_family != r->vk_present_family);
	create_info.pQueueFamilyIndices = queue_fams;
	create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	create_info.presentMode = best_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	VkResult res = r->vk_fns.vkCreateSwapchainKHR(r->vk_dev, &create_info, nullptr, &r->vk_swapchain);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vkCreateSwapchainKHR: {}", ham_vk_result_str(res));
		return false;
	}

	r->vk_swapchain_format = best_format.format;
	r->vk_swapchain_extent = swapchain_info.capabilities.currentExtent;

	r->vk_surface_caps = swapchain_info.capabilities;
	r->vk_surface_fmt = best_format;
	r->vk_present_mode = best_mode;

	res = r->vk_fns.vkGetSwapchainImagesKHR(r->vk_dev, r->vk_swapchain, &r->vk_num_swapchain_images, nullptr);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vkGetSwapchainImagesKHR: {}", ham_vk_result_str(res));
		r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
		r->vk_swapchain = nullptr;
		return false;
	}
	else if(r->vk_num_swapchain_images == 0){
		ham::logapierror("No swapchain images could be found");
		r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
		r->vk_swapchain = nullptr;
		return false;
	}

	const auto allocator = ham_super(r)->allocator;

	r->vk_swapchain_images = ham_allocator_new_array(allocator, VkImage, r->vk_num_swapchain_images);
	if(!r->vk_swapchain_images){
		ham::logapierror("Failed to allocate {} images", r->vk_num_swapchain_images);

		r->vk_num_swapchain_images = 0;

		r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
		r->vk_swapchain = nullptr;
		return false;
	}

	r->vk_swapchain_views = ham_allocator_new_array(allocator, VkImageView, r->vk_num_swapchain_images);
	if(!r->vk_swapchain_views){
		ham::logapierror("Failed to allocate {} image views", r->vk_num_swapchain_images);

		ham_allocator_delete_array(allocator, r->vk_swapchain_images, r->vk_num_swapchain_images);

		r->vk_swapchain_views = nullptr;
		r->vk_num_swapchain_images = 0;

		r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
		r->vk_swapchain = nullptr;
		return false;
	}

	res = r->vk_fns.vkGetSwapchainImagesKHR(r->vk_dev, r->vk_swapchain, &r->vk_num_swapchain_images, r->vk_swapchain_images);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vkGetSwapchainImagesKHR: {}", ham_vk_result_str(res));

		ham_allocator_delete_array(allocator, r->vk_swapchain_views, r->vk_num_swapchain_images);
		ham_allocator_delete_array(allocator, r->vk_swapchain_images, r->vk_num_swapchain_images);

		r->vk_swapchain_views = nullptr;
		r->vk_swapchain_images = nullptr;
		r->vk_num_swapchain_images = 0;

		r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
		r->vk_swapchain = nullptr;
		return false;
	}

	VkImageCreateInfo depth_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_D32_SFLOAT_S8_UINT,
		.extent = VkExtent3D{ .width = r->vk_swapchain_extent.width, .height = r->vk_swapchain_extent.height, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo alloc_info{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	};

	res = vmaCreateImage(r->vma_allocator, &depth_info, &alloc_info, &r->vk_swapchain_depth, &r->vk_swapchain_depth_alloc, nullptr);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vmaCreateImage: {}", ham_vk_result_str(res));

		ham_allocator_delete_array(allocator, r->vk_swapchain_views, r->vk_num_swapchain_images);
		ham_allocator_delete_array(allocator, r->vk_swapchain_images, r->vk_num_swapchain_images);

		r->vk_swapchain_views = nullptr;
		r->vk_swapchain_images = nullptr;
		r->vk_num_swapchain_images = 0;

		r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
		r->vk_swapchain = nullptr;
		return false;
	}

	VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0 };

	view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };

	view_create_info.subresourceRange.baseMipLevel = 0;
	view_create_info.subresourceRange.levelCount = 1;
	view_create_info.subresourceRange.baseArrayLayer = 0;
	view_create_info.subresourceRange.layerCount = 1;

	view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	view_create_info.image = r->vk_swapchain_depth;
	view_create_info.format = VK_FORMAT_D32_SFLOAT_S8_UINT;

	res = r->vk_fns.vkCreateImageView(r->vk_dev, &view_create_info, nullptr, &r->vk_swapchain_depth_view);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vkCreateImageView: {}", ham_vk_result_str(res));

		vmaDestroyImage(r->vma_allocator, r->vk_swapchain_depth, r->vk_swapchain_depth_alloc);

		ham_allocator_delete_array(allocator, r->vk_swapchain_views, r->vk_num_swapchain_images);
		ham_allocator_delete_array(allocator, r->vk_swapchain_images, r->vk_num_swapchain_images);

		r->vk_swapchain_depth = nullptr;
		r->vk_swapchain_depth_alloc = nullptr;
		r->vk_swapchain_views = nullptr;
		r->vk_swapchain_images = nullptr;
		r->vk_num_swapchain_images = 0;

		r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
		r->vk_swapchain = nullptr;
		return false;
	}

	view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	for(u32 i = 0; i < r->vk_num_swapchain_images; i++){
		view_create_info.image = r->vk_swapchain_images[i];
		view_create_info.format = r->vk_swapchain_format;

		res = r->vk_fns.vkCreateImageView(r->vk_dev, &view_create_info, nullptr, &r->vk_swapchain_views[i]);
		if(res != VK_SUCCESS){
			ham::logapierror("Error in vkCreateImageView: %s", ham_vk_result_str(res));

			for(u32 j = 0; j < i; j++){
				r->vk_fns.vkDestroyImageView(r->vk_dev, r->vk_swapchain_views[j], nullptr);
			}

			r->vk_fns.vkDestroyImageView(r->vk_dev, r->vk_swapchain_depth_view, nullptr);
			r->vk_swapchain_depth_view = nullptr;

			vmaDestroyImage(r->vma_allocator, r->vk_swapchain_depth, r->vk_swapchain_depth_alloc);
			r->vk_swapchain_depth = nullptr;
			r->vk_swapchain_depth_alloc = nullptr;

			ham_allocator_delete_array(allocator, r->vk_swapchain_views, r->vk_num_swapchain_images);
			ham_allocator_delete_array(allocator, r->vk_swapchain_images, r->vk_num_swapchain_images);
			r->vk_swapchain_views = nullptr;
			r->vk_swapchain_images = nullptr;
			r->vk_num_swapchain_images = 0;

			r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
			r->vk_swapchain = nullptr;
			return false;
		}
	}

	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_swapchain_fini(ham_renderer_vulkan *r){
	if(!r->vk_swapchain){
		ham::logapiwarn("Swapchain already finished");
		return true;
	}

	for(ham_usize i = 0; i < r->vk_num_swapchain_images; i++){
		r->vk_fns.vkDestroyImageView(r->vk_dev, r->vk_swapchain_views[i], nullptr);
	}

	r->vk_fns.vkDestroyImageView(r->vk_dev, r->vk_swapchain_depth_view, nullptr);
	r->vk_swapchain_depth_view = nullptr;

	vmaDestroyImage(r->vma_allocator, r->vk_swapchain_depth, r->vk_swapchain_depth_alloc);
	r->vk_swapchain_depth = nullptr;
	r->vk_swapchain_depth_alloc = nullptr;

	const auto allocator = ham_super(r)->allocator;

	ham_allocator_delete_array(allocator, r->vk_swapchain_views, r->vk_num_swapchain_images);
	ham_allocator_delete_array(allocator, r->vk_swapchain_images, r->vk_num_swapchain_images);
	r->vk_swapchain_views = nullptr;
	r->vk_swapchain_images = nullptr;

	r->vk_fns.vkDestroySwapchainKHR(r->vk_dev, r->vk_swapchain, nullptr);
	r->vk_swapchain = nullptr;

	return true;
}

//
// Render passes
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_screen_render_pass(ham_renderer_vulkan *r){
	VkAttachmentDescription attach_descs[] = {
		{
			.flags = 0,
			.format = r->vk_swapchain_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		},
		{
			.flags = 0,
			.format = VK_FORMAT_D32_SFLOAT_S8_UINT,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
		}
	};

	VkAttachmentReference color_attachments[] = {
		{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		}
	};

	VkAttachmentReference depth_attachment{
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpasses[] = {
		{
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0, .pInputAttachments = nullptr,
			.colorAttachmentCount = (ham_u32)std::size(color_attachments),
			.pColorAttachments = color_attachments,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = &depth_attachment,
			.preserveAttachmentCount = 0, .pPreserveAttachments = nullptr,
		}
	};

	VkSubpassDependency dependencies[] = {
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0,
		}
	};

	ham::vk::render_pass_create_info create_info(
		nullptr, 0,
		(ham_u32)std::size(attach_descs), attach_descs,
		(ham_u32)std::size(subpasses), subpasses,
		(ham_u32)std::size(dependencies), dependencies
	);

	const auto res = r->vk_fns.vkCreateRenderPass(r->vk_dev, create_info.ptr(), nullptr, &r->vk_render_pass_screen);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkCreateRenderPass: %s", ham::vk::result_to_str(res));
		return false;
	}

	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_data_render_pass(ham_renderer_vulkan *r){
	VkAttachmentDescription attach_descs[] = {
		{
			.flags = 0,
			.format = r->vk_swapchain_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		},
		{
			.flags = 0,
			.format = VK_FORMAT_D32_SFLOAT_S8_UINT,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		}
	};

	VkAttachmentReference color_attachments[] = {
		{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		}
	};

	VkAttachmentReference depth_attachment{
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpasses[] = {
		{
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0, .pInputAttachments = nullptr,
			.colorAttachmentCount = (ham_u32)std::size(color_attachments),
			.pColorAttachments = color_attachments,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = &depth_attachment,
			.preserveAttachmentCount = 0, .pPreserveAttachments = nullptr,
		}
	};

	VkSubpassDependency dependencies[] = {
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0,
		}
	};

	ham::vk::render_pass_create_info create_info(
		nullptr, 0,
		(ham_u32)std::size(attach_descs), attach_descs,
		(ham_u32)std::size(subpasses), subpasses,
		(ham_u32)std::size(dependencies), dependencies
	);

	const auto res = r->vk_fns.vkCreateRenderPass(r->vk_dev, create_info.ptr(), nullptr, &r->vk_render_pass_screen);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkCreateRenderPass: %s", ham::vk::result_to_str(res));
		return false;
	}

	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_render_passes(ham_renderer_vulkan *r){
	if(!ham_renderer_vulkan_init_screen_render_pass(r)){
		ham_logapierrorf("Error initializing screen render pass");
		return false;
	}

//	if(!ham_renderer_vulkan_init_data_render_pass(r)){
//		ham_logapierrorf("Error initializing data render pass");
//		r->vk_fns.vkDestroyRenderPass(r->vk_dev, r->vk_render_pass_screen, nullptr);
//		r->vk_render_pass_screen = nullptr;
//		return false;
//	}

	return true;
}

ham_nonnull_args(1)
static inline void ham_renderer_vulkan_fini_render_passes(ham_renderer_vulkan *r){
	r->vk_fns.vkDestroyRenderPass(r->vk_dev, r->vk_render_pass_screen, nullptr);
	//r->vk_fns.vkDestroyRenderPass(r->vk_dev, r->vk_render_pass_data, nullptr);

	r->vk_render_pass_screen = nullptr;
	//r->vk_render_pass_data = nullptr;
}

//
// Descriptor sets
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_descriptor_set_layouts(ham_renderer_vulkan *r){
	VkDescriptorSetLayoutBinding vertex_layout_binding{};
	vertex_layout_binding.binding = 0;
	vertex_layout_binding.descriptorCount = 1;
	vertex_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vertex_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertex_layout_binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layout_create_info{};
	layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_create_info.bindingCount = 1;
	layout_create_info.pBindings = &vertex_layout_binding;

	const ham::vk::result res = r->vk_fns.vkCreateDescriptorSetLayout(r->vk_dev, &layout_create_info, nullptr, &r->vk_descriptor_set_layout);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkCreateDescriptorSetLayout: %s", res.to_str());
		return false;
	}

	return true;
}

ham_nonnull_args(1)
static inline void ham_renderer_vulkan_fini_descriptor_set_layouts(ham_renderer_vulkan *r){
	r->vk_fns.vkDestroyDescriptorSetLayout(r->vk_dev, r->vk_descriptor_set_layout, nullptr);
	r->vk_descriptor_set_layout = nullptr;
}

// TODO: descriptor pools+sets

//
// Pipelines
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_pipeline_layout(ham_renderer_vulkan *r){
	ham::vk::pipeline_layout_create_info layout_create_info(nullptr, 0, 1, &r->vk_descriptor_set_layout, 0, nullptr);

	r->vk_pipeline_layout = ham::vk::create_pipeline_layout(r->vk_fns, r->vk_dev, layout_create_info);
	if(!r->vk_pipeline_layout){
		ham_logapierrorf("Error creating pipeline layout");
		return false;
	}

	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_pipeline(ham_renderer_vulkan *r){
	const auto plugin_dir = ham_plugin_dir(ham_super(r)->plugin);

	ham_path_buffer_utf8 shader_path = { 0 };

	memcpy(shader_path, plugin_dir.ptr, plugin_dir.len);
	shader_path[plugin_dir.len] = '/';

	const auto init_shader = [&](const char *kind, const ham::str8 &filename) -> VkShaderModule{
		const auto path_len = plugin_dir.len + filename.len() + 1;

		if(path_len >= HAM_PATH_BUFFER_SIZE){
			shader_path[plugin_dir.len] = '\0';
			ham_logerrorf(
				"ham_renderer_vulkan_init_shaders",
				"Path to %s shader too long (%zu, max %d): %s/%s",
				kind,
				path_len, HAM_PATH_BUFFER_SIZE,
				plugin_dir.ptr, filename.data()
			);
			return nullptr;
		}

		memcpy(shader_path + plugin_dir.len + 1, filename.ptr(), filename.len());
		shader_path[path_len] = '\0';

		const auto file = ham_file_open_utf8(ham::str8(shader_path, path_len), HAM_OPEN_READ);
		if(!file){
			ham_logerrorf(
				"ham_renderer_vulkan_init_shaders",
				"Failed to open %s shader: %s",
				kind,
				shader_path
			);
			return nullptr;
		}

		ham_file_info file_info;
		if(!ham_file_get_info(file, &file_info)){
			ham_logerrorf(
				"ham_renderer_vulkan_init_shaders",
				"Failed to get file info for %s shader: %s",
				kind,
				shader_path
			);
			ham_file_close(file);
			return nullptr;
		}

		const auto src_buf = (char*)ham_allocator_alloc(ham_super(r)->allocator, alignof(void*), file_info.size);
		if(!src_buf){
			ham_logerrorf(
				"ham_renderer_vulkan_init_shaders",
				"Failed to allocate memory for %s shader: %s",
				kind,
				shader_path
			);
			ham_file_close(file);
			return nullptr;
		}

		ham_file_read(file, src_buf, file_info.size);
		ham_file_close(file);

		VkShaderModule module = ham::vk::create_shader_module(r->vk_fns, r->vk_dev, file_info.size, src_buf);
		if(!module){
			ham_logerrorf(
				"ham_renderer_vulkan_init_shaders",
				"Failed to create %s shader: %s",
				kind,
				shader_path
			);
		}

		ham_allocator_free(ham_super(r)->allocator, src_buf);

		return module;
	};

	VkShaderModule vert = init_shader("vertex", "shaders/screen.vert.spv");
	if(!vert){
		ham_logapierrorf("Error initializing vertex shader");
		return false;
	}

	VkShaderModule frag = init_shader("fragment", "shaders/screen.frag.spv");
	if(!frag){
		ham_logapierrorf("Error initializing fragment shader");
		ham::vk::destroy_shader_module(r->vk_fns, r->vk_dev, vert);
		return false;
	}

	VkPipelineShaderStageCreateInfo stages[] = {
		ham::vk::pipeline_shader_stage_create_info(nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,   vert, "main", nullptr),
		ham::vk::pipeline_shader_stage_create_info(nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag, "main", nullptr),
	};

	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		//VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
		//VK_DYNAMIC_STATE_STENCIL_OP,
	};

	ham::vk::pipeline_dynamic_state_create_info dynamic_state_create_info(nullptr, 0, (ham_u32)std::size(dynamic_states), dynamic_states);

	const auto point_attribs_size = (sizeof(ham_vec3) * 2) + sizeof(ham_vec2);

	VkVertexInputBindingDescription binding_descs[] = {
		{ 0, point_attribs_size, VK_VERTEX_INPUT_RATE_VERTEX },
	};

	VkVertexInputAttributeDescription attrib_descs[] = {
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(ham_vec3) },
		{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 2 * sizeof(ham_vec3) },
	};

	ham::vk::pipeline_vertex_input_state_create_info vertex_input_state_create_info(
		nullptr,
		0,
		(ham_u32)std::size(binding_descs), binding_descs, // bindings
		(ham_u32)std::size(attrib_descs), attrib_descs // attribs
	);

	ham::vk::pipeline_input_assembly_state_create_info input_assembly_state_create_info(nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, VK_FALSE);

	VkViewport viewport = {
		.x = 0.f, .y = 0.f,
		.width = (float)r->vk_swapchain_extent.width,
		.height = (float)r->vk_swapchain_extent.height,
		.minDepth = 0.f,
		.maxDepth = 1.f
	};

	VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = r->vk_swapchain_extent
	};

	ham::vk::pipeline_viewport_state_create_info viewport_state_create_info(nullptr, 0, 1, &viewport, 1, &scissor);

	ham::vk::pipeline_rasterization_state_create_info rasterization_state_create_info(
		nullptr, 0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_CLOCKWISE,
		VK_FALSE, 0.f, 0.f, 0.f, // depth bias
		1.f
	);

	ham::vk::pipeline_multisample_state_create_info multisample_state_create_info(
		nullptr, 0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	);

	VkPipelineColorBlendAttachmentState color_blend_attachment_state{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	ham::vk::pipeline_color_blend_state_create_info color_blend_state_create_info(
		nullptr, 0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1, &color_blend_attachment_state,
		{ 0.f, 0.f, 0.f, 0.f }
	);

	/*
				VkPipelineLayout layout,
				VkRenderPass render_pass,
				u32 subpass,
				VkPipeline base_pipeline_handle,
				i32 base_pipeline_index
	*/

	VkPipelineDepthStencilStateCreateInfo depth_state_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = false,
		.depthWriteEnable = true,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = false,
		.stencilTestEnable = false,
		.front = { },
		.back = { },
		.minDepthBounds = 0.f,
		.maxDepthBounds = 1.f
	};

	ham::vk::graphics_pipeline_create_info create_info(
		nullptr, 0,
		(ham_u32)std::size(stages), stages,
		vertex_input_state_create_info.ptr(),
		input_assembly_state_create_info.ptr(),
		nullptr,
		viewport_state_create_info.ptr(),
		rasterization_state_create_info.ptr(),
		multisample_state_create_info.ptr(),
		&depth_state_info,
		color_blend_state_create_info.ptr(),
		dynamic_state_create_info.ptr(),
		r->vk_pipeline_layout,
		r->vk_render_pass_screen,
		0,
		VK_NULL_HANDLE,
		-1
	);

	const auto res = r->vk_fns.vkCreateGraphicsPipelines(r->vk_dev, VK_NULL_HANDLE, 1, create_info.ptr(), nullptr, &r->vk_pipeline);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkCreateGraphicsPipelines");
		ham::vk::destroy_shader_module(r->vk_fns, r->vk_dev, vert);
		ham::vk::destroy_shader_module(r->vk_fns, r->vk_dev, frag);
		return false;
	}

	ham::vk::destroy_shader_module(r->vk_fns, r->vk_dev, vert);
	ham::vk::destroy_shader_module(r->vk_fns, r->vk_dev, frag);
	return true;
}

//
// Framebuffers
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_framebuffers(ham_renderer_vulkan *r){
	const auto allocator = ham_super(r)->allocator;

	r->vk_swapchain_framebuffers = ham_allocator_new_array(allocator, VkFramebuffer, r->vk_num_swapchain_images);
	if(!r->vk_swapchain_framebuffers){
		ham::logapierror("Error allocating memory for {} framebuffers", r->vk_num_swapchain_images);
		return false;
	}

	for(ham_usize i = 0; i < r->vk_num_swapchain_images; i++){
		VkImageView attachments[] = { r->vk_swapchain_views[i], r->vk_swapchain_depth_view };

		ham::vk::framebuffer_create_info create_info(
			nullptr, 0,
			r->vk_render_pass_screen,
			(ham_u32)std::size(attachments), attachments,
			r->vk_swapchain_extent.width,
			r->vk_swapchain_extent.height,
			1
		);

		const auto res = r->vk_fns.vkCreateFramebuffer(r->vk_dev, create_info.ptr(), nullptr, &r->vk_swapchain_framebuffers[i]);
		if(res != VK_SUCCESS){
			ham::logapierror("Error in vkCreateFramebuffer: {}", ham_vk_result_str(res));
			for(ham_usize j = 0; j < i; j++){
				r->vk_fns.vkDestroyFramebuffer(r->vk_dev, r->vk_swapchain_framebuffers[j], nullptr);
			}
			return false;
		}
	}

	return true;
}

ham_nonnull_args(1)
static inline void ham_renderer_vulkan_fini_framebuffers(ham_renderer_vulkan *r){
	const auto allocator = ham_super(r)->allocator;

	for(ham_u32 i = 0; i < r->vk_num_swapchain_images; i++){
		r->vk_fns.vkDestroyFramebuffer(r->vk_dev, r->vk_swapchain_framebuffers[i], nullptr);
	}

	ham_allocator_delete_array(allocator, r->vk_swapchain_framebuffers, r->vk_num_swapchain_images);
	r->vk_swapchain_framebuffers = nullptr;
}

//
// Command buffers/pools
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_command_pools(ham_renderer_vulkan *r){
	ham::vk::command_pool_create_info create_info(nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, r->vk_graphics_family);

	const auto res = r->vk_fns.vkCreateCommandPool(r->vk_dev, create_info.ptr(), nullptr, &r->vk_cmd_pool);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkCreateCommandPool: %s", ham::vk::result_to_str(res));
		return false;
	}

	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_command_buffers(ham_renderer_vulkan *r){
	const VkCommandBufferAllocateInfo alloc_info{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		r->vk_cmd_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		ham_impl_max_queued_frames
	};

	const ham::vk::result res = r->vk_fns.vkAllocateCommandBuffers(r->vk_dev, &alloc_info, r->vk_cmd_bufs);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkAllocateCommandBuffers: %s", res.to_str());
		return false;
	}

	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_init_sync_objects(ham_renderer_vulkan *r){
	const VkSemaphoreCreateInfo sem_create_info{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0
	};

	const VkFenceCreateInfo fence_create_info{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT
	};

	for(ham_u32 i = 0; i < ham_impl_max_queued_frames; i++){
		const ham::vk::result res = r->vk_fns.vkCreateSemaphore(r->vk_dev, &sem_create_info, nullptr, &r->vk_img_sems[i]);
		if(res != VK_SUCCESS){
			ham_logapierrorf("Error in vkCreateSemaphore: %s", res.to_str());

			for(ham_u32 j = 0; j < i; j++){
				r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_img_sems[j], nullptr);
			}

			return false;
		}
	}

	for(ham_u32 i = 0; i < ham_impl_max_queued_frames; i++){
		const ham::vk::result res = r->vk_fns.vkCreateSemaphore(r->vk_dev, &sem_create_info, nullptr, &r->vk_render_sems[i]);
		if(res != VK_SUCCESS){
			ham_logapierrorf("Error in vkCreateSemaphore: %s", res.to_str());

			for(ham_u32 j = 0; j < i; j++){
				r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_render_sems[j], nullptr);
			}

			for(ham_u32 j = 0; j < ham_impl_max_queued_frames; j++){
				r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_img_sems[j], nullptr);
			}

			return false;
		}
	}

	for(ham_u32 i = 0; i < ham_impl_max_queued_frames; i++){
		const ham::vk::result res = r->vk_fns.vkCreateFence(r->vk_dev, &fence_create_info, nullptr, &r->vk_frame_fences[i]);
		if(res != VK_SUCCESS){
			ham_logapierrorf("Error in vkCreateFence: %s", res.to_str());

			for(ham_u32 j = 0; j < i; j++){
				r->vk_fns.vkDestroyFence(r->vk_dev, r->vk_frame_fences[j], nullptr);
			}

			for(ham_u32 j = 0; j < ham_impl_max_queued_frames; j++){
				r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_render_sems[j], nullptr);
				r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_img_sems[j], nullptr);
			}

			return false;
		}
	}

	return true;
}

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_fill_command_buffers(ham_renderer_vulkan *r, ham_u32 frame_idx, ham_u32 swapchain_idx){
	const VkCommandBufferBeginInfo begin_info{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr
	};

	const auto cmd_buf = r->vk_cmd_bufs[frame_idx];

	ham::vk::result res = r->vk_fns.vkBeginCommandBuffer(cmd_buf, &begin_info);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkBeginCommandBuffer: %s", res.to_str());
		return false;
	}

//	VkImageMemoryBarrier depth_barrier{
//		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//		.pNext = nullptr,
//		.srcAccessMask = 0,
//		.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
//		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//		.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
//		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//		.image = r->vk_swapchain_depth,
//		.subresourceRange = {
//			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
//			.baseMipLevel = 0,
//			.levelCount = 1,
//			.baseArrayLayer = 0,
//			.layerCount = 1,
//		},
//	};

//	r->vk_fns.vkCmdPipelineBarrier(
//		r->vk_cmd_bufs[frame_idx],
//		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
//		0,
//		0, nullptr,
//		0, nullptr,
//		1, &depth_barrier
//	);

	const VkClearValue clear_values[] = {
		{ .color = { .float32 = { 0.f, 0.f, 0.f, 1.f } } },
		{ .depthStencil = { .depth = 1.f, .stencil = 0 } }
	};

	const VkRenderPassBeginInfo render_pass_info{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr,
		r->vk_render_pass_screen, r->vk_swapchain_framebuffers[swapchain_idx],
		{ .offset = { 0, 0 }, .extent = r->vk_swapchain_extent },
		(ham_u32)std::size(clear_values), clear_values
	};

	r->vk_fns.vkCmdBeginRenderPass(cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	const VkViewport viewport{
		.x = 0.f,
		.y = 0.f,
		.width = (float)r->vk_swapchain_extent.width,
		.height = (float)r->vk_swapchain_extent.height,
		.minDepth = 0.f,
		.maxDepth = 1.f,
	};

	const VkRect2D scissor{
		.offset = { .x = 0, .y = 0 },
		.extent = r->vk_swapchain_extent
	};

	r->vk_fns.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, r->vk_pipeline);
	r->vk_fns.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
	r->vk_fns.vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

	VkBuffer screen_vbos[] = { r->screen_draw_group->vbo };
	VkDeviceSize screen_vbo_offsets[] = { 0 };

	r->vk_fns.vkCmdBindVertexBuffers(cmd_buf, 0, 1, screen_vbos, screen_vbo_offsets);
	r->vk_fns.vkCmdBindIndexBuffer(cmd_buf, r->screen_draw_group->ibo, 0, VK_INDEX_TYPE_UINT32);

	r->vk_fns.vkCmdDrawIndexedIndirect(cmd_buf, r->screen_draw_group->cbo, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
	//r->vk_fns.vkCmdDrawIndexed(cmd_buf, 4, 1, 0, 0, 0);

	r->vk_fns.vkCmdEndRenderPass(cmd_buf);

	res = r->vk_fns.vkEndCommandBuffer(cmd_buf);
	if(res != VK_SUCCESS){
		ham_logapierrorf("Error in vkEndCommandBuffer: %s", res.to_str());
		return false;
	}

	return true;
}

//
// Utility functions
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_swapchain_reset(ham_renderer_vulkan *r){
	r->vk_fns.vkDeviceWaitIdle(r->vk_dev);

	ham_renderer_vulkan_fini_framebuffers(r);
	ham_renderer_vulkan_swapchain_fini(r);

	if(!ham_renderer_vulkan_swapchain_init(r)){
		ham_logapierrorf("Error initializing swapchain");
		return false;
	}

	if(!ham_renderer_vulkan_init_framebuffers(r)){
		ham_logapierrorf("Error initializing framebuffers");
		ham_renderer_vulkan_swapchain_fini(r);
		return false;
	}

	r->swapchain_dirty = false;
	return true;
}

//
// Ctor/dtor
//

ham_nonnull_args(1)
static inline ham_renderer_vulkan *ham_renderer_vulkan_ctor(ham_renderer_vulkan *mem, ham_u32 nargs, va_list va){
	if(nargs != 1){
		ham::logapierror("Wrong number of arguments passed: {}, expected 1 (const ham_renderer_create_args*)", nargs);
		return nullptr;
	}

	const ham_renderer_create_args *create_args = va_arg(va, const ham_renderer_create_args*);

	const auto r = new(mem) ham_renderer_vulkan;

	// zero all members
	memset((char*)r + sizeof(ham_renderer), 0, sizeof(ham_renderer_vulkan) - sizeof(ham_renderer));

	r->vk_inst = create_args->vulkan.instance;
	r->vk_surface = create_args->vulkan.surface;
	r->vk_fns.vkGetInstanceProcAddr = create_args->vulkan.vkGetInstanceProcAddr;

#define HAM_RESOLVE_VK(fn_id) \
	r->vk_fns.fn_id = (PFN_##fn_id)r->vk_fns.vkGetInstanceProcAddr(r->vk_inst, #fn_id); \
	if(!r->vk_fns.fn_id){ \
		ham::logapierror("Could not resolve function \"" #fn_id "\""); \
		std::destroy_at(r); \
		return nullptr; \
	}

	HAM_RESOLVE_VK(vkGetDeviceProcAddr)

	HAM_RESOLVE_VK(vkEnumeratePhysicalDevices)
	HAM_RESOLVE_VK(vkGetPhysicalDeviceProperties)
	HAM_RESOLVE_VK(vkGetPhysicalDeviceFeatures)
	HAM_RESOLVE_VK(vkGetPhysicalDeviceQueueFamilyProperties)
	HAM_RESOLVE_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
	HAM_RESOLVE_VK(vkGetPhysicalDeviceSurfaceFormatsKHR)
	HAM_RESOLVE_VK(vkGetPhysicalDeviceSurfacePresentModesKHR)
	HAM_RESOLVE_VK(vkGetPhysicalDeviceSurfaceSupportKHR)

	HAM_RESOLVE_VK(vkCreateDevice)
	HAM_RESOLVE_VK(vkDestroyDevice)

#undef HAM_RESOLVE_VK

	if(!ham_renderer_vulkan_init_phys_device(r)){
		ham::logapierror("Error initializing physical device");
		std::destroy_at(r);
		return nullptr;
	}

	if(!ham_renderer_vulkan_init_logical_device(r)){
		ham::logapierror("Error initializing logical device");
		std::destroy_at(r);
		return nullptr;
	}

	r->vma_vk_fns.vkGetInstanceProcAddr = r->vk_fns.vkGetInstanceProcAddr;
	r->vma_vk_fns.vkGetDeviceProcAddr   = r->vk_fns.vkGetDeviceProcAddr;

#define HAM_RESOLVE_VK_DEV(fn_id) \
	r->vk_fns.fn_id = (PFN_##fn_id)r->vk_fns.vkGetDeviceProcAddr(r->vk_dev, #fn_id); \
	if(!r->vk_fns.fn_id){ \
		ham::logapierror("Could not resolve function \"" #fn_id "\""); \
		r->vk_fns.vkDestroyDevice(r->vk_dev, nullptr); \
		std::destroy_at(r); \
		return nullptr; \
	}

	HAM_RESOLVE_VK_DEV(vkDeviceWaitIdle)
	HAM_RESOLVE_VK_DEV(vkGetDeviceQueue)
	HAM_RESOLVE_VK_DEV(vkQueueSubmit)
	HAM_RESOLVE_VK_DEV(vkQueuePresentKHR)

	HAM_RESOLVE_VK_DEV(vkCreateSwapchainKHR)
	HAM_RESOLVE_VK_DEV(vkDestroySwapchainKHR)
	HAM_RESOLVE_VK_DEV(vkGetSwapchainImagesKHR)
	HAM_RESOLVE_VK_DEV(vkAcquireNextImageKHR)

	HAM_RESOLVE_VK_DEV(vkCreateImageView)
	HAM_RESOLVE_VK_DEV(vkDestroyImageView)

	HAM_RESOLVE_VK_DEV(vkCreateShaderModule)
	HAM_RESOLVE_VK_DEV(vkDestroyShaderModule)

	HAM_RESOLVE_VK_DEV(vkCreateRenderPass)
	HAM_RESOLVE_VK_DEV(vkDestroyRenderPass)

	HAM_RESOLVE_VK_DEV(vkCreatePipelineLayout)
	HAM_RESOLVE_VK_DEV(vkDestroyPipelineLayout)
	HAM_RESOLVE_VK_DEV(vkCreateGraphicsPipelines)
	HAM_RESOLVE_VK_DEV(vkDestroyPipeline)

	HAM_RESOLVE_VK_DEV(vkCreateDescriptorSetLayout)
	HAM_RESOLVE_VK_DEV(vkDestroyDescriptorSetLayout)

	HAM_RESOLVE_VK_DEV(vkCreateFramebuffer)
	HAM_RESOLVE_VK_DEV(vkDestroyFramebuffer)

	HAM_RESOLVE_VK_DEV(vkCreateCommandPool)
	HAM_RESOLVE_VK_DEV(vkDestroyCommandPool)

	HAM_RESOLVE_VK_DEV(vkAllocateCommandBuffers)
	HAM_RESOLVE_VK_DEV(vkFreeCommandBuffers)
	HAM_RESOLVE_VK_DEV(vkBeginCommandBuffer)
	HAM_RESOLVE_VK_DEV(vkEndCommandBuffer)
	HAM_RESOLVE_VK_DEV(vkResetCommandBuffer)

	HAM_RESOLVE_VK_DEV(vkCmdPipelineBarrier)
	HAM_RESOLVE_VK_DEV(vkCmdBeginRenderPass)
	HAM_RESOLVE_VK_DEV(vkCmdEndRenderPass)
	HAM_RESOLVE_VK_DEV(vkCmdBindPipeline)
	HAM_RESOLVE_VK_DEV(vkCmdBindVertexBuffers)
	HAM_RESOLVE_VK_DEV(vkCmdBindIndexBuffer)
	HAM_RESOLVE_VK_DEV(vkCmdSetViewport)
	HAM_RESOLVE_VK_DEV(vkCmdSetScissor)
	HAM_RESOLVE_VK_DEV(vkCmdDraw)
	HAM_RESOLVE_VK_DEV(vkCmdDrawIndexed)
	HAM_RESOLVE_VK_DEV(vkCmdDrawIndexedIndirect)

	HAM_RESOLVE_VK_DEV(vkCreateSemaphore)
	HAM_RESOLVE_VK_DEV(vkDestroySemaphore)
	HAM_RESOLVE_VK_DEV(vkCreateFence)
	HAM_RESOLVE_VK_DEV(vkDestroyFence)

	HAM_RESOLVE_VK_DEV(vkWaitForFences)
	HAM_RESOLVE_VK_DEV(vkResetFences)

#undef HAM_RESOLVE_VK_DEV

	r->vk_fns.vkGetDeviceQueue(r->vk_dev, r->vk_graphics_family, 0, &r->vk_graphics_queue);
	if(r->vk_graphics_family != r->vk_present_family){
		r->vk_fns.vkGetDeviceQueue(r->vk_dev, r->vk_present_family, 0, &r->vk_present_queue);
	}
	else{
		r->vk_present_queue = r->vk_graphics_queue;
	}

	ham::indirect_function<void(ham_renderer_vulkan*)> cleanup_fn = [](ham_renderer_vulkan *r){
		r->vk_fns.vkDestroyDevice(r->vk_dev, nullptr);
		std::destroy_at(r);
	};

	VmaAllocatorCreateInfo vma_allocator_info = {};
	vma_allocator_info.vulkanApiVersion = VK_API_VERSION_1_1;
	vma_allocator_info.physicalDevice = r->vk_phys_dev;
	vma_allocator_info.device = r->vk_dev;
	vma_allocator_info.instance = r->vk_inst;
	vma_allocator_info.pVulkanFunctions = &r->vma_vk_fns;

	VkResult vk_res = vmaCreateAllocator(&vma_allocator_info, &r->vma_allocator);
	if(vk_res != VK_SUCCESS){
		ham::logapierror("Error in vmaCreateAllocator: {}", ham_vk_result_str(vk_res));
		cleanup_fn(r);
		return nullptr;
	}

	// add vma to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		vmaDestroyAllocator(r->vma_allocator);
		old(r);
	};

	if(!ham_renderer_vulkan_swapchain_init(r)){
		ham::logapierror("Error initializing swapchain");
		cleanup_fn(r);
		return nullptr;
	}

	// add swapchain to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		ham_renderer_vulkan_swapchain_fini(r);
		old(r);
	};

	if(!ham_renderer_vulkan_init_descriptor_set_layouts(r)){
		ham::logapierror("Error initializing descriptor set layouts");
		cleanup_fn(r);
		return nullptr;
	}

	// add descriptor set layouts to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		ham_renderer_vulkan_fini_descriptor_set_layouts(r);
		old(r);
	};

	if(!ham_renderer_vulkan_init_pipeline_layout(r)){
		ham::logapierror("Error initializing pipeline layout");
		cleanup_fn(r);
		return nullptr;
	}

	// add pipeline layout to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		r->vk_fns.vkDestroyPipelineLayout(r->vk_dev, r->vk_pipeline_layout, nullptr);
		old(r);
	};

	if(!ham_renderer_vulkan_init_render_passes(r)){
		ham::logapierror("Error initializing render passes");
		cleanup_fn(r);
		return nullptr;
	}

	// add render passes to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		r->vk_fns.vkDestroyRenderPass(r->vk_dev, r->vk_render_pass_screen, nullptr);
		old(r);
	};

	if(!ham_renderer_vulkan_init_pipeline(r)){
		ham::logapierror("Error initializing pipeline");
		cleanup_fn(r);
		return nullptr;
	}

	// add pipeline to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		r->vk_fns.vkDestroyPipeline(r->vk_dev, r->vk_pipeline, nullptr);
		old(r);
	};

	if(!ham_renderer_vulkan_init_framebuffers(r)){
		ham::logapierror("Error initializing framebuffers");
		cleanup_fn(r);
		return nullptr;
	}

	// add framebuffers to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		const auto allocator = ham_super(r)->allocator;

		for(ham_u32 i = 0; i < r->vk_num_swapchain_images; i++){
			const auto fb = r->vk_swapchain_framebuffers[i];
			r->vk_fns.vkDestroyFramebuffer(r->vk_dev, fb, nullptr);
		}

		ham_allocator_delete_array(allocator, r->vk_swapchain_framebuffers, r->vk_num_swapchain_images);

		old(r);
	};

	if(!ham_renderer_vulkan_init_command_pools(r)){
		ham::logapierror("Error initializing command pools");
		cleanup_fn(r);
		return nullptr;
	}

	// add command pools to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		r->vk_fns.vkDestroyCommandPool(r->vk_dev, r->vk_cmd_pool, nullptr);
		old(r);
	};

	if(!ham_renderer_vulkan_init_command_buffers(r)){
		ham::logapierror("Error initializing command buffers");
		cleanup_fn(r);
		return nullptr;
	}

	// add command buffers to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		r->vk_fns.vkFreeCommandBuffers(r->vk_dev, r->vk_cmd_pool, ham_impl_max_queued_frames, r->vk_cmd_bufs);
		old(r);
	};

	if(!ham_renderer_vulkan_init_sync_objects(r)){
		ham::logapierror("Error initializing synchronization objects");
		cleanup_fn(r);
		return nullptr;
	}

	// add sync objs to cleanup
	cleanup_fn = [old{std::move(cleanup_fn)}](ham_renderer_vulkan *r){
		for(ham_u32 i = 0; i < ham_impl_max_queued_frames; i++){
			r->vk_fns.vkDestroyFence(r->vk_dev, r->vk_frame_fences[i], nullptr);
			r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_render_sems[i], nullptr);
			r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_img_sems[i], nullptr);
		}
		old(r);
	};

	const ham_shape *const shapes[] = {
		ham_shape_unit_square()
	};

	const ham_image *const images[] = {
		nullptr
	};

	r->screen_draw_group = (ham_draw_group_vulkan*)ham_draw_group_create(ham_super(r), 1, shapes, images);
	if(!r->screen_draw_group){
		ham::logapierror("Error creating screen draw group");
		cleanup_fn(r);
		return nullptr;
	}

	return r;
}

ham_nonnull_args(1)
ham_nothrow static inline void ham_renderer_vulkan_dtor(ham_renderer_vulkan *r){
	r->vk_fns.vkDeviceWaitIdle(r->vk_dev);

	ham_draw_group_destroy(ham_super(r->screen_draw_group));

	for(ham_u32 i = 0; i < ham_impl_max_queued_frames; i++){
		r->vk_fns.vkDestroyFence(r->vk_dev, r->vk_frame_fences[i], nullptr);
		r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_render_sems[i], nullptr);
		r->vk_fns.vkDestroySemaphore(r->vk_dev, r->vk_img_sems[i], nullptr);
	}

	r->vk_fns.vkFreeCommandBuffers(r->vk_dev, r->vk_cmd_pool, ham_impl_max_queued_frames, r->vk_cmd_bufs);

	r->vk_fns.vkDestroyCommandPool(r->vk_dev, r->vk_cmd_pool, nullptr);

	r->vk_fns.vkDestroyPipeline(r->vk_dev, r->vk_pipeline, nullptr);
	r->vk_fns.vkDestroyRenderPass(r->vk_dev, r->vk_render_pass_screen, nullptr);
	r->vk_fns.vkDestroyPipelineLayout(r->vk_dev, r->vk_pipeline_layout, nullptr);

	ham_renderer_vulkan_fini_descriptor_set_layouts(r);
	ham_renderer_vulkan_fini_framebuffers(r);

	ham_renderer_vulkan_swapchain_fini(r);

	vmaDestroyAllocator(r->vma_allocator);

	r->vk_fns.vkDestroyDevice(r->vk_dev, nullptr);

	std::destroy_at(r);
}

//
// Resize
//

ham_nonnull_args(1)
static inline bool ham_renderer_vulkan_resize(ham_renderer_vulkan *r, ham_u32 w, ham_u32 h){
	const u32 cur_w = r->vk_swapchain_extent.width;
	const u32 cur_h = r->vk_swapchain_extent.height;

	if(w != 0 && h != 0){
		r->vk_swapchain_extent = VkExtent2D{ w, h };
	}

	if(!ham_renderer_vulkan_swapchain_reset(r)){
		ham::logapierror("Error recreating swapchain");
		if(w != 0 && h != 0){
			r->vk_swapchain_extent = VkExtent2D{ cur_w, cur_h };
		}
		return false;
	}

	return true;
}

//
// Frames
//

ham_nonnull_args(1)
static inline void ham_renderer_vulkan_frame(ham_renderer_vulkan *r, ham_f64 dt, const ham_renderer_frame_data *data){
	(void)dt;

	vmaSetCurrentFrameIndex(r->vma_allocator, (u32)data->common.current_frame);

	const auto frame_idx = data->common.current_frame % ham_impl_max_queued_frames;

	VkResult res = r->vk_fns.vkWaitForFences(r->vk_dev, 1, &r->vk_frame_fences[frame_idx], VK_TRUE, HAM_U64_MAX);
	if(res != VK_SUCCESS){
		ham::logapierror("Failed to get current frame fence");
		return;
	}

	u32 swap_idx;
	res = r->vk_fns.vkAcquireNextImageKHR(r->vk_dev, r->vk_swapchain, HAM_U64_MAX, r->vk_img_sems[frame_idx], VK_NULL_HANDLE, &swap_idx);
	if(res == VK_ERROR_OUT_OF_DATE_KHR){
		ham_renderer_vulkan_swapchain_reset(r);
		return;
	}
	else if(res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR){
		ham::logapierror("Error in vkAcquireNextImageKHR: {}", ham_vk_result_str(res));
		return;
	}

	res = r->vk_fns.vkResetFences(r->vk_dev, 1, &r->vk_frame_fences[frame_idx]);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vkResetFences: {}", ham_vk_result_str(res));
		return;
	}

	res = r->vk_fns.vkResetCommandBuffer(r->vk_cmd_bufs[frame_idx], 0);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vkResetCommandBuffer: {}", ham_vk_result_str(res));
		return;
	}

	if(!ham_renderer_vulkan_fill_command_buffers(r, frame_idx, swap_idx)){
		ham::logapierror("Failed to fill command buffers");
	}

	constexpr VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit_info{
		VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr,
		1, &r->vk_img_sems[frame_idx],
		wait_stages,
		1, &r->vk_cmd_bufs[frame_idx],
		1, &r->vk_render_sems[frame_idx],
	};

	res = r->vk_fns.vkQueueSubmit(r->vk_graphics_queue, 1, &submit_info, r->vk_frame_fences[frame_idx]);
	if(res != VK_SUCCESS){
		ham::logapierror("Error in vkQueueSubmit: {}", ham_vk_result_str(res));
		return;
	}

	VkPresentInfoKHR present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &r->vk_render_sems[frame_idx],
		.swapchainCount = 1,
		.pSwapchains = &r->vk_swapchain,
		.pImageIndices = &swap_idx,
		.pResults = nullptr,
	};

	res = r->vk_fns.vkQueuePresentKHR(r->vk_present_queue, &present_info);
	if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || r->swapchain_dirty){
		ham_renderer_vulkan_swapchain_reset(r);
	}
	else if(res != VK_SUCCESS){
		ham::logapierror("Error in vkQueuePresentKHR: {}", ham_vk_result_str(res));
		return;
	}
}

static inline bool ham_def_method(ham_renderer_vulkan, add_shader_include, ham_str8 name, ham_str8 src){
	(void)name; (void)src;
	return false;
}

ham_define_renderer(ham_renderer_vulkan, ham_shader_vulkan, ham_draw_group_vulkan, ham_light_group_vulkan)

HAM_C_API_END
