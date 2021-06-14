#include "Typedefs.h"
#include "Window.h"
#include "ObjLoader.h"

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
#include "VulkanSwapchain.h"

typedef struct Vertex_t {
	Vector3 Position;
	Vector3 Normal;
	Vector3 TexCoord;
} Vertex;

typedef struct Mesh_t {
	Vertex* Vertices;
	u64 VertexLength;
	u32* Indices;
	u64 IndexLength;
} Mesh;

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
	if (!ChooseVulkanPhysicalDevice(
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

	VkSurfaceFormatKHR surfaceFormat = {};
	if (!ChooseVulkanSurfaceFormat(&surfaceFormat, physicalDevice, surface)) {
		printf("Unable to find a suitable surface format!\n");
		return -1;
	}

	VkRenderPass renderPass = VK_NULL_HANDLE;
	{
		VkCall(vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &(VkAttachmentDescription){
				.format = surfaceFormat.format,
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

	u32 windowWidth = 0, windowHeight = 0;
	Window_GetSize(window, &windowWidth, &windowHeight);

	VulkanSwapchain swapchain = {};
	if (!VulkanSwapchain_Create(
		&swapchain,
		physicalDevice,
		device,
		surface,
		renderPass,
		surfaceFormat,
		window,
		graphicsQueueFamilyIndex,
		presentQueueFamilyIndex,
		windowWidth,
		windowHeight)
	) {
		printf("Unable to create swapchain!\n");
		return -1;
	}

	VkShaderModule vertexShader = VK_NULL_HANDLE;
	{
		FILE* file = fopen("triangle.vert.spirv", "rb");
		ASSERT(file);

		fseek(file, 0, SEEK_END);
		u64 length = ftell(file);
		ASSERT(length > 0);
		fseek(file, 0, SEEK_SET);

		char code[length];
		ASSERT(fread(code, sizeof(code[0]), length, file) == length);
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
		ASSERT(fread(code, sizeof(code[0]), length, file) == length);
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

	while (Window_PollEvents()) {
		VulkanSwapchain_TryResize(&swapchain);

		u32 swapchainImageIndex = 0;
		VkCall(vkAcquireNextImageKHR(device, swapchain.Swapchain, ~0ull, imageAvailableSemaphore, NULL, &swapchainImageIndex));

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
				.image = swapchain.Images[swapchainImageIndex],
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
			.framebuffer = swapchain.Framebuffers[swapchainImageIndex],
			.renderArea = (VkRect2D){
				.offset = (VkOffset2D){ .x = 0, .y = 0 },
				.extent = swapchain.Extent,
			},
			.clearValueCount = 1,
			.pClearValues = &Clear,
		}, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(graphicsCommandBuffer, 0, 1, &(VkViewport){
			.x = 0.0f,
			.y = swapchain.Extent.height,
			.width = swapchain.Extent.width,
			.height = -cast(float) swapchain.Extent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		});
		vkCmdSetScissor(graphicsCommandBuffer, 0, 1, &(VkRect2D){
			.offset = (VkOffset2D){
				.x = 0,
				.y = 0,
			},
			.extent = (VkExtent2D){
				.width = swapchain.Extent.width,
				.height = swapchain.Extent.height,
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
				.image = swapchain.Images[swapchainImageIndex],
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
			.pSwapchains = &swapchain.Swapchain,
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

		VulkanSwapchain_Destroy(&swapchain);

		vkDestroyRenderPass(device, renderPass, NULL);

		vkDestroyCommandPool(device, graphicsCommandPool, NULL);

		vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
		vkDestroySemaphore(device, renderFinishedSemaphore, NULL);
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
