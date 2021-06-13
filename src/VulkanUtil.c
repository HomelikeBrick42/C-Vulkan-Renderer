#if defined(_WIN32) || defined(_WIN64)
	#define VK_USE_PLATFORM_WIN32_KHR
#else
	#error This platform is not supported
#endif

#include "VulkanUtil.h"

#include "Window.h"
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#include "Win32Window.h"
#else
	#error This platform is not supported
#endif

#include <string.h>

static b8 HasRequiredLayers(
	const char** requiredLayers, u32 requiredLayerCount,
	VkLayerProperties* supportedLayers, u32 supportedLayerCount
) {
	for (u32 i = 0; i < requiredLayerCount; i++) {
		b8 hasLayer = false;

		for (u32 j = 0; j < supportedLayerCount; j++) {
			if (strcmp(requiredLayers[i], supportedLayers[j].layerName) == 0) {
				hasLayer = true;
				break;
			}
		}

		if (!hasLayer) {
			return false;
		}
	}

	return true;
}

static b8 HasRequiredExtensions(
	const char** requiredExtension, u32 requiredExtensionCount,
	VkExtensionProperties* supportedExtensions, u32 supportedExtensionCount
) {
	for (u32 i = 0; i < requiredExtensionCount; i++) {
		b8 hasExtension = false;

		for (u32 j = 0; j < supportedExtensionCount; j++) {
			if (strcmp(requiredExtension[i], supportedExtensions[j].extensionName) == 0) {
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension) {
			return false;
		}
	}

	return true;
}

b8 CreateVulkanInstance(
	VkInstance* instance,
	u32 version,
	const char** layers, u32 layerCount,
	const char** extensions, u32 extensionCount
) {
	u32 layerPropertiesCount = 0;
	VkCall(vkEnumerateInstanceLayerProperties(&layerPropertiesCount, NULL));
	VkLayerProperties layerProperties[layerPropertiesCount];
	VkCall(vkEnumerateInstanceLayerProperties(&layerPropertiesCount, layerProperties));
	if (!HasRequiredLayers(layers, layerCount, layerProperties, layerPropertiesCount)) {
		return false;
	}

	u32 extensionPropertiesCount = 0;
	VkCall(vkEnumerateInstanceExtensionProperties(NULL, &extensionPropertiesCount, NULL));
	VkExtensionProperties extensionProperties[extensionPropertiesCount];
	VkCall(vkEnumerateInstanceExtensionProperties(NULL, &extensionPropertiesCount, extensionProperties));
	if (!HasRequiredExtensions(extensions, extensionCount, extensionProperties, extensionPropertiesCount)) {
		return false;
	}

	*instance = VK_NULL_HANDLE;

	VkResult result = vkCreateInstance(&(VkInstanceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &(VkApplicationInfo){
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Test Renderer", // TODO:
			.applicationVersion = VK_MAKE_VERSION(0, 1, 0),
			.pEngineName = "Test Renderer Engine", // TODO:
			.engineVersion = VK_MAKE_VERSION(0, 1, 0),
			.apiVersion = version,
		},
		.enabledLayerCount = layerCount,
		.ppEnabledLayerNames = layers,
		.enabledExtensionCount = extensionCount,
		.ppEnabledExtensionNames = extensions,
	}, NULL, instance);

	return result == VK_SUCCESS && instance != VK_NULL_HANDLE;
}

b8 CreateVulkanSurface(VkSurfaceKHR* surface, VkInstance instance, Window window) {
	*surface = VK_NULL_HANDLE;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VkResult result = vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR){
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = window->Instance,
		.hwnd = window->Handle,
	}, NULL, surface);
#else
	#error This platform is not supported
#endif

	return result == VK_SUCCESS && surface != VK_NULL_HANDLE;
}

static b8 GetQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, u32* graphicsQueueFamilyIndex, u32* presentQueueFamilyIndex) {
	u32 queueFamilyPropertiesCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, NULL);
	VkQueueFamilyProperties queueFamilyProperties[queueFamilyPropertiesCount];
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);

	*graphicsQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	*presentQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	for (u32 i = 0; i < queueFamilyPropertiesCount; i++) {
		if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			*graphicsQueueFamilyIndex = i;

			VkBool32 presentSupport = false;
			VkCall(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport));

			if (presentSupport &&
			#if defined(VK_USE_PLATFORM_WIN32_KHR)
				vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, i)
			#else
				#error This platform is not supported
			#endif
			) {
				*presentQueueFamilyIndex = i;
				break;
			}
		}
	}

	if (*presentQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) {
		for (u32 i = 0; i < queueFamilyPropertiesCount; i++) {
			VkBool32 presentSupport = false;
			VkCall(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport));

			if (presentSupport &&
			#if defined(VK_USE_PLATFORM_WIN32_KHR)
				vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, i)
			#else
				#error This platform is not supported
			#endif
			) {
				*presentQueueFamilyIndex = i;
				break;
			}
		}
	}

	return *graphicsQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED && *presentQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED;
}

b8 PickVulkanPhysicalDevice(
	VkPhysicalDevice* physicalDevice,
	VkInstance instance,
	VkSurfaceKHR surface,
	u32 version,
	const char** layers, u32 layerCount,
	const char** extensions, u32 extensionCount,
	u32* graphicsQueueFamilyIndex,
	u32* presentQueueFamilyIndex
) {
	*physicalDevice = VK_NULL_HANDLE;

	u32 physicalDeviceCount = 0;
	VkCall(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL));
	if (physicalDeviceCount == 0) {
		return false;
	}

	VkPhysicalDevice physicalDevices[physicalDeviceCount];
	VkCall(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

	for (u32 i = 0; i < physicalDeviceCount; i++) {
		u32 layerPropertiesCount = 0;
		VkCall(vkEnumerateDeviceLayerProperties(physicalDevices[i], &layerPropertiesCount, NULL));
		VkLayerProperties layerProperties[layerPropertiesCount];
		VkCall(vkEnumerateDeviceLayerProperties(physicalDevices[i], &layerPropertiesCount, layerProperties));
		if (!HasRequiredLayers(layers, layerCount, layerProperties, layerPropertiesCount)) {
			continue;
		}

		u32 extensionPropertiesCount = 0;
		VkCall(vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &extensionPropertiesCount, NULL));
		VkExtensionProperties extensionProperties[extensionPropertiesCount];
		VkCall(vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &extensionPropertiesCount, extensionProperties));
		if (!HasRequiredExtensions(extensions, extensionCount, extensionProperties, extensionPropertiesCount)) {
			continue;
		}

		u32 tempGraphicsQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		u32 tempPresentQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		if (!GetQueueFamilies(physicalDevices[i], surface, &tempGraphicsQueueFamilyIndex, &tempPresentQueueFamilyIndex)) {
			continue;
		}

		VkPhysicalDeviceProperties properties = {};
		vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);
		if (properties.apiVersion < version) {
			continue;
		}

		*physicalDevice = physicalDevices[i];
		*graphicsQueueFamilyIndex = tempGraphicsQueueFamilyIndex;
		*presentQueueFamilyIndex = tempPresentQueueFamilyIndex;

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			break;
		}
	}

	return *physicalDevice != VK_NULL_HANDLE;
}

b8 CreateVulkanDevice(
	VkDevice* device,
	VkPhysicalDevice physicalDevice,
	const char** layers, u32 layerCount,
	const char** extensions, u32 extensionCount,
	u32 graphicsQueueFamilyIndex,
	u32 presentQueueFamilyIndex
) {
	*device = VK_NULL_HANDLE;

	VkResult result = vkCreateDevice(physicalDevice, &(VkDeviceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = graphicsQueueFamilyIndex == presentQueueFamilyIndex ? 1 : 2,
		.pQueueCreateInfos = (VkDeviceQueueCreateInfo[2]){
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = graphicsQueueFamilyIndex,
				.queueCount = 1,
				.pQueuePriorities = (float[1]){ 1.0f },
			},
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = presentQueueFamilyIndex,
				.queueCount = 1,
				.pQueuePriorities = (float[1]){ 1.0f },
			}
		},
		.enabledLayerCount = layerCount,
		.ppEnabledLayerNames = layers,
		.enabledExtensionCount = extensionCount,
		.ppEnabledExtensionNames = extensions,
	}, NULL, device);

	return result == VK_SUCCESS && device != VK_NULL_HANDLE;
}
