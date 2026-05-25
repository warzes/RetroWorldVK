#include "stdafx.h"
#include "gpu_system.h"
#include "app_window.h"
#include "core_log.h"
#include "_vk_context.h"
#include "_vk_swapchain.h"
#include "_vk_resurces.h"
#include "_vk_debugUtils.h"

#if USE_SLANG
#else
#include "shaders/_autogen/shader.vert.glsl.h"
#include "shaders/_autogen/shader.frag.glsl.h"
#endif
//=============================================================================
namespace
{
	bool              vSync{ false };
	VkExtent2D        windowSize{ 0 };    // The window size

	Context           context{};          // The Vulkan context
	VkSurfaceKHR      surface{ nullptr }; // The window surface
	ResourceAllocator allocator;          // The VMA allocator
	VkCommandPool     transientCmdPool{ nullptr };
	Swapchain         swapchain;          // The swapchain
	struct DepthBuffer final
	{
		VkImage       depthImage{ nullptr };
		VkImageView   depthImageView{ nullptr };
		VmaAllocation depthImageAllocation{ nullptr };
	} depthBuffer;

	// Frame resources and synchronization
	struct FrameData final
	{
		VkCommandPool   cmdPool;          // Command pool for recording commands for this frame
		VkCommandBuffer cmdBuffer;        // Command buffer containing the frame's rendering commands
		uint64_t        lastSignalValue;  // Timeline value last signaled for this frame's resources
	};
	std::vector<FrameData> frameData;     // Collection of per-frame resources to support multiple frames in flight
	VkSemaphore frameTimelineSemaphore{}; // Timeline semaphore used to synchronize CPU submission with GPU completion
	uint64_t    frameCounter{ 1 };        // Monotonic timeline counter (increments each frame)


	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VmaAllocation vBufferAllocation{ VK_NULL_HANDLE };
	VkBuffer vBuffer{ VK_NULL_HANDLE };
	struct ShaderData
	{
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model[3];
		glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
		uint32_t selected{ 1 };
	} shaderData{};
	struct ShaderDataBuffer
	{
		VmaAllocation allocation{ VK_NULL_HANDLE };
		VmaAllocationInfo allocationInfo{};
		VkBuffer buffer{ VK_NULL_HANDLE };
		VkDeviceAddress deviceAddress{};
	};
	constexpr uint32_t maxFramesInFlight{ 2 };
	std::array<ShaderDataBuffer, maxFramesInFlight> shaderDataBuffers;
	struct Texture
	{
		VmaAllocation allocation{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		VkImageView view{ VK_NULL_HANDLE };
		VkSampler sampler{ VK_NULL_HANDLE };
	};
	std::array<Texture, 3> textures{};
	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayoutTex{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSetTex{ VK_NULL_HANDLE };
	glm::vec3 camPos{ 0.0f, 0.0f, -6.0f };
	glm::vec3 objectRotations[3]{};
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
	};
#if USE_SLANG
	Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
#endif


	VkCommandBuffer mainCommandBuffer;
	VkShaderEXT shaders[2];
	struct Vertex2 { float x, y, r, g, b; };
	Buffer vertexBuffer;
	Buffer indexBuffer;
	std::vector<Vertex2> vertices = {
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
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType     = VK_IMAGE_TYPE_2D,
		.format        = depthFormat,
		.extent{.width = windowSize.width, .height = windowSize.height, .depth = 1},
		.mipLevels     = 1,
		.arrayLayers   = 1,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
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
	const uint32_t queueFamily = context.GetGraphicsQueue().familyIndex;

	frameData.resize(numFrames);

	// Initialize timeline semaphore at 0. We'll use a monotonic counter (m_frameCounter) starting at 1.
	const uint64_t initialValue = 0;

	/*--
	 * Create timeline semaphore for GPU-CPU synchronization
	 * This ensures resources aren't overwritten while still in use by the GPU
	-*/
	VkSemaphoreTypeCreateInfo timelineCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue  = initialValue,
	};
	const VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timelineCreateInfo };
	VK_CHECK_FALSE(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frameTimelineSemaphore));
	//DBG_VK_NAME(frameTimelineSemaphore);

	/*--
	 * Create command pools and buffers for each frame
	 * Each frame gets its own command pool to allow parallel command recording while previous frames may still be executing on the GPU
	-*/
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

		if (!AllocateCommandBuffers(frameData[i].cmdBuffer, device, frameData[i].cmdPool))
			return false;
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
		.binding = 0, .stride = sizeof(Vertex2), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, .divisor = 1
	};
	const VkVertexInputAttributeDescription2EXT vertexAttributes[2] = {
	{.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex2, x) },
	{.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex2, r) }
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
#if USE_SLANG
	// Initialize Slang shader compiler
	slang::createGlobalSession(slangGlobalSession.writeRef());
	auto slangTargets{ std::to_array<slang::TargetDesc>({ {.format{SLANG_SPIRV}, .profile{slangGlobalSession->findProfile("spirv_1_4")} } }) };
	auto slangOptions{ std::to_array<slang::CompilerOptionEntry>({ { slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1} } }) };
	slang::SessionDesc slangSessionDesc{ .targets{slangTargets.data()}, .targetCount{SlangInt(slangTargets.size())}, .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR, .compilerOptionEntries{slangOptions.data()}, .compilerOptionEntryCount{uint32_t(slangOptions.size())} };
	// Load shader
	Slang::ComPtr<slang::ISession> slangSession;
	slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef());
	Slang::ComPtr<slang::IModule> slangModule{ slangSession->loadModuleFromSource("triangle", "assets/shader.slang", nullptr, nullptr) };
	Slang::ComPtr<ISlangBlob> spirv;
	slangModule->getTargetCode(0, spirv.writeRef());
	VkShaderModuleCreateInfo shaderModuleCI{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = spirv->getBufferSize(), .pCode = (uint32_t*)spirv->getBufferPointer() };

	const char* vertEntryName = "vertexMain";
	const char* fragEntryName = "fragmentMain";
	const std::span<const uint32_t> vertCode{ (uint32_t*)spirv->getBufferPointer(), spirv->getBufferSize() };
	const std::span<const uint32_t> fragCode{ (uint32_t*)spirv->getBufferPointer(), spirv->getBufferSize() };
#else
	const char* vertEntryName = "main";
	const char* fragEntryName = "main";
	const std::span<const uint32_t> vertCode{ shader_vert_glsl, std::size(shader_vert_glsl) };
	const std::span<const uint32_t> fragCode{ shader_frag_glsl, std::size(shader_frag_glsl) };
#endif

	// Three unlinked shader objects. nextStage is a hint to the driver about
	// which stage will follow at bind time -- it doesn't constrain what we
	// can actually bind.
	const VkShaderCreateFlagsEXT commonFlags = VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT;

	const VkShaderCreateInfoEXT vertCreateInfo{
		.sType                  = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
		.flags                  = commonFlags,
		.stage                  = VK_SHADER_STAGE_VERTEX_BIT,
		.nextStage              = VK_SHADER_STAGE_FRAGMENT_BIT,
		.codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT,
		.codeSize               = vertCode.size() * sizeof(uint32_t),
		.pCode                  = vertCode.data(),
		.pName                  = vertEntryName,
		.setLayoutCount         = 0,  // Descriptor heap: no descriptor set layouts
		.pSetLayouts            = nullptr,
		.pushConstantRangeCount = 0,  // Push data (vkCmdPushDataEXT) is used instead
		.pPushConstantRanges    = nullptr,
		.pSpecializationInfo    = nullptr,  // Vertex shader has no spec constants
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

	// Surface
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
		.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = createInfo.instance,
		.hwnd      = createInfo.hwnd, };
	VK_CHECK_FALSE(vkCreateWin32SurfaceKHR(context.GetInstance(), &surfaceCreateInfo, nullptr, &surface));

	// VMA
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

	// SwapChain
	swapchain.Init(context.GetPhysicalDevice(), context.GetDevice(), context.GetGraphicsQueue(), surface, transientCmdPool);
	windowSize = swapchain.InitResources(vSync); // Update the window size to the actual size of the surface
	if (!initDepthBuffer())
		return false;

	// Create what is needed to submit the scene for each frame in-flight m_frameData is sized by frames-in-flight (CPU parallelism), NOT by imageCount (GPU/presentation parallelism).
	if (!createFrameSubmission(swapchain.GetFramesInFlight()))
		return false;

	createGraphicsShaders();

	{
		// Mesh data
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, "assets/suzanne.obj"))
		{
			return false;
		}
		const VkDeviceSize indexCount{ shapes[0].mesh.indices.size() };
		std::vector<Vertex> vertices{};
		std::vector<uint16_t> indices{};
		// Load vertex and index data
		for (auto& index : shapes[0].mesh.indices) {
			Vertex v{
				.pos = { attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1], attrib.vertices[index.vertex_index * 3 + 2] },
				.normal = { attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1], attrib.normals[index.normal_index * 3 + 2] },
				.uv = { attrib.texcoords[index.texcoord_index * 2], 1.0 - attrib.texcoords[index.texcoord_index * 2 + 1] }
			};
			vertices.push_back(v);
			indices.push_back(indices.size());
		}
		VkDeviceSize vBufSize{ sizeof(Vertex) * vertices.size() };
		VkDeviceSize iBufSize{ sizeof(uint16_t) * indices.size() };
		VkBufferCreateInfo bufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = vBufSize + iBufSize, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT };
		VmaAllocationCreateInfo vBufferAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
		VmaAllocationInfo vBufferAllocInfo{};
		VK_CHECK_FALSE(vmaCreateBuffer(allocator, &bufferCI, &vBufferAllocCI, &vBuffer, &vBufferAllocation, &vBufferAllocInfo));
		memcpy(vBufferAllocInfo.pMappedData, vertices.data(), vBufSize);
		memcpy(((char*)vBufferAllocInfo.pMappedData) + vBufSize, indices.data(), iBufSize);
		
		// Shader data buffers
		for (auto i = 0; i < maxFramesInFlight; i++)
		{
			VkBufferCreateInfo uBufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sizeof(ShaderData), .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT };
			VmaAllocationCreateInfo uBufferAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
			VK_CHECK_FALSE(vmaCreateBuffer(allocator, &uBufferCI, &uBufferAllocCI, &shaderDataBuffers[i].buffer, &shaderDataBuffers[i].allocation, &shaderDataBuffers[i].allocationInfo));
			VkBufferDeviceAddressInfo uBufferBdaInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = shaderDataBuffers[i].buffer };
			shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(context.GetDevice(), &uBufferBdaInfo);
		}
	}

		std::vector<VkDescriptorImageInfo> textureDescriptors{};
	{
		// Texture images
		for (auto i = 0; i < textures.size(); i++) {
			ktxTexture* ktxTexture{ nullptr };
			std::string filename = "assets/suzanne" + std::to_string(i) + ".ktx";
			/*Текстуры, которые мы загружаем, имеют 8-битный формат RGBA на канал, хотя альфа-канал нам не нужен. Может возникнуть соблазн использовать вместо него RGB, чтобы сэкономить память, но этот формат поддерживается не везде. Если вы использовали такие форматы в OpenGL, драйвер часто незаметно преобразовывал их в RGBA. В Vulkan попытка использовать неподдерживаемый формат приведет к ошибке.*/
			ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
			VkImageCreateInfo texImgCI{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = ktxTexture_GetVkFormat(ktxTexture),
				.extent = {.width = ktxTexture->baseWidth, .height = ktxTexture->baseHeight, .depth = 1 },
				.mipLevels = ktxTexture->numLevels,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			};
			VmaAllocationCreateInfo texImageAllocCI{ .usage = VMA_MEMORY_USAGE_AUTO };
			VK_CHECK_FALSE(vmaCreateImage(allocator, &texImgCI, &texImageAllocCI, &textures[i].image, &textures[i].allocation, nullptr));
			VkImageViewCreateInfo texVewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = textures[i].image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = texImgCI.format, .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 } };
			VK_CHECK_FALSE(vkCreateImageView(context.GetDevice(), &texVewCI, nullptr, &textures[i].view));
			// Upload
			/*
			Расширения, которые упростили бы эту задачу, — это VK_EXT_host_image_copy, позволяющее копировать данные изображения напрямую с центрального процессора без использования командного буфера, и VK_KHR_unified_image_layouts, упрощающее работу с макетами изображений. Эти расширения пока не получили широкого распространения, но в будущем они могут упростить использование Vulkan.
			*/
			VkBuffer imgSrcBuffer{};
			VmaAllocation imgSrcAllocation{};
			VkBufferCreateInfo imgSrcBufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = (uint32_t)ktxTexture->dataSize, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
			VmaAllocationCreateInfo imgSrcAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO };
			VmaAllocationInfo imgSrcAllocInfo{};
			VK_CHECK_FALSE(vmaCreateBuffer(allocator, &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation, &imgSrcAllocInfo));
			memcpy(imgSrcAllocInfo.pMappedData, ktxTexture->pData, ktxTexture->dataSize);
			VkFenceCreateInfo fenceOneTimeCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			VkFence fenceOneTime{};
			VK_CHECK_FALSE(vkCreateFence(context.GetDevice(), &fenceOneTimeCI, nullptr, &fenceOneTime));
			VkCommandBuffer cbOneTime{};
			VkCommandBufferAllocateInfo cbOneTimeAI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = transientCmdPool, .commandBufferCount = 1 };
			VK_CHECK_FALSE(vkAllocateCommandBuffers(context.GetDevice(), &cbOneTimeAI, &cbOneTime));
			VkCommandBufferBeginInfo cbOneTimeBI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
			VK_CHECK_FALSE(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));
			VkImageMemoryBarrier2 barrierTexImage{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
				.srcAccessMask = VK_ACCESS_2_NONE,
				.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.image = textures[i].image,
				.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 }
			};
			VkDependencyInfo barrierTexInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrierTexImage };
			vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
			std::vector<VkBufferImageCopy> copyRegions{};
			for (auto j = 0; j < ktxTexture->numLevels; j++) {
				ktx_size_t mipOffset{ 0 };
				KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, j, 0, 0, &mipOffset);
				copyRegions.push_back({
					.bufferOffset = mipOffset,
					.imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = (uint32_t)j, .layerCount = 1},
					.imageExtent{.width = ktxTexture->baseWidth >> j, .height = ktxTexture->baseHeight >> j, .depth = 1 },
					});
			}
			vkCmdCopyBufferToImage(cbOneTime, imgSrcBuffer, textures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
			VkImageMemoryBarrier2 barrierTexRead{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
				.image = textures[i].image,
				.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 }
			};
			barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
			vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
			VK_CHECK_FALSE(vkEndCommandBuffer(cbOneTime));
			VkSubmitInfo oneTimeSI{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cbOneTime };
			VK_CHECK_FALSE(vkQueueSubmit(context.GetGraphicsQueue().queue, 1, &oneTimeSI, fenceOneTime));
			VK_CHECK_FALSE(vkWaitForFences(context.GetDevice(), 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
			vkDestroyFence(context.GetDevice(), fenceOneTime, nullptr);
			vmaDestroyBuffer(allocator, imgSrcBuffer, imgSrcAllocation);
			// Sampler
			VkSamplerCreateInfo samplerCI{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
				.anisotropyEnable = VK_TRUE,
				.maxAnisotropy = 8.0f,
				.maxLod = (float)ktxTexture->numLevels,
			};
			VK_CHECK_FALSE(vkCreateSampler(context.GetDevice(), &samplerCI, nullptr, &textures[i].sampler));
			ktxTexture_Destroy(ktxTexture);
			textureDescriptors.push_back({ .sampler = textures[i].sampler, .imageView = textures[i].view, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL });
		}
	}
	{
		// Descriptor (indexing)
		VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
		VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .bindingCount = 1, .pBindingFlags = &descVariableFlag };
		VkDescriptorSetLayoutBinding descLayoutBindingTex{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<uint32_t>(textures.size()), .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
		VkDescriptorSetLayoutCreateInfo descLayoutTexCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = &descBindingFlags, .bindingCount = 1, .pBindings = &descLayoutBindingTex };
		VK_CHECK_FALSE(vkCreateDescriptorSetLayout(context.GetDevice(), &descLayoutTexCI, nullptr, &descriptorSetLayoutTex));
		VkDescriptorPoolSize poolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<uint32_t>(textures.size()) };
		VkDescriptorPoolCreateInfo descPoolCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize };
		VK_CHECK_FALSE(vkCreateDescriptorPool(context.GetDevice(), &descPoolCI, nullptr, &descriptorPool));
		uint32_t variableDescCount{ static_cast<uint32_t>(textures.size()) };
		VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT, .descriptorSetCount = 1, .pDescriptorCounts = &variableDescCount };
		VkDescriptorSetAllocateInfo texDescSetAlloc{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = &variableDescCountAI, .descriptorPool = descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &descriptorSetLayoutTex };
		VK_CHECK_FALSE(vkAllocateDescriptorSets(context.GetDevice(), &texDescSetAlloc, &descriptorSetTex));
		VkWriteDescriptorSet writeDescSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSetTex, .dstBinding = 0, .descriptorCount = static_cast<uint32_t>(textureDescriptors.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = textureDescriptors.data() };
		vkUpdateDescriptorSets(context.GetDevice(), 1, &writeDescSet, 0, nullptr);
	}

	// Buffer (VMA)
	{
		VkCommandBuffer cmd = BeginSingleTimeCommands(context.GetDevice(), transientCmdPool);

		vertexBuffer = allocator.CreateBufferAndUploadData(cmd, std::span<Vertex2>(vertices),
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
		// Bind the vertex buffer. vkCmdBindVertexBuffers2 (Vulkan 1.3 core) extends the older vkCmdBindVertexBuffers with optional pSizes and pStrides arrays. With shader objects, vertex input layout is dynamic (set via vkCmdSetVertexInputEXT above), so passing explicit sizes/strides here is fine.
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