#include "Typedefs.h"
#include "Window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
	#define VK_USE_PLATFORM_WIN32_KHR
	#include "Win32Window.h"
#else
	#error This platform is not supported
#endif

#include <vulkan/vulkan.h>

#include "VulkanUtil.h"

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
	const u32 VulkanAPIVersion = VK_API_VERSION_1_2;

	{
		u32 apiVersion = 0;
		VkCall(vkEnumerateInstanceVersion(&apiVersion));

		if (apiVersion < VulkanAPIVersion) {
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

	const char* InstanceLayers[] = {
	#if defined(_DEBUG)
		"VK_LAYER_KHRONOS_validation",
	#endif
	};

	const char* InstanceExtensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
	#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	#else
		#error This platform is not supported
	#endif
	#if defined(_DEBUG)
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	#endif
	};

	VkInstance instance = VK_NULL_HANDLE;
	if (!CreateVulkanInstance(
			&instance,
			VulkanAPIVersion,
			InstanceLayers, sizeof(InstanceLayers) / sizeof(InstanceLayers[0]),
			InstanceExtensions, sizeof(InstanceExtensions) / sizeof(InstanceExtensions[0]))
	) {
		printf("Unable to create vulkan instance!\n");
		return -1;
	}

#if defined(_DEBUG)
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
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
	ASSERT(debugMessenger != VK_NULL_HANDLE);
#endif

	Window window = Window_Create(1280, 720, "Test Renderer");
	if (window == NULL) {
		printf("Unable to create window!\n");
		return -1;
	}

	Window_Show(window);

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (!CreateVulkanSurface(&surface, instance, window)) {
		printf("Unable to create surface!\n");
		return -1;
	}

	const char* DeviceLayers[] = {
	};

	const char* DeviceExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	u32 graphicsQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	u32 presentQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	if (!PickVulkanPhysicalDevice(
			&physicalDevice,
			instance,
			surface,
			VulkanAPIVersion,
			DeviceLayers, sizeof(DeviceLayers) / sizeof(DeviceLayers[0]),
			DeviceExtensions, sizeof(DeviceExtensions) / sizeof(DeviceExtensions[0]),
			&graphicsQueueFamilyIndex,
			&presentQueueFamilyIndex)
	) {
		printf("Unable to find suitable physical device!\n");
		return -1;
	}

	VkDevice device = VK_NULL_HANDLE;
	if (!CreateVulkanDevice(
			&device,
			physicalDevice,
			DeviceLayers, sizeof(DeviceLayers) / sizeof(DeviceLayers[0]),
			DeviceExtensions, sizeof(DeviceExtensions) / sizeof(DeviceExtensions[0]),
			graphicsQueueFamilyIndex,
			presentQueueFamilyIndex)
	) {
		printf("Unable to create device!\n");
		return -1;
	}

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	{
		vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
		vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
	}
	ASSERT(graphicsQueue != VK_NULL_HANDLE);
	ASSERT(presentQueue != VK_NULL_HANDLE);

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR swapchainFormat = {};
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkExtent2D swapchainExtent = {};
	{
		{
			u32 surfaceFormatCount = 0;
			VkCall(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, NULL));
			ASSERT(surfaceFormatCount > 0);
			VkSurfaceFormatKHR surfaceFormats[surfaceFormatCount];
			VkCall(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats));

			if (surfaceFormatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
				swapchainFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
				swapchainFormat.colorSpace = surfaceFormats[0].colorSpace;
			} else {
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
		VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
		{
			VkCall(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

			if (surfaceCapabilities.currentExtent.width != ~0u && surfaceCapabilities.currentExtent.height != ~0u) {
				swapchainExtent = surfaceCapabilities.currentExtent;
			} else {
				u32 width = 0, height = 0;
				Window_GetSize(window, &width, &height);

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

		VkCompositeAlphaFlagsKHR compositeAlpha =
			  (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
			? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
			: (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
			? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
			: (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
			? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
			: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

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
			.compositeAlpha = compositeAlpha,
			.presentMode = swapchainPresentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = swapchain,
		}, NULL, &swapchain));
	}
	ASSERT(swapchain != VK_NULL_HANDLE);

	u32 swapchainImageCount = 0;
	VkCall(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL));
	VkImage swapchainImages[swapchainImageCount];
	VkCall(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages));

	u32 swapchainImageViewCount = swapchainImageCount;
	VkImageView swapchainImageViews[swapchainImageViewCount];
	for (u64 i = 0; i < swapchainImageViewCount; i++) {
		swapchainImageViews[i] = VK_NULL_HANDLE;

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

		ASSERT(swapchainImageViews[i] != VK_NULL_HANDLE);
	}

	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	{
		VkCall(vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		}, NULL, &imageAvailableSemaphore));

		VkCall(vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		}, NULL, &renderFinishedSemaphore));
	}
	ASSERT(imageAvailableSemaphore != VK_NULL_HANDLE);
	ASSERT(renderFinishedSemaphore != VK_NULL_HANDLE);

	VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
	{
		VkCall(vkCreateCommandPool(device, &(VkCommandPoolCreateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = graphicsQueueFamilyIndex,
		}, NULL, &graphicsCommandPool));
	}
	ASSERT(graphicsCommandPool != VK_NULL_HANDLE);

	VkCommandBuffer graphicsCommandBuffer = VK_NULL_HANDLE;
	{
		VkCall(vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = graphicsCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		}, &graphicsCommandBuffer));
	}
	ASSERT(graphicsCommandBuffer != VK_NULL_HANDLE);

	VkRenderPass renderPass = VK_NULL_HANDLE;
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
				.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
	ASSERT(renderPass != VK_NULL_HANDLE);

	u32 swapchainFramebufferCount = swapchainImageCount;
	VkFramebuffer swapchainFramebuffers[swapchainFramebufferCount];
	for (u64 i = 0; i < swapchainFramebufferCount; i++) {
		swapchainFramebuffers[i] = VK_NULL_HANDLE;

		VkCall(vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = 1,
			.pAttachments = &swapchainImageViews[i],
			.width = swapchainExtent.width,
			.height = swapchainExtent.height,
			.layers = 1,
		}, NULL, &swapchainFramebuffers[i]));

		ASSERT(swapchainFramebuffers[i] != VK_NULL_HANDLE);
	}

	VkShaderModule vertexShader = VK_NULL_HANDLE;
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
	ASSERT(vertexShader != VK_NULL_HANDLE);

	VkShaderModule fragmentShader = VK_NULL_HANDLE;
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
	ASSERT(fragmentShader != VK_NULL_HANDLE);

	VkPipelineLayout trianglePipelineLayout = VK_NULL_HANDLE;
	{
		VkCall(vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		}, NULL, &trianglePipelineLayout));
	}
	ASSERT(trianglePipelineLayout != VK_NULL_HANDLE);

	VkPipelineCache trianglePipelineCache = VK_NULL_HANDLE;
	{
		VkCall(vkCreatePipelineCache(device, &(VkPipelineCacheCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		}, NULL, &trianglePipelineCache));
	}
	ASSERT(trianglePipelineCache != VK_NULL_HANDLE);

	VkPipeline trianglePipeline = VK_NULL_HANDLE;
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
	ASSERT(trianglePipeline != VK_NULL_HANDLE);

	while (Window_PollEvents()) {
		u32 swapchainImageIndex = 0;
		VkCall(vkAcquireNextImageKHR(device, swapchain, ~0ull, imageAvailableSemaphore, NULL, &swapchainImageIndex));

		VkCall(vkResetCommandPool(device, graphicsCommandPool, 0));

		VkCall(vkBeginCommandBuffer(graphicsCommandBuffer, &(VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		}));

		vkCmdPipelineBarrier(
			graphicsCommandBuffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_DEPENDENCY_BY_REGION_BIT,
			0,
			NULL,
			0,
			NULL,
			1,
			&(VkImageMemoryBarrier){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = swapchainImages[swapchainImageIndex],
				.subresourceRange = (VkImageSubresourceRange){
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = VK_REMAINING_MIP_LEVELS,
					.layerCount = VK_REMAINING_ARRAY_LAYERS,
				},
			}
		);

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
		Window_GetSize(window, &windowWidth, &windowHeight);

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

		vkCmdPipelineBarrier(
			graphicsCommandBuffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_DEPENDENCY_BY_REGION_BIT,
			0,
			NULL,
			0,
			NULL,
			1,
			&(VkImageMemoryBarrier){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = 0,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = swapchainImages[swapchainImageIndex],
				.subresourceRange = (VkImageSubresourceRange){
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = VK_REMAINING_MIP_LEVELS,
					.layerCount = VK_REMAINING_ARRAY_LAYERS,
				},
			}
		);

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
	Window_Destroy(window);

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
