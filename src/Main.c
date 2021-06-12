#include "./Typedefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
	#define PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR
#else
	#error This platform is not supported
#endif

#include <vulkan/vulkan.h>

#define VkCall(x) ASSERT((x) == VK_SUCCESS)

#if defined(PLATFORM_WINDOWS)

#include <Windows.h>

typedef struct {
	HMODULE Instance;
	HWND Handle;
} *Window;

LRESULT Win32WindowCallback(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;

	Window window = cast(Window) GetWindowLongPtrA(windowHandle, GWLP_USERDATA);
	if (window) {
		switch (message) {
			case WM_CLOSE:
			case WM_QUIT: {
				PostQuitMessage(0);
			} break;

			default: {
				result = DefWindowProcA(window->Handle, message, wParam, lParam);
			} break;
		}
	} else {
		result = DefWindowProcA(windowHandle, message, wParam, lParam);
	}

	return result;
}

static b8 WindowClassInitialized = false;
Window WindowCreate(u32 width, u32 height, const char* title) {
	const char* WindowClassName = "TestRendererWindowClassName";

	Window window = calloc(1, sizeof(*window));
	window->Instance = GetModuleHandle(NULL);

	if (!WindowClassInitialized) {
		WNDCLASSEXA windowClass = {
			.cbSize = sizeof(windowClass),
			.lpfnWndProc = Win32WindowCallback,
			.hInstance = window->Instance,
			.hCursor = LoadCursor(NULL, IDC_ARROW),
			.lpszClassName = WindowClassName,
		};

		if (!RegisterClassExA(&windowClass)) {
			free(window);
			return NULL;
		}

		WindowClassInitialized = true;
	}

	const DWORD windowStyle = WS_OVERLAPPEDWINDOW;

	RECT windowRect = {};
	windowRect.left = 100;
	windowRect.right = windowRect.left + width;
	windowRect.top = 100;
	windowRect.bottom = windowRect.top + height;

	AdjustWindowRect(&windowRect, windowStyle, false);

	u32 windowWidth = windowRect.right - windowRect.left;
	u32 windowHeight = windowRect.bottom - windowRect.top;

	window->Handle = CreateWindowExA(
		0,
		WindowClassName,
		title,
		windowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowWidth,
		windowHeight,
		NULL,
		NULL,
		window->Instance,
		NULL
	);

	if (!window->Handle) {
		free(window);
		return NULL;
	}

	SetWindowLongPtrA(window->Handle, GWLP_USERDATA, (LONG_PTR)window);

	return window;
}

void WindowDestroy(Window window) {
	DestroyWindow(window->Handle);
	free(window);
}

void WindowShow(Window window) {
	ShowWindow(window->Handle, SW_SHOW);
}

void WindowGetSize(Window window, u32* width, u32* height) {
	RECT clientRect = {};
	GetClientRect(window->Handle, &clientRect);

	if (width) {
		*width = cast(u32) (clientRect.right - clientRect.left);
	}

	if (height) {
		*height = cast(u32) (clientRect.bottom - clientRect.top);
	}
}

b8 WindowPollEvents() {
	MSG message;
	while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			return false;
		}

		TranslateMessage(&message);
		DispatchMessageA(&message);
	}

	return true;
}

#else
	#error This platform is not supported
#endif

#if defined(_DEBUG)

VkBool32 VKAPI_CALL DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	const char* flagString =
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) ? "Verbose" :
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "Info" :
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "Warning" :
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "Error" :
		"Unknown Type";

	printf("%s: %s\n", flagString, pCallbackData->pMessage);

	ASSERT(!(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT));
	return VK_TRUE;
}

#endif

int main(int argc, char** argv) {
	const u32 RequiredVulkanAPIVersion = VK_API_VERSION_1_2;

	{
		u32 apiVersion = 0;
		VkCall(vkEnumerateInstanceVersion(&apiVersion));

		if (apiVersion < RequiredVulkanAPIVersion) {
			printf(
				"This renderer requires a vulkan api version >= 1.2.0\nYou have %d.%d.%d\n",
				VK_VERSION_MAJOR(apiVersion),
				VK_VERSION_MINOR(apiVersion),
				VK_VERSION_PATCH(apiVersion)
			);
			ASSERT(false);
			return -1;
		}
	}

	const char* const InstanceLayers[] = {
	#if defined(_DEBUG)
		"VK_LAYER_KHRONOS_validation",
	#endif
	};

	const char* const InstanceExtentions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
	#if defined(PLATFORM_WINDOWS)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	#else
		#error This platform is not supported
	#endif
	#if defined(_DEBUG)
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	#endif
	};

	VkInstance instance = NULL;
	{
		b8 hasLayers = true;
		{
			u32 layerPropertiesCount = 0;
			VkCall(vkEnumerateInstanceLayerProperties(&layerPropertiesCount, NULL));
			VkLayerProperties layerProperties[layerPropertiesCount];
			VkCall(vkEnumerateInstanceLayerProperties(&layerPropertiesCount, layerProperties));

			for (u64 i = 0; i < sizeof(InstanceLayers) / sizeof(InstanceLayers[0]); i++) {
				b8 found = false;

				for (u64 j = 0; j < layerPropertiesCount; j++) {
					if (strcmp(InstanceLayers[i], layerProperties[j].layerName) == 0) {
						found = true;
						break;
					}
				}

				if (!found) {
					hasLayers = false;
					break;
				}
			}
		}
		ASSERT(hasLayers);

		b8 hasExtentions = true;
		{
			u32 extensionPropertiesCount = 0;
			VkCall(vkEnumerateInstanceExtensionProperties(NULL, &extensionPropertiesCount, NULL));
			VkExtensionProperties extensionProperties[extensionPropertiesCount];
			VkCall(vkEnumerateInstanceExtensionProperties(NULL, &extensionPropertiesCount, extensionProperties));

			for (u64 i = 0; i < sizeof(InstanceExtentions) / sizeof(InstanceExtentions[0]); i++) {
				b8 found = false;

				for (u64 j = 0; j < extensionPropertiesCount; j++) {
					if (strcmp(InstanceExtentions[i], extensionProperties[j].extensionName) == 0) {
						found = true;
						break;
					}
				}

				if (!found) {
					hasExtentions = false;
					break;
				}
			}
		}
		ASSERT(hasExtentions);

		VkCall(vkCreateInstance(&(VkInstanceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &(VkApplicationInfo){
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = "Test Renderer",
				.applicationVersion = VK_MAKE_VERSION(0, 1, 0),
				.pEngineName = "Test Renderer Engine",
				.engineVersion = VK_MAKE_VERSION(0, 1, 0),
				.apiVersion = RequiredVulkanAPIVersion,
			},
			.enabledLayerCount = sizeof(InstanceLayers) / sizeof(InstanceLayers[0]),
			.ppEnabledLayerNames = InstanceLayers,
			.enabledExtensionCount = sizeof(InstanceExtentions) / sizeof(InstanceExtentions[0]),
			.ppEnabledExtensionNames = InstanceExtentions,
		}, NULL, &instance));
	}
	ASSERT(instance);

#if defined(_DEBUG)
	VkDebugUtilsMessengerEXT debugMessenger = NULL;
	{
		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = cast(PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		ASSERT(vkCreateDebugReportCallbackEXT);

		VkCall(vkCreateDebugUtilsMessengerEXT(instance, &(VkDebugUtilsMessengerCreateInfoEXT){
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
    							|/*VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
    							|*/VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
    							| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = &DebugMessengerCallback,
		}, NULL, &debugMessenger));
	}
	ASSERT(debugMessenger);
#endif

	Window window = NULL;
	{
		window = WindowCreate(1280, 720, "Test Renderer");
	}
	ASSERT(window);

	WindowShow(window);

	VkSurfaceKHR surface = NULL;
	{
	#if defined(PLATFORM_WINDOWS)
		VkCall(vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR){
			.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.hinstance = window->Instance,
			.hwnd = window->Handle,
		}, NULL, &surface));
	#else
		#error This platform is not supported
	#endif
	}
	ASSERT(surface);

	const char* const DeviceLayers[] = {
	};

	const char* const DeviceExtentions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	VkPhysicalDevice physicalDevice = NULL;
	u32 graphicsQueueFamilyIndex = cast(u32) -1;
	u32 presentQueueFamilyIndex = cast(u32) -1;
	{
		u32 physicalDeviceCount = 0;
		VkCall(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL));
		VkPhysicalDevice physicalDevices[physicalDeviceCount];
		VkCall(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

		for (u64 i = 0; i < physicalDeviceCount; i++) {
			VkPhysicalDeviceProperties properties = {};
			vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

			if (properties.apiVersion >= RequiredVulkanAPIVersion) {
				b8 hasLayers = true;
				{
					u32 layerPropertiesCount = 0;
					VkCall(vkEnumerateDeviceLayerProperties(physicalDevices[i], &layerPropertiesCount, NULL));
					VkLayerProperties layerProperties[layerPropertiesCount];
					VkCall(vkEnumerateDeviceLayerProperties(physicalDevices[i], &layerPropertiesCount, layerProperties));

					for (u64 i = 0; i < sizeof(DeviceLayers) / sizeof(DeviceLayers[0]); i++) {
						b8 found = false;

						for (u64 j = 0; j < layerPropertiesCount; j++) {
							if (strcmp(DeviceLayers[i], layerProperties[j].layerName) == 0) {
								found = true;
								break;
							}
						}

						if (!found) {
							hasLayers = false;
							break;
						}
					}
				}

				b8 hasExtentions = true;
				{
					u32 extensionPropertiesCount = 0;
					VkCall(vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &extensionPropertiesCount, NULL));
					VkExtensionProperties extensionProperties[extensionPropertiesCount];
					VkCall(vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &extensionPropertiesCount, extensionProperties));

					for (u64 i = 0; i < sizeof(DeviceExtentions) / sizeof(DeviceExtentions[0]); i++) {
						b8 found = false;

						for (u64 j = 0; j < extensionPropertiesCount; j++) {
							if (strcmp(DeviceExtentions[i], extensionProperties[j].extensionName) == 0) {
								found = true;
								break;
							}
						}

						if (!found) {
							hasExtentions = false;
							break;
						}
					}
				}

				u32 tempGraphicsQueueFamilyIndex = cast(u32) -1;
				u32 tempPresentQueueFamilyIndex = cast(u32) -1;
				{
					u32 queueFamilyPropertiesCount = 0;
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertiesCount, NULL);
					VkQueueFamilyProperties queueFamilyProperties[queueFamilyPropertiesCount];
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertiesCount, queueFamilyProperties);

					for (u64 i = 0; i < queueFamilyPropertiesCount; i++) {
						if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
							tempGraphicsQueueFamilyIndex = i;

							VkBool32 presentSupport = false;
							VkCall(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], i, surface, &presentSupport));
							if (presentSupport) {
								tempPresentQueueFamilyIndex = i;
								break;
							}
						}
					}

					if (tempPresentQueueFamilyIndex == cast(u32) -1) {
						for (u64 i = 0; i < queueFamilyPropertiesCount; i++) {
							VkBool32 presentSupport = false;
							VkCall(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], i, surface, &presentSupport));
							if (presentSupport) {
								tempPresentQueueFamilyIndex = i;
								break;
							}
						}
					}
				}

				if (hasLayers &&
					hasExtentions &&
					tempGraphicsQueueFamilyIndex != cast(u32) -1 &&
					tempPresentQueueFamilyIndex != cast(u32) -1
				) {
					physicalDevice = physicalDevices[i];
					graphicsQueueFamilyIndex = tempGraphicsQueueFamilyIndex;
					presentQueueFamilyIndex = tempPresentQueueFamilyIndex;

					if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
						break;
					}
				}
			}
		}
	}
	ASSERT(physicalDevice);
	ASSERT(graphicsQueueFamilyIndex != cast(u32) -1);
	ASSERT(presentQueueFamilyIndex != cast(u32) -1);

	VkDevice device = NULL;
	{
		VkCall(vkCreateDevice(physicalDevice, &(VkDeviceCreateInfo){
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
			.enabledLayerCount = sizeof(DeviceLayers) / sizeof(DeviceLayers[0]),
			.ppEnabledLayerNames = DeviceLayers,
			.enabledExtensionCount = sizeof(DeviceExtentions) / sizeof(DeviceExtentions[0]),
			.ppEnabledExtensionNames = DeviceExtentions,
		}, NULL, &device));
	}
	ASSERT(device);

	VkQueue graphicsQueue = NULL;
	VkQueue presentQueue = NULL;
	{
		vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
		vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
	}
	ASSERT(graphicsQueue);
	ASSERT(presentQueue);

	VkSwapchainKHR swapchain = NULL;
	VkSurfaceFormatKHR swapchainFormat = {};
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkExtent2D swapchainExtent = {};
	{
		{
			u32 surfaceFormatCount = 0;
			VkCall(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, NULL));
			VkSurfaceFormatKHR surfaceFormats[surfaceFormatCount];
			VkCall(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats));

			b8 found = false;
			for (u64 i = 0; i < surfaceFormatCount; i++) {
				if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					swapchainFormat = surfaceFormats[i];
					found = true;
					break;
				}
			}

			if (!found) {
				swapchainFormat = surfaceFormats[0];
			}
		}

		{
			u32 presentModeCount = 0;
			VkCall(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL));
			VkPresentModeKHR presentModes[presentModeCount];
			VkCall(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes));

			for (u64 i = 0; i < presentModeCount; i++) {
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
					swapchainPresentMode = presentModes[i];
					break;
				}
			}
		}

		u32 imageCount = 0;
		VkSurfaceTransformFlagBitsKHR transform = 0;
		{
			VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
			VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

			if (surfaceCapabilities.currentExtent.width != ~0u && surfaceCapabilities.currentExtent.height != ~0u) {
				swapchainExtent = surfaceCapabilities.currentExtent;
			} else {
				u32 width = 0, height = 0;
				WindowGetSize(window, &width, &height);

				width = max(surfaceCapabilities.minImageExtent.width, min(surfaceCapabilities.maxImageExtent.width, width));
				height = max(surfaceCapabilities.minImageExtent.height, min(surfaceCapabilities.maxImageExtent.height, height));

				swapchainExtent = (VkExtent2D){
					.width = width,
					.height = height
				};
			}

			imageCount = surfaceCapabilities.minImageCount + 1;
			imageCount = min(surfaceCapabilities.maxImageCount, imageCount);

			transform = surfaceCapabilities.currentTransform;
		}

		VkCall(vkCreateSwapchainKHR(device, &(VkSwapchainCreateInfoKHR){
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = imageCount,
			.imageFormat = swapchainFormat.format,
			.imageColorSpace = swapchainFormat.colorSpace,
			.imageExtent = swapchainExtent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = graphicsQueueFamilyIndex != presentQueueFamilyIndex ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = graphicsQueueFamilyIndex != presentQueueFamilyIndex ? 2 : 1,
			.pQueueFamilyIndices = (u32[2]){ graphicsQueueFamilyIndex, presentQueueFamilyIndex },
			.preTransform = transform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = swapchainPresentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = swapchain,
		}, NULL, &swapchain));
	}
	ASSERT(swapchain);

	u32 swapchainImageCount = 0;
	VkCall(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL));
	VkImage swapchainImages[swapchainImageCount];
	VkCall(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages));

	u32 swapchainImageViewCount = swapchainImageCount;
	VkImageView swapchainImageViews[swapchainImageViewCount];
	for (u64 i = 0; i < swapchainImageViewCount; i++) {
		VkCall(vkCreateImageView(device, &(VkImageViewCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchainFormat.format,
			.subresourceRange = (VkImageSubresourceRange){
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			},
		}, NULL, &swapchainImageViews[i]));

		ASSERT(swapchainImageViews[i]);
	}

	VkSemaphore imageAvailableSemaphore = NULL;
	VkSemaphore renderFinishedSemaphore = NULL;
	{
		VkCall(vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		}, NULL, &imageAvailableSemaphore));

		VkCall(vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		}, NULL, &renderFinishedSemaphore));
	}
	ASSERT(imageAvailableSemaphore);
	ASSERT(renderFinishedSemaphore);

	VkCommandPool graphicsCommandPool = NULL;
	{
		VkCall(vkCreateCommandPool(device, &(VkCommandPoolCreateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = graphicsQueueFamilyIndex,
		}, NULL, &graphicsCommandPool));
	}
	ASSERT(graphicsCommandPool);

	VkCommandBuffer graphicsCommandBuffer = NULL;
	{
		VkCall(vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = graphicsCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		}, &graphicsCommandBuffer));
	}
	ASSERT(graphicsCommandBuffer);

	VkRenderPass renderPass = NULL;
	{
		VkCall(vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &(VkAttachmentDescription){
				.format = swapchainFormat.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			},
			.subpassCount = 1,
			.pSubpasses = &(VkSubpassDescription){
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 1,
				.pColorAttachments = &(VkAttachmentReference){
					.attachment = 0,
    				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				},
			},
		}, NULL, &renderPass));
	}
	ASSERT(renderPass);

	u32 swapchainFramebufferCount = swapchainImageCount;
	VkFramebuffer swapchainFramebuffers[swapchainFramebufferCount];
	for (u64 i = 0; i < swapchainFramebufferCount; i++) {
		VkCall(vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = 1,
			.pAttachments = &swapchainImageViews[i],
			.width = swapchainExtent.width,
			.height = swapchainExtent.height,
			.layers = 1,
		}, NULL, &swapchainFramebuffers[i]));

		ASSERT(swapchainFramebuffers[i]);
	}

	VkShaderModule vertexShader = NULL;
	{
		FILE* file = fopen("triangle.vert.spirv", "rb");
		ASSERT(file);

		fseek(file, 0, SEEK_END);
		u64 length = ftell(file);
		ASSERT(length > 0);
		fseek(file, 0, SEEK_SET);

		u8 code[length];
		ASSERT(fread(code, sizeof(u8), length, file) == length);
		fclose(file);

		VkCall(vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = length,
			.pCode = cast(u32*) code,
		}, NULL, &vertexShader));
	}
	ASSERT(vertexShader);

	VkShaderModule fragmentShader = NULL;
	{
		FILE* file = fopen("triangle.frag.spirv", "rb");
		ASSERT(file);

		fseek(file, 0, SEEK_END);
		u64 length = ftell(file);
		ASSERT(length > 0);
		fseek(file, 0, SEEK_SET);

		u8 code[length];
		ASSERT(fread(code, sizeof(u8), length, file) == length);
		fclose(file);

		VkCall(vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = length,
			.pCode = cast(u32*) code,
		}, NULL, &fragmentShader));
	}
	ASSERT(fragmentShader);

	VkPipelineLayout trianglePipelineLayout = NULL;
	{
		VkCall(vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		}, NULL, &trianglePipelineLayout));
	}
	ASSERT(trianglePipelineLayout);

	VkPipelineCache trianglePipelineCache = NULL;
	{
		VkCall(vkCreatePipelineCache(device, &(VkPipelineCacheCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		}, NULL, &trianglePipelineCache));
	}
	ASSERT(trianglePipelineCache);

	VkPipeline trianglePipeline = NULL;
	{
		VkCall(vkCreateGraphicsPipelines(device, trianglePipelineCache, 1, &(VkGraphicsPipelineCreateInfo){
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = (VkPipelineShaderStageCreateInfo[2]){
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.module = vertexShader,
					.pName = "main",
				},
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.module = fragmentShader,
					.pName = "main",
				},
			},
			.pVertexInputState = &(VkPipelineVertexInputStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			},
			.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			},
			.pViewportState = &(VkPipelineViewportStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    			.viewportCount = 1,
    			.scissorCount = 1,
			},
			.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.frontFace = VK_FRONT_FACE_CLOCKWISE,
				.lineWidth = 1.0f,
			},
			.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			},
			.pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			},
			.pColorBlendState = &(VkPipelineColorBlendStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.attachmentCount = 1,
				.pAttachments = &(VkPipelineColorBlendAttachmentState){
					.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
				},
			},
			.pDynamicState = &(VkPipelineDynamicStateCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				.dynamicStateCount = 2,
				.pDynamicStates = (VkDynamicState[2]){
					VK_DYNAMIC_STATE_VIEWPORT,
					VK_DYNAMIC_STATE_SCISSOR,
				},
			},
			.layout = trianglePipelineLayout,
			.renderPass = renderPass,
		}, NULL, &trianglePipeline));
	}
	ASSERT(trianglePipeline);

	while (WindowPollEvents()) {
		u32 swapchainImageIndex = 0;
		VkCall(vkAcquireNextImageKHR(device, swapchain, ~0ull, imageAvailableSemaphore, NULL, &swapchainImageIndex));

		VkCall(vkResetCommandPool(device, graphicsCommandPool, 0));

		VkCall(vkBeginCommandBuffer(graphicsCommandBuffer, &(VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		}));

		const VkClearColorValue ClearColor = (VkClearColorValue){
			{ 0.9f, 0.3f, 0.1f, 1.0f },
		};

		const VkClearValue Clear = (VkClearValue){
			.color = ClearColor,
		};

		vkCmdBeginRenderPass(graphicsCommandBuffer, &(VkRenderPassBeginInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = renderPass,
			.framebuffer = swapchainFramebuffers[swapchainImageIndex],
			.renderArea = (VkRect2D){
				.offset = (VkOffset2D){ .x = 0, .y = 0 },
				.extent = swapchainExtent,
			},
			.clearValueCount = 1,
			.pClearValues = &Clear,
		}, VK_SUBPASS_CONTENTS_INLINE);

		u32 windowWidth = 0, windowHeight = 0; // TODO: Do not get this every frame
		WindowGetSize(window, &windowWidth, &windowHeight);

		vkCmdSetViewport(graphicsCommandBuffer, 0, 1, &(VkViewport){
			.x = 0.0f,
			.y = windowHeight,
			.width = windowWidth,
			.height = -cast(float) windowHeight,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		});
		vkCmdSetScissor(graphicsCommandBuffer, 0, 1, &(VkRect2D){
			.offset = (VkOffset2D){
				.x = 0,
				.y = 0,
			},
			.extent = (VkExtent2D){
				.width = windowWidth,
				.height = windowHeight,
			},
		});

		vkCmdBindPipeline(graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
		vkCmdDraw(graphicsCommandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(graphicsCommandBuffer);

		VkCall(vkEndCommandBuffer(graphicsCommandBuffer));

		VkCall(vkQueueSubmit(graphicsQueue, 1, &(VkSubmitInfo){
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &imageAvailableSemaphore,
			.pWaitDstStageMask = (VkPipelineStageFlags[1]){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }, // HACK: Array of length 1 allows me to take the address of the flags inline
			.commandBufferCount = 1,
			.pCommandBuffers = &graphicsCommandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &renderFinishedSemaphore,
		}, NULL));

		VkCall(vkQueuePresentKHR(presentQueue, &(VkPresentInfoKHR){
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &renderFinishedSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &swapchain,
			.pImageIndices = &swapchainImageIndex,
		}));

		VkCall(vkDeviceWaitIdle(device));
	}

	VkCall(vkDeviceWaitIdle(device));
	{
		vkDestroyPipelineLayout(device, trianglePipelineLayout, NULL);
		vkDestroyPipelineCache(device, trianglePipelineCache, NULL);
		vkDestroyPipeline(device, trianglePipeline, NULL);

		vkDestroyShaderModule(device, fragmentShader, NULL);
		vkDestroyShaderModule(device, vertexShader, NULL);

		for (u64 i = 0; i < swapchainFramebufferCount; i++) {
			vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
		}

		vkDestroyRenderPass(device, renderPass, NULL);

		vkDestroyCommandPool(device, graphicsCommandPool, NULL);

		vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
		vkDestroySemaphore(device, renderFinishedSemaphore, NULL);

		for (u64 i = 0; i < swapchainImageViewCount; i++) {
			vkDestroyImageView(device, swapchainImageViews[i], NULL);
		}

		vkDestroySwapchainKHR(device, swapchain, NULL);
	}
	vkDestroyDevice(device, NULL);

	vkDestroySurfaceKHR(instance, surface, NULL);
	WindowDestroy(window);

#if defined(_DEBUG)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = cast(PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		ASSERT(vkDestroyDebugUtilsMessengerEXT);
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
	}
#endif

	vkDestroyInstance(instance, NULL);
	return 0;
}
