#include "stdafx.h"
#include "gpu_system.h"
#include "app_window.h"
#include "core_log.h"
#include "_vk_context.h"
#include "_vk_resurces.h"
#include "shaders/_autogen/shader.vert.glsl.h"
#include "shaders/_autogen/shader.frag.glsl.h"
//=============================================================================
namespace
{
	bool       vSync{ false };
	VkExtent2D windowSize{ 0 };    // The window size

	Context           context{};          // The Vulkan context
	VkSurfaceKHR      surface{ nullptr }; // The window surface
	ResourceAllocator allocator;        // The VMA allocator

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
	VkExtent2D swapchainExtent = { 800, 600 };

	VkCommandPool cmdPool;
	VkCommandBuffer cmdBuffers[2];

	const int MAX_FRAMES_IN_FLIGHT = 2;
	VkSemaphore semImageAvailable[MAX_FRAMES_IN_FLIGHT];
	VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
	// Семафоры renderFinished теперь зависят от количества картинок в swapchain!
	std::vector<VkSemaphore> semRenderFinished;
	VkSemaphoreCreateInfo semInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	uint32_t imageIndex;

	VkShaderEXT shaders[2];

	struct Vertex { float x, y, r, g, b; };
	Buffer vertexBuffer;
	Buffer indexBuffer;

	std::vector<Vertex> vertices = {
		{ -0.5f,  0.5f,  1.0f, 0.0f, 0.0f }, // Top-left     (Red)
		{  0.5f,  0.5f,  0.0f, 1.0f, 0.0f }, // Top-right    (Green)
		{  0.5f, -0.5f,  0.0f, 0.0f, 1.0f }, // Bottom-right (Blue)
		{ -0.5f, -0.5f,  1.0f, 1.0f, 1.0f }  // Bottom-left  (White)
	};
	std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

	uint32_t currentFrame = 0;
}
void createRenderFinishedSemaphores()
{
	for (size_t i = 0; i < semRenderFinished.size(); i++)
	{
		vkDestroySemaphore(context.GetDevice(), semRenderFinished[i], nullptr);
		semRenderFinished[i] = nullptr;
	}

	semRenderFinished.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainImages.size(); i++) {
		vkCreateSemaphore(context.GetDevice(), &semInfo, nullptr, &semRenderFinished[i]);
	}
}
void createSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE)
{
	if (oldSwapchain != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(context.GetDevice());

		for (auto view : swapchainImageViews) vkDestroyImageView(context.GetDevice(), view, nullptr);
		swapchainImageViews.clear();
	}

	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.GetPhysicalDevice(), surface, &caps);
	swapchainExtent = caps.currentExtent;
	if (swapchainExtent.width == UINT32_MAX)
	{
		swapchainExtent = { (uint32_t)window::GetWidth(), (uint32_t)window::GetHeight() };
	}

	VkSwapchainCreateInfoKHR scInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface,
		.minImageCount = caps.minImageCount + 1, .imageFormat = swapchainFormat,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, .imageExtent = swapchainExtent,
		.imageArrayLayers = 1, .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = caps.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, .presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = VK_TRUE, .oldSwapchain = oldSwapchain
	};
	vkCreateSwapchainKHR(context.GetDevice(), &scInfo, nullptr, &swapchain);
	if (oldSwapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(context.GetDevice(), oldSwapchain, nullptr);

	uint32_t imgCount;
	vkGetSwapchainImagesKHR(context.GetDevice(), swapchain, &imgCount, nullptr);
	swapchainImages.resize(imgCount);
	vkGetSwapchainImagesKHR(context.GetDevice(), swapchain, &imgCount, swapchainImages.data());

	swapchainImageViews.resize(imgCount);
	for (uint32_t i = 0; i < imgCount; i++) {
		VkImageViewCreateInfo viewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = swapchainFormat,
			.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
		};
		vkCreateImageView(context.GetDevice(), &viewInfo, nullptr, &swapchainImageViews[i]);
	}
	createRenderFinishedSemaphores();
}

void setGraphicsDynamicState(VkCommandBuffer cmd, const VkViewport& viewport, const VkRect2D& scissor)
{
	// Viewport / scissor (counts and values are both dynamic).
	vkCmdSetViewportWithCount(cmd, 1, &viewport);
	vkCmdSetScissorWithCount(cmd, 1, &scissor);

	// Vertex input: bindings (stride, input rate) and attributes (location, format, offset).
	// VK_EXT_vertex_input_dynamic_state replaces VkPipelineVertexInputStateCreateInfo.
	const VkVertexInputBindingDescription2EXT vertexBinding = {
		.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
		.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, .divisor = 1
	};
	const VkVertexInputAttributeDescription2EXT vertexAttributes[2] = {
	{.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, x) },
	{.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, r) }
	};
	vkCmdSetVertexInputEXT(cmd, 1, &vertexBinding, 2, vertexAttributes);

	// Input assembly.
	vkCmdSetPrimitiveTopologyEXT(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);

	// Rasterization (most of these come from VK_EXT_extended_dynamic_state_3).
	vkCmdSetRasterizerDiscardEnable(cmd, VK_FALSE);
	vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
	vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);
	vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	vkCmdSetDepthBiasEnable(cmd, VK_FALSE);
	vkCmdSetDepthClampEnableEXT(cmd, VK_FALSE);

	// Multisampling.
	vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);
	const VkSampleMask sampleMask = 0xFFFFFFFF;
	vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
	vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
	// alphaToOne is required by the spec when its device feature is enabled and a
	// shader object is bound, even if we don't actually use it.
	vkCmdSetAlphaToOneEnableEXT(cmd, VK_FALSE);

	// Depth / stencil.
	vkCmdSetDepthTestEnable(cmd, VK_FALSE);
	vkCmdSetDepthWriteEnable(cmd, VK_FALSE);
	vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_LESS_OR_EQUAL);
	vkCmdSetDepthBoundsTestEnable(cmd, VK_FALSE);
	vkCmdSetStencilTestEnable(cmd, VK_FALSE);

	// Color blend (for one color attachment). Match the previous pipeline's
	// alpha-blend setup; nothing varies between draws so we set it once.
	const VkBool32                blendEnable = VK_TRUE;
	const VkColorBlendEquationEXT blendEquation{
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaBlendOp = VK_BLEND_OP_ADD,
	};
	const VkColorComponentFlags colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &blendEnable);
	vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blendEquation);
	vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &colorWriteMask);
	vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);
}

void createGraphicsShaders()
{
	const char* vertEntryName = "main";
	const char* fragEntryName = "main";
	const std::span<const uint32_t> vertCode{ shader_vert_glsl, std::size(shader_vert_glsl) };
	const std::span<const uint32_t> fragCode{ shader_frag_glsl, std::size(shader_frag_glsl) };

	// Three unlinked shader objects. nextStage is a hint to the driver about
	// which stage will follow at bind time -- it doesn't constrain what we
	// can actually bind.
	const VkShaderCreateFlagsEXT commonFlags = VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT;

	const VkShaderCreateInfoEXT vertCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
		.flags = commonFlags,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.nextStage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
		.codeSize = vertCode.size() * sizeof(uint32_t),
		.pCode = vertCode.data(),
		.pName = vertEntryName,
		.setLayoutCount = 0,  // Descriptor heap: no descriptor set layouts
		.pSetLayouts = nullptr,
		.pushConstantRangeCount = 0,  // Push data (vkCmdPushDataEXT) is used instead
		.pPushConstantRanges = nullptr,
		.pSpecializationInfo = nullptr,  // Vertex shader has no spec constants
	};

	VkShaderCreateInfoEXT fragCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
		.flags = commonFlags,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.nextStage = 0,  // Last stage in the pipeline
		.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
		.codeSize = fragCode.size() * sizeof(uint32_t),
		.pCode = fragCode.data(),
		.pName = fragEntryName,
		.setLayoutCount = 0,
		.pSetLayouts = nullptr,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
		.pSpecializationInfo = nullptr,
	};

	vkCreateShadersEXT(context.GetDevice(), 1, &vertCreateInfo, nullptr, &shaders[0]);
	vkCreateShadersEXT(context.GetDevice(), 1, &fragCreateInfo, nullptr, &shaders[1]);
}
//=============================================================================
bool gpu::Init(const CreateInfo& createInfo)
{
	vSync = createInfo.vSync;
	windowSize.width = window::GetWidth();
	windowSize.height = window::GetHeight();

	if (!context.Init())
		return false;

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = createInfo.instance,
		.hwnd = createInfo.hwnd,
	};
	VkResult result = vkCreateWin32SurfaceKHR(context.GetInstance(), &surfaceCreateInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
	{
		core::Fatal("Failed to create Vulkan surface. " + VkResultStr(result));
		return false;
	}

	VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice   = context.GetPhysicalDevice(),
		.device           = context.GetDevice(),
		.instance         = context.GetInstance(),
		.vulkanApiVersion = std::min(context.GetDeviceApiVersion(), VK_API_VERSION_1_4)
	};
	if (!allocator.Init(allocatorInfo))
		return false;

	// 13. Sync Objects
	VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkCreateSemaphore(context.GetDevice(), &semInfo, nullptr, &semImageAvailable[i]);
		vkCreateFence(context.GetDevice(), &fenceInfo, nullptr, &inFlightFences[i]);
	}

	createSwapchain();

	// 12. VkCommandPool + VkCommandBuffer[]
	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = context.GetGraphicsQueue().familyIndex
	};
	vkCreateCommandPool(context.GetDevice(), &poolInfo, nullptr, &cmdPool);

	VkCommandBufferAllocateInfo cmdAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cmdPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 2
	};
	vkAllocateCommandBuffers(context.GetDevice(), &cmdAllocInfo, cmdBuffers);

	createGraphicsShaders();

	// Buffer (VMA)
	vertexBuffer = allocator.CreateBuffer(vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	void* vertexMappedData;
	vmaMapMemory(allocator, vertexBuffer.allocation, &vertexMappedData);
	std::memcpy(vertexMappedData, vertices.data(), vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(allocator, vertexBuffer.allocation);

	indexBuffer = allocator.CreateBuffer(indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	void* indexMappedData;
	vmaMapMemory(allocator, indexBuffer.allocation, &indexMappedData);
	std::memcpy(indexMappedData, indices.data(), indices.size() * sizeof(uint32_t));
	vmaUnmapMemory(allocator, indexBuffer.allocation);

	return true;
}
//=============================================================================
void gpu::Close()
{
	if (context.GetDevice()) vkDeviceWaitIdle(context.GetDevice());

	allocator.DestroyBuffer(vertexBuffer);
	allocator.DestroyBuffer(indexBuffer);
	allocator.Close();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(context.GetDevice(), semImageAvailable[i], nullptr);
		vkDestroyFence(context.GetDevice(), inFlightFences[i], nullptr);
	}
	for (auto sem : semRenderFinished) {
		vkDestroySemaphore(context.GetDevice(), sem, nullptr);
	}

	vkDestroyShaderEXT(context.GetDevice(), shaders[0], nullptr);
	vkDestroyShaderEXT(context.GetDevice(), shaders[1], nullptr);
	vkDestroyCommandPool(context.GetDevice(), cmdPool, nullptr);
	for (auto view : swapchainImageViews) vkDestroyImageView(context.GetDevice(), view, nullptr);
	vkDestroySwapchainKHR(context.GetDevice(), swapchain, nullptr);

	vkDestroySurfaceKHR(context.GetInstance(), surface, nullptr);
	context.Close();
}
//=============================================================================
bool gpu::BeginFrame()
{
	if (window::GetWindowMinimized())
		return false;

	if (windowSize.width != window::GetWidth() || windowSize.height != window::GetHeight())
	{
		windowSize.width = window::GetWidth();
		windowSize.height = window::GetHeight();
	}

	vkWaitForFences(context.GetDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(context.GetDevice(), 1, &inFlightFences[currentFrame]);

	VkResult result = vkAcquireNextImageKHR(context.GetDevice(), swapchain, UINT64_MAX, semImageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// Swapchain устарел, пересоздаем и пропускаем текущий кадр
		createSwapchain(swapchain);
		return false; // Пропустить кадр
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		// Неожиданная ошибка
		core::Fatal("swapchain failed");
		return false;
	}

	// Готовим командный буфер
	VkCommandBuffer cmd = cmdBuffers[currentFrame];
	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	vkBeginCommandBuffer(cmd, &beginInfo);

	return true;
}
//=============================================================================
void Draw()
{
	VkCommandBuffer cmd = cmdBuffers[currentFrame];

	// --- СТАЛО: Barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL ---
	// VkImageMemoryBarrier2 объединяет всё в одну структуру
	VkImageMemoryBarrier2 barrier1 = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,      // Stage теперь в структуре
		.srcAccessMask = 0,                                       // Stage теперь в структуре
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // Stage теперь в структуре
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,  // Access теперь в структуре
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.image = swapchainImages[imageIndex],
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
	};
	// VkDependencyInfo собирает все барьеры для одного вызова
	VkDependencyInfo depInfo1 = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier1
	};
	vkCmdPipelineBarrier2(cmd, &depInfo1);

	// Dynamic Rendering
	VkRenderingAttachmentInfo colorAttachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = swapchainImageViews[imageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue = {.color = {0.1f, 0.1f, 0.1f, 1.0f} }
	};
	VkRenderingInfo renderingInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = { {0, 0}, swapchainExtent },
		.layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &colorAttachment
	};
	vkCmdBeginRendering(cmd, &renderingInfo);

	VkViewport viewport = { 0, 0, (float)swapchainExtent.width, (float)swapchainExtent.height, 0, 1 };
	VkRect2D scissor = { {0, 0}, swapchainExtent };
	setGraphicsDynamicState(cmd, viewport, scissor);

	// Bind Shaders & Draw
	const std::array<VkShaderStageFlagBits, 5> stages = {
	  VK_SHADER_STAGE_VERTEX_BIT,
	  VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
	  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
	  VK_SHADER_STAGE_GEOMETRY_BIT,
	  VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	const std::array<VkShaderEXT, 5> inshaders = { shaders[0], VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, shaders[1] };
	vkCmdBindShadersEXT(cmd, uint32_t(stages.size()), stages.data(), inshaders.data());

	VkDeviceSize offsets[] = { 0 }; // Смещение для вершинного буфера
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, offsets); // Привязываем вершинный буфер

	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32); // Привязываем индексный буфер

	vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	vkCmdEndRendering(cmd);

	// --- СТАЛО: Barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR ---
	VkImageMemoryBarrier2 barrier2 = barrier1; // Копируем первую, меняем нужное
	barrier2.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier2.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	barrier2.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT; // Обычно хватает bottom-of-pipe
	barrier2.dstAccessMask = 0; // Никакой доступ не требуется
	barrier2.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkDependencyInfo depInfo2 = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier2
	};

	vkCmdPipelineBarrier2(cmd, &depInfo2);

	vkEndCommandBuffer(cmd);
}
//=============================================================================
void gpu::EndFrame()
{
	Draw();

	// Submit
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1,
		.pWaitSemaphores = &semImageAvailable[currentFrame],
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1, .pCommandBuffers = &cmdBuffers[currentFrame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &semRenderFinished[imageIndex]
	};
	vkQueueSubmit(context.GetGraphicsQueue().queue, 1, &submitInfo, inFlightFences[currentFrame]);

	// Present
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1,
		.pWaitSemaphores = &semRenderFinished[imageIndex],
		.swapchainCount = 1, .pSwapchains = &swapchain, .pImageIndices = &imageIndex
	};

	VkResult presentRes = vkQueuePresentKHR(context.GetGraphicsQueue().queue, &presentInfo);
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
		createSwapchain(swapchain);
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
//=============================================================================