#include "stdafx.h"
#include "gpu_system.h"
#include "_vk_core.h"
#include "app_window.h"
#include "_vk_context.h"
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif
#define APP_USE_UNLIMITED_FRAME_RATE
//=============================================================================
namespace
{
	static VkAllocationCallbacks*   Allocator{ nullptr };
	Context                         context{};
	//static VkInstance               Instance{ nullptr };
	//static VkPhysicalDevice         PhysicalDevice{ nullptr };
	//static VkDevice                 Device{ nullptr };
	//static uint32_t                 QueueFamily = (uint32_t)-1;
	//static VkQueue                  Queue{ nullptr };
	//static VkDebugReportCallbackEXT DebugReport{ nullptr };
	static VkSurfaceKHR             surface{ nullptr };
	static VkPipelineCache          PipelineCache{ nullptr };
	static VkDescriptorPool         DescriptorPool{ nullptr };

	static ImGui_ImplVulkanH_Window MainWindowData;
	static uint32_t                 MinImageCount{ 2 };
	static bool                     SwapChainRebuild{ false };

	uint16_t fbWidth{ 0 };
	uint16_t fbHeight{ 0 };
}
//=============================================================================
static bool SetupVulkan()
{
	// Create Descriptor Pool
	// If you wish to load e.g. additional textures you may need to alter pools sizes and maxSets.
	{
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 0;
		for (VkDescriptorPoolSize& pool_size : pool_sizes)
			pool_info.maxSets += pool_size.descriptorCount;
		pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		VK_CHECK_FALSE(vkCreateDescriptorPool(context.GetDevice(), &pool_info, Allocator, &DescriptorPool));
	}

	return true;
}
//=============================================================================
// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
	// Select Surface Format
	const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	wd->Surface = surface;
	wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(context.GetPhysicalDevice(), wd->Surface, requestSurfaceImageFormat, (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);

	// Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(context.GetPhysicalDevice(), wd->Surface, &present_modes[0], IM_COUNTOF(present_modes));
	//printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

	// Create SwapChain, RenderPass, Framebuffer, etc.
	IM_ASSERT(MinImageCount >= 2);
	ImGui_ImplVulkanH_CreateOrResizeWindow(context.GetInstance(), context.GetPhysicalDevice(), context.GetDevice(), wd, context.GetGraphicsQueue().familyIndex, Allocator, width, height, MinImageCount, 0);
}
//=============================================================================
static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
	VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkResult err = vkAcquireNextImageKHR(context.GetDevice(), wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		SwapChainRebuild = true;
	if (err == VK_ERROR_OUT_OF_DATE_KHR)
		return;
	//if (err != VK_SUBOPTIMAL_KHR)
	//	check_vk_result(err);

	ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
	{
		err = vkWaitForFences(context.GetDevice(), 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
		//check_vk_result(err);

		err = vkResetFences(context.GetDevice(), 1, &fd->Fence);
		//check_vk_result(err);
	}
	{
		err = vkResetCommandPool(context.GetDevice(), fd->CommandPool, 0);
		//check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
		//check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = wd->RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = wd->Width;
		info.renderArea.extent.height = wd->Height;
		info.clearValueCount = 1;
		info.pClearValues = &wd->ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record dear imgui primitives into command buffer
	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

	// Submit command buffer
	vkCmdEndRenderPass(fd->CommandBuffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;

		err = vkEndCommandBuffer(fd->CommandBuffer);
		//check_vk_result(err);
		err = vkQueueSubmit(context.GetGraphicsQueue().queue, 1, &info, fd->Fence);
		//check_vk_result(err);
	}
}
//=============================================================================
static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
	if (SwapChainRebuild)
		return;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &render_complete_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &wd->Swapchain;
	info.pImageIndices = &wd->FrameIndex;
	VkResult err = vkQueuePresentKHR(context.GetGraphicsQueue().queue, &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		SwapChainRebuild = true;
	if (err == VK_ERROR_OUT_OF_DATE_KHR)
		return;
	//if (err != VK_SUBOPTIMAL_KHR)
	//	check_vk_result(err);
	wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}
//=============================================================================
static void CleanupVulkan()
{
	vkDestroyDescriptorPool(context.GetDevice(), DescriptorPool, Allocator);
}
//=============================================================================
static void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd)
{
	ImGui_ImplVulkanH_DestroyWindow(context.GetInstance(), context.GetDevice(), wd, Allocator);
	vkDestroySurfaceKHR(context.GetInstance(), wd->Surface, Allocator);
}
//=============================================================================
bool gpu::Init(const CreateInfo& createInfo)
{
	// Vulkan feature structs - allocated on the stack
	VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeature{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR };
	// Descriptor heap replaces traditional descriptor sets/pools with GPU buffer-based bindless descriptors.
	// Samplers and images are written into heap buffers and accessed by index in the shaders.
	VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptorHeapFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT };
	// Untyped pointers: required by descriptor heap
	VkPhysicalDeviceShaderUntypedPointersFeaturesKHR untypedPtrFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR };
	// Shader objects: replace VkPipeline for graphics with linkable, reusable VkShaderEXT
	// objects bound via vkCmdBindShadersEXT. Pairs naturally with the layout=NULL design:
	// there is no graphics pipeline object at all, only shader objects + dynamic state.
	VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT };
	// Extended dynamic state 3: required by shader objects for blend/rasterization
	// state that no longer lives in a pipeline object.
	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamicState3Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT };
	// Vertex input dynamic state: required by shader objects (vertex bindings/attributes
	// become a vkCmdSetVertexInputEXT call instead of pipeline state).
	VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertexInputDynamicStateFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT };

	// Configure Vulkan context with required and optional extensions
	ContextCreateInfo contextConfig;

	// Required extensions (with their feature struct pointers)
	contextConfig.deviceExtensions.push_back({ VK_KHR_SWAPCHAIN_EXTENSION_NAME, true, nullptr });
	contextConfig.deviceExtensions.push_back({ VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME, true, &unifiedImageLayoutsFeature });
	contextConfig.deviceExtensions.push_back({ VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME, true, &descriptorHeapFeatures });  // Bindless descriptor heap for textures and samplers
	contextConfig.deviceExtensions.push_back({ VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME, true, &untypedPtrFeatures });  // Required by bindless
	contextConfig.deviceExtensions.push_back({ VK_EXT_SHADER_OBJECT_EXTENSION_NAME, true, &shaderObjectFeatures });  // Graphics: shader objects instead of pipelines
	contextConfig.deviceExtensions.push_back({ VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME, true, &dynamicState3Features });  // Required for shader-object blend/rasterization state
	contextConfig.deviceExtensions.push_back({ VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, true,  &vertexInputDynamicStateFeatures });  // Required for shader-object vertex input


	if (!context.Init(contextConfig))
		return false;
	if (!SetupVulkan())
		return false;

	// Create Window Surface
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hwnd = createInfo.hwnd;
	surfaceCreateInfo.hinstance = createInfo.instance;
	if (vkCreateWin32SurfaceKHR(context.GetInstance(), &surfaceCreateInfo, nullptr, &surface) != VK_SUCCESS)
	{
		printf("Failed to create Vulkan surface.\n");
		return 1;
	}

	ImGui_ImplVulkanH_Window* wd = &MainWindowData;
	SetupVulkanWindow(wd, surface, window::GetWidth(), window::GetHeight());

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	//style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	//style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(createInfo.hwnd);
	ImGui_ImplVulkan_InitInfo init_info = {};
	//init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
	init_info.Instance = context.GetInstance();
	init_info.PhysicalDevice = context.GetPhysicalDevice();
	init_info.Device = context.GetDevice();
	init_info.QueueFamily = context.GetGraphicsQueue().familyIndex;
	init_info.Queue = context.GetGraphicsQueue().queue;
	init_info.PipelineCache = PipelineCache;
	init_info.DescriptorPool = DescriptorPool;
	init_info.MinImageCount = MinImageCount;
	init_info.ImageCount = wd->ImageCount;
	init_info.Allocator = Allocator;
	init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	ImGui_ImplVulkan_Init(&init_info);

	// Load Fonts
	// - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
	//   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
	// - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//style.FontSizeBase = 20.0f;
	//io.Fonts->AddFontDefaultVector();
	//io.Fonts->AddFontDefaultBitmap();
	//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
	//IM_ASSERT(font != nullptr);

	fbWidth = window::GetWidth();
	fbHeight = window::GetHeight();

	return true;
}
//=============================================================================
void gpu::Close()
{
	CleanupVulkanWindow(&MainWindowData);
	CleanupVulkan();

	ImGui_ImplVulkan_Shutdown();
	context.Close();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
//=============================================================================
void gpu::BeginFrame()
{
	if (fbWidth != window::GetWidth() || fbHeight != window::GetHeight())
	{
		fbWidth = window::GetWidth();
		fbHeight = window::GetHeight();

		if (SwapChainRebuild || MainWindowData.Width != fbWidth || MainWindowData.Height != fbHeight)
		{
			ImGui_ImplVulkan_SetMinImageCount(MinImageCount);
			ImGui_ImplVulkanH_CreateOrResizeWindow(context.GetInstance(), context.GetPhysicalDevice(), context.GetDevice(), &MainWindowData, context.GetGraphicsQueue().familyIndex, Allocator, fbWidth, fbHeight, MinImageCount, 0);
			MainWindowData.FrameIndex = 0;
			SwapChainRebuild = false;
		}
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}
//=============================================================================
void gpu::EndFrame()
{
	ImGui::Begin("Hello, world!");
	ImGui::Text("This is some useful text.");
	ImGui::End();

	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
	if (!is_minimized)
	{
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		MainWindowData.ClearValue.color.float32[0] = clear_color.x * clear_color.w;
		MainWindowData.ClearValue.color.float32[1] = clear_color.y * clear_color.w;
		MainWindowData.ClearValue.color.float32[2] = clear_color.z * clear_color.w;
		MainWindowData.ClearValue.color.float32[3] = clear_color.w;
		FrameRender(&MainWindowData, draw_data);
		FramePresent(&MainWindowData);
	}
}
//=============================================================================