#include "VulkanSwapchain.h"
#include "VulkanUtil.h"

#include <stdlib.h>

static b8 ChoosePresentMode(VkPresentModeKHR* presentMode, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
	u32 presentModeCount = 0;
	VkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL));
	if (presentModeCount == 0) {
		return false;
	}

	VkPresentModeKHR presentModes[presentModeCount];
	VkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes));

	for (u64 i = 0; i < presentModeCount; i++) {
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			*presentMode = presentModes[i];
			return true;
		}
	}

	*presentMode = VK_PRESENT_MODE_FIFO_KHR; // NOTE: Always availiable
	return true;
}

b8 VulkanSwapchain_Create(
	VulkanSwapchain* swapchain,
	VkPhysicalDevice physicalDevice,
	VkDevice device,
	VkSurfaceKHR surface,
	VkRenderPass renderPass,
	VkSurfaceFormatKHR format,
	Window window,
	u32 graphicsQueueFamilyIndex,
	u32 presentQueueFamilyIndex,
	u32 width,
	u32 height
) {
	if (!swapchain->Swapchain) {
		swapchain->Swapchain = VK_NULL_HANDLE;
	}
	swapchain->PhysicalDevice = physicalDevice;
	swapchain->Device = device;
	swapchain->Surface = surface;
	swapchain->RenderPass = renderPass;
	swapchain->Format = format;
	swapchain->Window = window;
	swapchain->GraphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
	swapchain->PresentQueueFamilyIndex = presentQueueFamilyIndex;

	if (!ChoosePresentMode(&swapchain->PresentMode, swapchain->PhysicalDevice, swapchain->Surface)) {
		return false;
	}

	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
	VkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

	swapchain->CompositeAlpha =
		(surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
		: (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
		: (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
		: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	swapchain->ImageCount = surfaceCapabilities.minImageCount + 1;
	if (swapchain->ImageCount > surfaceCapabilities.maxImageCount) {
		swapchain->ImageCount = surfaceCapabilities.maxImageCount;
	}

	if (surfaceCapabilities.currentExtent.width != ~0u && surfaceCapabilities.currentExtent.height != ~0u) {
		swapchain->Extent = surfaceCapabilities.currentExtent;
	} else {
		u32 width = 0, height = 0;
		Window_GetSize(window, &width, &height);

		if (width > surfaceCapabilities.maxImageExtent.width) {
			width = surfaceCapabilities.maxImageExtent.width;
		} else if (width < surfaceCapabilities.minImageExtent.width) {
			width = surfaceCapabilities.minImageExtent.width;
		}
		
		if (height > surfaceCapabilities.maxImageExtent.height) {
			height = surfaceCapabilities.maxImageExtent.height;
		} else if (height < surfaceCapabilities.minImageExtent.height) {
			height = surfaceCapabilities.minImageExtent.height;
		}

		swapchain->Extent = (VkExtent2D){
			.width = width,
			.height = height
		};
	}

	swapchain->Transform = surfaceCapabilities.currentTransform;

	VkResult result = vkCreateSwapchainKHR(device, &(VkSwapchainCreateInfoKHR){
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = swapchain->ImageCount,
		.imageFormat = swapchain->Format.format,
		.imageColorSpace = swapchain->Format.colorSpace,
		.imageExtent = swapchain->Extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = swapchain->GraphicsQueueFamilyIndex != swapchain->PresentQueueFamilyIndex ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = swapchain->GraphicsQueueFamilyIndex != swapchain->PresentQueueFamilyIndex ? 2 : 1,
		.pQueueFamilyIndices = (u32[2]){ swapchain->GraphicsQueueFamilyIndex, swapchain->PresentQueueFamilyIndex },
		.preTransform = swapchain->Transform,
		.compositeAlpha = swapchain->CompositeAlpha,
		.presentMode = swapchain->PresentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = swapchain->Swapchain,
	}, NULL, &swapchain->Swapchain);

	if (result != VK_SUCCESS || swapchain->Swapchain == VK_NULL_HANDLE) {
		return false;
	}

	VkCheck(vkGetSwapchainImagesKHR(device, swapchain->Swapchain, &swapchain->ImageCount, NULL));
	if (swapchain->ImageCount == 0) {
		return false;
	}

	swapchain->Images = malloc(swapchain->ImageCount * sizeof(swapchain->Images[0]));
	for (u32 i = 0; i < swapchain->ImageCount; i++) {
		swapchain->Images[i] = VK_NULL_HANDLE;
	}

	VkCheck(vkGetSwapchainImagesKHR(device, swapchain->Swapchain, &swapchain->ImageCount, swapchain->Images));
	for (u32 i = 0; i < swapchain->ImageCount; i++) {
		if (swapchain->Images[i] == VK_NULL_HANDLE) {
			return false;
		}
	}

	swapchain->ImageViews = malloc(swapchain->ImageCount * sizeof(swapchain->ImageViews[0]));
	for (u32 i = 0; i < swapchain->ImageCount; i++) {
		swapchain->ImageViews[i] = VK_NULL_HANDLE;

		VkCheck(vkCreateImageView(device, &(VkImageViewCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain->Images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain->Format.format,
			.subresourceRange = (VkImageSubresourceRange){
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			},
		}, NULL, &swapchain->ImageViews[i]));

		if (swapchain->ImageViews[i] == VK_NULL_HANDLE) {
			return false;
		}
	}

	swapchain->Framebuffers = malloc(swapchain->ImageCount * sizeof(swapchain->Framebuffers[0]));
	for (u32 i = 0; i < swapchain->ImageCount; i++) {
		swapchain->Framebuffers[i] = VK_NULL_HANDLE;

		VkResult result = (vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = swapchain->RenderPass,
			.attachmentCount = 1,
			.pAttachments = &swapchain->ImageViews[i],
			.width = swapchain->Extent.width,
			.height = swapchain->Extent.height,
			.layers = 1,
		}, NULL, &swapchain->Framebuffers[i]));

		if (swapchain->Framebuffers[i] == VK_NULL_HANDLE) {
			return false;
		}
	}

	return true;
}

void VulkanSwapchain_Destroy(VulkanSwapchain* swapchain) {
	for (u32 i = 0; i < swapchain->ImageCount; i++) {
		vkDestroyFramebuffer(swapchain->Device, swapchain->Framebuffers[i], NULL);
	}
	free(swapchain->Framebuffers);
	
	for (u32 i = 0; i < swapchain->ImageCount; i++) {
		vkDestroyImageView(swapchain->Device, swapchain->ImageViews[i], NULL);
	}
	free(swapchain->ImageViews);

	vkDestroySwapchainKHR(swapchain->Device, swapchain->Swapchain, NULL);
}

b8 VulkanSwapchain_TryResize(VulkanSwapchain* swapchain) {
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
	VkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(swapchain->PhysicalDevice, swapchain->Surface, &surfaceCapabilities));

	if (swapchain->Extent.width == surfaceCapabilities.currentExtent.width && swapchain->Extent.height == surfaceCapabilities.currentExtent.height) {
		return false;
	}

	VulkanSwapchain oldSwapchain = *swapchain;

	if (!VulkanSwapchain_Create(
		swapchain,
		swapchain->PhysicalDevice,
		swapchain->Device,
		swapchain->Surface,
		swapchain->RenderPass,
		swapchain->Format,
		swapchain->Window,
		swapchain->GraphicsQueueFamilyIndex,
		swapchain->PresentQueueFamilyIndex,
		surfaceCapabilities.currentExtent.width,
		surfaceCapabilities.currentExtent.height)
	) {
		return false;
	}

	VkCheck(vkDeviceWaitIdle(swapchain->Device));

	VulkanSwapchain_Destroy(&oldSwapchain);

	return true;
}
