#include "stdafx.h"
#include "gpu_system.h"
#include "app_window.h"
#include "core_log.h"
#include "_vk_context.h"
#include "_vk_swapchain.h"
#include "_vk_resurces.h"
#include "_vk_debugUtils.h"
#include "shaders/_autogen/shader.vert.glsl.h"
#include "shaders/_autogen/shader.frag.glsl.h"
//=============================================================================
namespace
{
	bool       vSync{ false };
	VkExtent2D windowSize{ 0 };    // The window size

	Context           context{};          // The Vulkan context
	VkSurfaceKHR      surface{ nullptr }; // The window surface
	ResourceAllocator allocator;          // The VMA allocator
	VkCommandPool     transientCmdPool{ nullptr };
	Swapchain         swapchain;          // The swapchain
	struct DepthBuffer final
	{
		VkImage depthImage{ nullptr };
		VkImageView depthImageView{ nullptr };
		VmaAllocation depthImageAllocation{ nullptr };

	} depthBuffer;

	// Frame resources and synchronization
	struct FrameData final
	{
		VkCommandPool   cmdPool;          // Command pool for recording commands for this frame
		VkCommandBuffer cmdBuffer;        // Command buffer containing the frame's rendering commands
		uint64_t        lastSignalValue;  // Timeline value last signaled for this frame's resources
	};
	std::vector<FrameData> frameData;      // Collection of per-frame resources to support multiple frames in flight
	VkSemaphore frameTimelineSemaphore{};  // Timeline semaphore used to synchronize CPU submission with GPU completion
	uint64_t    frameCounter{ 1 };           // Monotonic timeline counter (increments each frame)





	VkCommandBuffer mainCommandBuffer;
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
}
//=============================================================================
bool initDepthBuffer()
{
	const std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
	for (const VkFormat& format : depthFormatList)
	{
		VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
		vkGetPhysicalDeviceFormatProperties2(context.GetPhysicalDevice(), format, &formatProperties);
		if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depthFormat = format;
			break;
		}
	}
	if (depthFormat == VK_FORMAT_UNDEFINED)
		return false;

	VkImageCreateInfo depthImageCI{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depthFormat,
		.extent{.width = windowSize.width, .height = windowSize.height, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VmaAllocationCreateInfo allocCI{ .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
	VK_CHECK_FALSE(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthBuffer.depthImage, &depthBuffer.depthImageAllocation, nullptr));
	VkImageViewCreateInfo depthViewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = depthBuffer.depthImage, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = depthFormat, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 } };
	VK_CHECK_FALSE(vkCreateImageView(context.GetDevice(), &depthViewCI, nullptr, &depthBuffer.depthImageView));

	return true;
}
//=============================================================================
/*--
* Creates a command pool (long life) and buffer for each frame in flight. Unlike the temporary command pool,
* these pools persist between frames and don't use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT.
* Each frame gets its own command buffer which records all rendering commands for that frame.
-*/
bool createFrameSubmission(uint32_t numFrames)
{
	VkDevice device = context.GetDevice();

	frameData.resize(numFrames);

	// Initialize timeline semaphore at 0. We'll use a monotonic counter (m_frameCounter) starting at 1.
	const uint64_t initialValue = 0;

	VkSemaphoreTypeCreateInfo timelineCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = nullptr,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = initialValue,
	};

	/*--
	 * Create timeline semaphore for GPU-CPU synchronization
	 * This ensures resources aren't overwritten while still in use by the GPU
	-*/
	const VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timelineCreateInfo };
	VK_CHECK_FALSE(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frameTimelineSemaphore));
	//DBG_VK_NAME(frameTimelineSemaphore);

	/*--
	 * Create command pools and buffers for each frame
	 * Each frame gets its own command pool to allow parallel command recording while previous frames may still be executing on the GPU
	-*/
	const uint32_t queueFamily = context.GetGraphicsQueue().familyIndex;

	for (uint32_t i = 0; i < numFrames; i++)
	{
		frameData[i].lastSignalValue = initialValue;  // Initialize to timeline semaphore's initial value

		// Separate pools allow independent reset/recording of commands while other frames are still in-flight
		if (!CreateCommandPool(frameData[i].cmdPool, device, queueFamily))
		{
			core::Fatal("CreateCommandPool failed");
			return false;
		}
		//DBG_VK_NAME(m_frameData[i].cmdPool);

		const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = frameData[i].cmdPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		VK_CHECK_FALSE(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &frameData[i].cmdBuffer));
		//DBG_VK_NAME(m_frameData[i].cmdBuffer);
	}

	return true;
}
//=============================================================================
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



/*---
* Prepare frame resources - the first step in the rendering process.
* It looks if the swapchain require rebuild, which happens when the window is resized.
* It acquires the image from the swapchain to render into.
* Returns true if we can proceed with rendering, false otherwise.
-*/
bool prepareFrameResources()
{
	// Check if swapchain needs rebuilding (this internally calls vkQueueWaitIdle())
	if (swapchain.NeedRebuilding())
	{
		windowSize = swapchain.ReInitResources(vSync);
		//return false;
	}

	// Wait first, *then* acquire. Waiting on the timeline semaphore guarantees the
	// GPU has released this slot's resources (command buffer, in-flight data) before
	// we start reusing them. Acquiring first would mean we hold a swapchain image
	// while still potentially racing the GPU on per-frame resources -- and in
	// out-of-order presentation, the wrong slot's wait value would be in scope.
	auto& frame = frameData[swapchain.GetFrameResourceIndex()];

	// Wait until GPU has finished processing the frame that was using these resources previously
	// Note: If swapchain was rebuilt above, this wait is essentially a no-op since vkQueueWaitIdle() was already called
	const VkSemaphoreWaitInfo waitInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &frameTimelineSemaphore,
		.pValues = &frame.lastSignalValue,
	};
	/*VK_CHECK*/(vkWaitSemaphores(context.GetDevice(), &waitInfo, std::numeric_limits<uint64_t>::max()));
#ifdef NVVK_SEMAPHORE_DEBUG
	LOGI("WaitFrame: \t\t slot=%u waitValue=%llu", m_swapchain.getFrameResourceIndex(),
		static_cast<unsigned long long>(frame.lastSignalValue));
#endif

	VkResult result = swapchain.AcquireNextImage(context.GetDevice());
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		// Неожиданная ошибка
		core::Fatal("swapchain failed");
		return false;
	}
	return true;
}

/*---
* Begin command buffer recording for the frame
* It resets the command pool to reuse the command buffer for recording new rendering commands for the current frame.
* Returns the command buffer for the frame.
-*/
VkCommandBuffer beginCommandRecording()
{
	VkDevice device = context.GetDevice();

	// Get the frame data for the current in-flight slot (owned by Swapchain).
	auto& frame = frameData[swapchain.GetFrameResourceIndex()];

	/*--
	 * Reset the whole command pool to reuse its command buffer for recording
	 * the current frame. An equivalent alternative is to create the pool with
	 * VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT and call
	 * vkResetCommandBuffer() per buffer; whole-pool reset is simpler when each
	 * pool only contains one buffer (as here).
	-*/
	/*VK_CHECK*/(vkResetCommandPool(device, frame.cmdPool, 0));
	VkCommandBuffer cmd = frame.cmdBuffer;

	// Begin the command buffer recording for the frame
	const VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	/*VK_CHECK*/(vkBeginCommandBuffer(cmd, &beginInfo));

	return cmd;
}
/*--
 * End command buffer recording for the frame
-*/
void endCommandRecording(VkCommandBuffer cmd) { /*VK_CHECK*/(vkEndCommandBuffer(cmd)); }

/*--
   * End the frame by submitting the command buffer to the GPU and presenting the image.
   * Adds binary semaphores to wait for the image to be available and signal when rendering is done.
   * Adds the timeline semaphore to signal when the frame is completed.
   * Moves to the next frame.
  -*/
void endFrame(VkCommandBuffer cmd)
{
	/*--
	 * Prepare to submit the current frame for rendering
	 * First add the swapchain semaphore to wait for the image to be available.
	-*/
	std::vector<VkSemaphoreSubmitInfo> waitSemaphores;
	std::vector<VkSemaphoreSubmitInfo> signalSemaphores;
	waitSemaphores.push_back({
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = swapchain.GetAcquireSemaphore(),
		.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		});
	signalSemaphores.push_back({
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = swapchain.GetPresentSemaphore(),
		.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		});

	// Get the frame data for the current in-flight slot. The Swapchain owns the
	// frame-resource index; we use it here so both stay in lockstep by construction.
	const uint32_t frameSlot = swapchain.GetFrameResourceIndex();
	auto& frame = frameData[frameSlot];

	/*--
	 * Calculate the signal value for when this frame completes
	 * Use monotonic counter that increments by 1 each frame: 1, 2, 3, 4...
	-*/
	const uint64_t signalFrameValue = frameCounter++;
	frame.lastSignalValue = signalFrameValue;  // Store for next time this frame buffer is used
#ifdef NVVK_SEMAPHORE_DEBUG
	LOGI("SubmitFrame: \t\t slot=%u signalValue=%llu", frameSlot, static_cast<unsigned long long>(signalFrameValue));
#endif

	/*--
	 * Add timeline semaphore to signal when GPU completes this frame
	 * The color attachment output stage is used since that's when the frame is fully rendered
	-*/
	signalSemaphores.push_back({
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = frameTimelineSemaphore,
		.value = signalFrameValue,
		.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		});

	// Note : in this sample, we only have one command buffer per frame.
	const std::array<VkCommandBufferSubmitInfo, 1> cmdBufferInfo{ {{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = cmd,
	}} };

	// Populate the submit info to synchronize rendering and send the command buffer
	const std::array<VkSubmitInfo2, 1> submitInfo{ {{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = uint32_t(waitSemaphores.size()),    //
		.pWaitSemaphoreInfos = waitSemaphores.data(),              // Wait for the image to be available
		.commandBufferInfoCount = uint32_t(cmdBufferInfo.size()),     //
		.pCommandBufferInfos = cmdBufferInfo.data(),               // Command buffer to submit
		.signalSemaphoreInfoCount = uint32_t(signalSemaphores.size()),  //
		.pSignalSemaphoreInfos = signalSemaphores.data(),            // Signal when rendering is finished
	}} };

	// Submit the command buffer to the GPU and signal when it's done
	/*VK_CHECK*/(vkQueueSubmit2(context.GetGraphicsQueue().queue, uint32_t(submitInfo.size()), submitInfo.data(), nullptr));

	// Present the image. presentFrame() advances the swapchain's frame-resource
	// index for us, so the next call to prepareFrameResources() will pick up
	// the next slot.
	swapchain.PresentFrame(context.GetGraphicsQueue().queue);
}
//=============================================================================
bool gpu::Init(const CreateInfo& createInfo)
{
	vSync = createInfo.vSync;
	//windowSize.width = window::GetWidth();
	//windowSize.height = window::GetHeight();

	if (!context.Init())
		return false;

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = createInfo.instance,
		.hwnd = createInfo.hwnd,
	};
	VK_CHECK_FALSE(vkCreateWin32SurfaceKHR(context.GetInstance(), &surfaceCreateInfo, nullptr, &surface));

	VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice   = context.GetPhysicalDevice(),
		.device           = context.GetDevice(),
		.instance         = context.GetInstance(),
		.vulkanApiVersion = std::min(context.GetDeviceApiVersion(), VK_API_VERSION_1_4)
	};
	if (!allocator.Init(allocatorInfo))
		return false;

	if (!CreateCommandPool(transientCmdPool, context.GetDevice(), context.GetGraphicsQueue().familyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT))
		return false;
	//DBG_VK_NAME(transientCmdPool);

	swapchain.Init(context.GetPhysicalDevice(), context.GetDevice(), context.GetGraphicsQueue(), surface, transientCmdPool);
	windowSize = swapchain.InitResources(vSync); // Update the window size to the actual size of the surface
	if (!initDepthBuffer())
		return false;

	// Create what is needed to submit the scene for each frame in-flight m_frameData is sized by frames-in-flight (CPU parallelism), NOT by imageCount (GPU/presentation parallelism).
	if (!createFrameSubmission(swapchain.GetFramesInFlight()))
		return false;

	createGraphicsShaders();

	// Buffer (VMA)
	{
		VkCommandBuffer cmd = BeginSingleTimeCommands(context.GetDevice(), transientCmdPool);

		vertexBuffer = allocator.CreateBufferAndUploadData(cmd, std::span<Vertex>(vertices),
			VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT).value();
		//DBG_VK_NAME(m_vertexBuffer.buffer);

		indexBuffer = allocator.CreateBufferAndUploadData(cmd, std::span<uint32_t>(indices),
			VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT).value();
		//DBG_VK_NAME(m_vertexBuffer.buffer);

		EndSingleTimeCommands(cmd, context.GetDevice(), transientCmdPool, context.GetGraphicsQueue().queue);
	}
	allocator.FreeStagingBuffers();  // Data is uploaded, staging buffers can be released

	return true;
}
//=============================================================================
void gpu::Close()
{
	if (context.GetDevice()) vkDeviceWaitIdle(context.GetDevice());

	vkDestroyShaderEXT(context.GetDevice(), shaders[0], nullptr);
	vkDestroyShaderEXT(context.GetDevice(), shaders[1], nullptr);

	swapchain.Close();
	vkDestroyCommandPool(context.GetDevice(), transientCmdPool, nullptr);

	// Frame info
	for (size_t i = 0; i < frameData.size(); i++)
	{
		vkFreeCommandBuffers(context.GetDevice(), frameData[i].cmdPool, 1, &frameData[i].cmdBuffer);
		vkDestroyCommandPool(context.GetDevice(), frameData[i].cmdPool, nullptr);
	}
	vkDestroySemaphore(context.GetDevice(), frameTimelineSemaphore, nullptr);

	vkDestroySurfaceKHR(context.GetInstance(), surface, nullptr);

	allocator.DestroyBuffer(vertexBuffer);
	allocator.DestroyBuffer(indexBuffer);
	allocator.Close();
	context.Close();
}
//=============================================================================
bool gpu::BeginFrame()
{
	if (window::GetWindowMinimized())
		return false;

	//if (windowSize.width != window::GetWidth() || windowSize.height != window::GetHeight())
	//{
	//	windowSize.width = window::GetWidth();
	//	windowSize.height = window::GetHeight();
	//}
	if (!prepareFrameResources()) return false;

	// Begin command buffer recording
	mainCommandBuffer = beginCommandRecording();

	return true;
}
//=============================================================================
void gpu::EndFrame()
{
	// draw
	{
		// Transition the swapchain image to general layout for use as a render target in dynamic rendering
		cmdTransitionSwapchainLayout(mainCommandBuffer, swapchain.GetImage(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL);

		// Dynamic Rendering
		// Image to render to
		VkRenderingAttachmentInfo colorAttachment {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = swapchain.GetImageView(),
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,   // Clear the image (see clearValue)
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,  // Store the image (keep the image)
			.clearValue = {{{0.1f, 0.1f, 0.1f, 1.0f}}},
		};

		// Details of the dynamic rendering
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {{0, 0}, windowSize},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachment,
		};

		vkCmdBeginRendering(mainCommandBuffer, &renderingInfo);

		VkViewport viewport = { 0, 0, (float)windowSize.width, (float)windowSize.height, 0, 1 };
		VkRect2D scissor = { {0, 0}, windowSize };
		setGraphicsDynamicState(mainCommandBuffer, viewport, scissor);

		// Bind Shaders & Draw
		cmdBindGraphicsShaders(mainCommandBuffer, shaders[0], shaders[1]);

		const VkDeviceSize offsets[] = { 0 };
		const VkDeviceSize sizes[] = { VK_WHOLE_SIZE };
		// Bind the vertex buffer. vkCmdBindVertexBuffers2 (Vulkan 1.3 core) extends the
		// older vkCmdBindVertexBuffers with optional pSizes and pStrides arrays. With
		// shader objects, vertex input layout is dynamic (set via vkCmdSetVertexInputEXT
		// above), so passing explicit sizes/strides here is fine.
		vkCmdBindVertexBuffers2(mainCommandBuffer, 0, 1, &vertexBuffer.buffer, offsets, sizes, nullptr);
		vkCmdBindIndexBuffer(mainCommandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(mainCommandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		vkCmdEndRendering(mainCommandBuffer);

		// Transition the swapchain image back to the present layout
		cmdTransitionSwapchainLayout(mainCommandBuffer, swapchain.GetImage(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	// Ends recording of commands for the frame
	endCommandRecording(mainCommandBuffer);
	// End frame and present
	endFrame(mainCommandBuffer);
}
//=============================================================================