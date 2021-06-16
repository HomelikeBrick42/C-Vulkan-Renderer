#include "Typedefs.h"
#include "Window.h"
#include "ObjLoader.h"
#include "Vector.h"
#include "Matrix.h"

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

#if defined(_DEBUG)

VkBool32 VKAPI_CALL DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	const char* flagString =
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) ? "Verbose" :
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "Info" :
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "Warning" :
		(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "Error" :
		"Unknown Type";

	printf("%s: %s\n", flagString, pCallbackData->pMessage);
#if defined(_WIN32) || defined(_WIN64)
	OutputDebugStringA(pCallbackData->pMessage);
#endif

	ASSERT(!(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT));
	return VK_TRUE;
}

#endif

typedef struct Vertex_t {
	Vector3 Position;
	Vector3 Normal;
	Vector2 TexCoord;
} Vertex;

typedef struct Mesh_t {
	Vertex* Vertices;
	u64 VertexCount;
	u32* Indices;
	u64 IndexCount;
} Mesh;

typedef struct UniformBuffer_t {
	Matrix4 ModelMatrix;
	Matrix4 ViewMatrix;
	Matrix4 ProjectionMatrix;
} UniformBuffer;

typedef struct VulkanBuffer_t {
	VkBuffer Buffer;
	VkDevice Device;
	VkPhysicalDevice PhysicalDevice;
	VkDeviceMemory Memory;
	void* Data;
	u64 Size;

	VkBufferUsageFlags UsageFlags;
} VulkanBuffer;

static u32 VulkanBuffer_SelectMemoryType(const VkPhysicalDeviceMemoryProperties* memoryProperties, u32 memoryTypeBits, VkMemoryPropertyFlags propertyFlags) {
	for (u32 i = 0; i < memoryProperties->memoryTypeCount; i++) {
		if ((memoryTypeBits & (1 << i)) != 0 && (memoryProperties->memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
			return i;
		}
	}

	return ~0u;
}

b8 VulkanBuffer_Create(VulkanBuffer* buffer, VkDevice device, VkPhysicalDevice physicalDevice, u64 size, VkBufferUsageFlags usageFlags) {
	buffer->Buffer = VK_NULL_HANDLE;
	buffer->Device = device;
	buffer->PhysicalDevice = physicalDevice;
	buffer->Memory = VK_NULL_HANDLE;
	buffer->Data = NULL;
	buffer->Size = size;
	buffer->UsageFlags = usageFlags;

	VkCheck(vkCreateBuffer(device, &(VkBufferCreateInfo){
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = buffer->UsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	}, NULL, &buffer->Buffer));

	if (buffer->Buffer == VK_NULL_HANDLE) {
		return false;
	}

	VkPhysicalDeviceMemoryProperties memoryProperties = {};
	vkGetPhysicalDeviceMemoryProperties(buffer->PhysicalDevice, &memoryProperties);

	VkMemoryRequirements memoryRequirements = {};
	vkGetBufferMemoryRequirements(buffer->Device, buffer->Buffer, &memoryRequirements);
	
	u32 memoryTypeIndex = VulkanBuffer_SelectMemoryType(&memoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	ASSERT(memoryTypeIndex != ~0u);

	VkCheck(vkAllocateMemory(buffer->Device, &(VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = memoryTypeIndex,
	}, NULL, &buffer->Memory));

	if (buffer->Memory == VK_NULL_HANDLE) {
		return false;
	}

	VkCheck(vkBindBufferMemory(buffer->Device, buffer->Buffer, buffer->Memory, 0));

	VkCheck(vkMapMemory(buffer->Device, buffer->Memory, 0, buffer->Size, 0, &buffer->Data));
	
	if (buffer->Data == NULL) {
		return false;
	}

	return true;
}

void VulkanBuffer_Destroy(VulkanBuffer* buffer) {
	vkUnmapMemory(buffer->Device, buffer->Memory);
	vkFreeMemory(buffer->Device, buffer->Memory, NULL);
	vkDestroyBuffer(buffer->Device, buffer->Buffer, NULL);
}

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
		ASSERT(vkCreateDebugUtilsMessengerEXT);

		VkCall(vkCreateDebugUtilsMessengerEXT(instance, &(VkDebugUtilsMessengerCreateInfoEXT){
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    		.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
    							| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
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

	VkDescriptorSetLayout meshDescriptorSetLayout = VK_NULL_HANDLE;
	{
		VkCall(vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &(VkDescriptorSetLayoutBinding){
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			},
		}, NULL, &meshDescriptorSetLayout));
	}
	ASSERT(meshDescriptorSetLayout != VK_NULL_HANDLE);

	VkPipelineLayout meshPipelineLayout = VK_NULL_HANDLE;
	{
		VkCall(vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
    		.pSetLayouts = &meshDescriptorSetLayout,
		}, NULL, &meshPipelineLayout));
	}
	ASSERT(meshPipelineLayout != VK_NULL_HANDLE);

	VkPipelineCache meshPipelineCache = VK_NULL_HANDLE;
	{
		VkCall(vkCreatePipelineCache(device, &(VkPipelineCacheCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		}, NULL, &meshPipelineCache));
	}
	ASSERT(meshPipelineCache != VK_NULL_HANDLE);

	VkPipeline meshPipeline = VK_NULL_HANDLE;
	{
		VkCall(vkCreateGraphicsPipelines(device, meshPipelineCache, 1, &(VkGraphicsPipelineCreateInfo){
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
				.vertexBindingDescriptionCount = 1,
				.pVertexBindingDescriptions = &(VkVertexInputBindingDescription){
					.binding = 0,
					.stride = sizeof(Vertex),
					.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
				},
				.vertexAttributeDescriptionCount = 3,
				.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[3]){
					{
						.location = 0,
						.binding = 0,
						.format = VK_FORMAT_R32G32B32_SFLOAT,
						.offset = 0,
					},
					{
						.location = 1,
						.binding = 0,
						.format = VK_FORMAT_R32G32B32_SFLOAT,
						.offset = sizeof(Vector3),
					},
					{
						.location = 2,
						.binding = 0,
						.format = VK_FORMAT_R32G32_SFLOAT,
						.offset = sizeof(Vector3) * 2,
					},
				},
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
			.layout = meshPipelineLayout,
			.renderPass = renderPass,
		}, NULL, &meshPipeline));
	}
	ASSERT(meshPipeline != VK_NULL_HANDLE);

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

	VkDescriptorPool meshDescriptorPool = VK_NULL_HANDLE;
	{
		VkCall(vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = 1,
			.poolSizeCount = 1,
			.pPoolSizes = &(VkDescriptorPoolSize){
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
			},
		}, NULL, &meshDescriptorPool));
	}
	ASSERT(meshDescriptorPool != VK_NULL_HANDLE);

	VkDescriptorSet meshDescriptorSet = VK_NULL_HANDLE;
	{
		VkCall(vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = meshDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &meshDescriptorSetLayout,
		}, &meshDescriptorSet));
	}
	ASSERT(meshDescriptorSet != VK_NULL_HANDLE);

	Mesh mesh = {};
	{
		ObjMesh objMesh = {};
		if (!ObjMesh_Create(&objMesh, "Cube.obj")) {
			printf("Unable to load Cube.obj\n");
			return -1;
		}

		for (u64 i = 0; i < objMesh.FaceCount; i++) {
			mesh.VertexCount += 3;
			mesh.Vertices = realloc(mesh.Vertices, mesh.VertexCount * sizeof(mesh.Vertices[0]));
			ASSERT(mesh.Vertices);

			mesh.Vertices[mesh.VertexCount - 3].Position = objMesh.Positions[objMesh.Faces[i].PositionIndices[0]];
			mesh.Vertices[mesh.VertexCount - 2].Position = objMesh.Positions[objMesh.Faces[i].PositionIndices[1]];
			mesh.Vertices[mesh.VertexCount - 1].Position = objMesh.Positions[objMesh.Faces[i].PositionIndices[2]];
			
			mesh.Vertices[mesh.VertexCount - 3].Normal = objMesh.Normals[objMesh.Faces[i].NormalIndices[0]];
			mesh.Vertices[mesh.VertexCount - 2].Normal = objMesh.Normals[objMesh.Faces[i].NormalIndices[1]];
			mesh.Vertices[mesh.VertexCount - 1].Normal = objMesh.Normals[objMesh.Faces[i].NormalIndices[2]];
			
			mesh.Vertices[mesh.VertexCount - 3].TexCoord = objMesh.TexCoords[objMesh.Faces[i].TexCoordIndices[0]];
			mesh.Vertices[mesh.VertexCount - 2].TexCoord = objMesh.TexCoords[objMesh.Faces[i].TexCoordIndices[1]];
			mesh.Vertices[mesh.VertexCount - 1].TexCoord = objMesh.TexCoords[objMesh.Faces[i].TexCoordIndices[2]];
		}

		for (u64 i = 0; i < mesh.VertexCount; i++) {
			mesh.IndexCount++;
			mesh.Indices = realloc(mesh.Indices, mesh.IndexCount * sizeof(mesh.Indices[0]));
			ASSERT(mesh.Indices);

			mesh.Indices[mesh.IndexCount - 1] = i;
		}

		ObjMesh_Destory(&objMesh);
	}

	VulkanBuffer vertexBuffer = {};
	if (!VulkanBuffer_Create(&vertexBuffer, device, physicalDevice, mesh.VertexCount * sizeof(mesh.Vertices[0]), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
		printf("Unable to create vertex buffer!\n");
		return -1;
	}

	memcpy(vertexBuffer.Data, mesh.Vertices, mesh.VertexCount * sizeof(mesh.Vertices[0]));

	VulkanBuffer indexBuffer = {};
	if (!VulkanBuffer_Create(&indexBuffer, device, physicalDevice, mesh.IndexCount * sizeof(mesh.Indices[0]), VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
		printf("Unable to create index buffer!\n");
		return -1;
	}

	memcpy(indexBuffer.Data, mesh.Indices, mesh.IndexCount * sizeof(mesh.Indices[0]));

	VulkanBuffer uniformBuffer = {};
	if (!VulkanBuffer_Create(&uniformBuffer, device, physicalDevice, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) {
		printf("Unable to create uniform buffer!\n");
		return -1;
	}

	UniformBuffer* uniformData = uniformBuffer.Data;
	uniformData->ModelMatrix = Matrix4_Scale((Vector3){ 0.8f, 0.8f, 0.8f });
	uniformData->ViewMatrix = Matrix4_Identity();
	uniformData->ProjectionMatrix = Matrix4_Identity();

	while (Window_PollEvents()) {
		if (VulkanSwapchain_TryResize(&swapchain)) {
			uniformData->ProjectionMatrix = Matrix4_Identity();
		}

		vkUpdateDescriptorSets(device, 1, &(VkWriteDescriptorSet){
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = meshDescriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &(VkDescriptorBufferInfo){
				.buffer = uniformBuffer.Buffer,
				.offset = 0,
				.range = uniformBuffer.Size,
			},
		}, 0, NULL);

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

		vkCmdBindPipeline(graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

		vkCmdBindDescriptorSets(graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineLayout, 0, 1, &meshDescriptorSet, 0, NULL);
		vkCmdBindVertexBuffers(graphicsCommandBuffer, 0, 1, &vertexBuffer.Buffer, &(VkDeviceSize){ 0 });
		vkCmdBindIndexBuffer(graphicsCommandBuffer, indexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(graphicsCommandBuffer, mesh.IndexCount, 1, 0, 0, 0);

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
		VulkanBuffer_Destroy(&uniformBuffer);
		VulkanBuffer_Destroy(&vertexBuffer);
		VulkanBuffer_Destroy(&indexBuffer);

		if (mesh.Vertices) {
			free(mesh.Vertices);
		}

		if (mesh.Indices) {
			free(mesh.Indices);
		}

		vkDestroyPipelineLayout(device, meshPipelineLayout, NULL);
		vkDestroyPipelineCache(device, meshPipelineCache, NULL);
		vkDestroyPipeline(device, meshPipeline, NULL);

		vkDestroyDescriptorSetLayout(device, meshDescriptorSetLayout, NULL);

		vkDestroyShaderModule(device, fragmentShader, NULL);
		vkDestroyShaderModule(device, vertexShader, NULL);

		VulkanSwapchain_Destroy(&swapchain);

		vkDestroyRenderPass(device, renderPass, NULL);

		vkDestroyDescriptorPool(device, meshDescriptorPool, NULL);

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
