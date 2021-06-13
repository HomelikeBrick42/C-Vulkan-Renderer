#pragma once

#include "Typedefs.h"
#include "Window.h"

#include <vulkan/vulkan.h>

#define VkCall(x) ASSERT((x) == VK_SUCCESS)

b8 CreateVulkanInstance(
	VkInstance* instance,
	u32 version,
	const char** layers, u32 layerCount,
	const char** extensions, u32 extensionCount
);

b8 CreateVulkanSurface(VkSurfaceKHR* surface, VkInstance instance, Window window);

b8 PickVulkanPhysicalDevice(
	VkPhysicalDevice* physicalDevice,
	VkInstance instance,
	VkSurfaceKHR surface,
	u32 version,
	const char** layers, u32 layerCount,
	const char** extensions, u32 extensionCount,
	u32* graphicsQueueFamilyIndex,
	u32* presentQueueFamilyIndex
);

b8 CreateVulkanDevice(
	VkDevice* device,
	VkPhysicalDevice physicalDevice,
	const char** layers, u32 layerCount,
	const char** extensions, u32 extensionCount,
	u32 graphicsQueueFamilyIndex,
	u32 presentQueueFamilyIndex
);
