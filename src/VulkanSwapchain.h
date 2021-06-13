#pragma once

#include "Typedefs.h"
#include "Window.h"

#include <vulkan/vulkan.h>

typedef struct VulkanSwapchain_t {
	VkSwapchainKHR Swapchain;
	VkPhysicalDevice PhysicalDevice;
	VkDevice Device;
	VkSurfaceKHR Surface;
	VkRenderPass RenderPass;
	Window Window;
	u32 GraphicsQueueFamilyIndex;
	u32 PresentQueueFamilyIndex;

	u32 ImageCount;
	VkImage* Images;
	VkImageView* ImageViews;
	VkFramebuffer* Framebuffers;

	VkSurfaceFormatKHR Format;
	VkPresentModeKHR PresentMode;
	VkExtent2D Extent;
	VkSurfaceTransformFlagsKHR Transform;
	VkCompositeAlphaFlagsKHR CompositeAlpha;
} VulkanSwapchain;

// HACK: VulkanSwapchain::Swapchain must always be zero when called externally
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
);

void VulkanSwapchain_Destroy(VulkanSwapchain* swapchain);

b8 VulkanSwapchain_TryResize(VulkanSwapchain* swapchain);
