#include "stdafx.h"
#include "gpu_system.h"
#include "_gpu_core.h"
#include "app_window.h"
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif
#define APP_USE_UNLIMITED_FRAME_RATE
//=============================================================================
namespace
{
	static VkAllocationCallbacks*   Allocator{ nullptr };
	static VkInstance               Instance{ nullptr };
	static VkPhysicalDevice         PhysicalDevice{ nullptr };
	static VkDevice                 Device{ nullptr };
	static uint32_t                 QueueFamily = (uint32_t)-1;
	static VkQueue                  Queue{ nullptr };
	static VkDebugReportCallbackEXT DebugReport{ nullptr };
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
#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	core::Print("[vulkan] Debug report from ObjectType: " + std::to_string(objectType) + "\nMessage: " + pMessage);
	return VK_FALSE;
}
#endif // APP_USE_VULKAN_DEBUG_REPORT
//=============================================================================
static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension)
{
	for (const VkExtensionProperties& p : properties)
		if (strcmp(p.extensionName, extension) == 0)
			return true;
	return false;
}
//=============================================================================
static bool SetupVulkan(ImVector<const char*> instance_extensions)
{
	VK_CHECK_FALSE(volkInitialize());

	// Create Vulkan Instance
	{
		VkInstanceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

		// Enumerate available extensions
		uint32_t properties_count;
		ImVector<VkExtensionProperties> properties;
		vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
		properties.resize(properties_count);
		VK_CHECK_FALSE(vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data));

		// Enable required extensions
		if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
			instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
		if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
		{
			instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}
#endif

		// Enabling validation layers
#ifdef APP_USE_VULKAN_DEBUG_REPORT
		const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = layers;
		instance_extensions.push_back("VK_EXT_debug_report");
#endif

		// Create Vulkan Instance
		create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
		create_info.ppEnabledExtensionNames = instance_extensions.Data;
		VK_CHECK_FALSE(vkCreateInstance(&create_info, Allocator, &Instance));

		volkLoadInstance(Instance);

		// Setup the debug report callback
#ifdef APP_USE_VULKAN_DEBUG_REPORT
		auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(Instance, "vkCreateDebugReportCallbackEXT");
		IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
		VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
		debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		debug_report_ci.pfnCallback = debug_report;
		debug_report_ci.pUserData = nullptr;
		VK_CHECK_FALSE(f_vkCreateDebugReportCallbackEXT(Instance, &debug_report_ci, Allocator, &DebugReport));
#endif
	}

	// Select Physical Device (GPU)
	PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(Instance);
	IM_ASSERT(PhysicalDevice != VK_NULL_HANDLE);

	// Select graphics queue family
	QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(PhysicalDevice);
	IM_ASSERT(QueueFamily != (uint32_t)-1);

	// Create Logical Device (with 1 queue)
	{
		ImVector<const char*> device_extensions;
		device_extensions.push_back("VK_KHR_swapchain");

		// Enumerate physical device extension
		uint32_t properties_count;
		ImVector<VkExtensionProperties> properties;
		vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &properties_count, nullptr);
		properties.resize(properties_count);
		vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
		if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
			device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

		const float queue_priority[] = { 1.0f };
		VkDeviceQueueCreateInfo queue_info[1] = {};
		queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info[0].queueFamilyIndex = QueueFamily;
		queue_info[0].queueCount = 1;
		queue_info[0].pQueuePriorities = queue_priority;
		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
		create_info.pQueueCreateInfos = queue_info;
		create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
		create_info.ppEnabledExtensionNames = device_extensions.Data;
		VK_CHECK_FALSE(vkCreateDevice(PhysicalDevice, &create_info, Allocator, &Device));
		vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);
	}

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
		VK_CHECK_FALSE(vkCreateDescriptorPool(Device, &pool_info, Allocator, &DescriptorPool));
	}

	return true;
}
//=============================================================================
// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
	// Check for WSI support
	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, QueueFamily, surface, &res);
	if (res != VK_TRUE)
	{
		fprintf(stderr, "Error no WSI support on physical device 0\n");
		exit(-1);
	}

	// Select Surface Format
	const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	wd->Surface = surface;
	wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);

	// Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(PhysicalDevice, wd->Surface, &present_modes[0], IM_COUNTOF(present_modes));
	//printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

	// Create SwapChain, RenderPass, Framebuffer, etc.
	IM_ASSERT(MinImageCount >= 2);
	ImGui_ImplVulkanH_CreateOrResizeWindow(Instance, PhysicalDevice, Device, wd, QueueFamily, Allocator, width, height, MinImageCount, 0);
}
//=============================================================================
static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
	VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
	VkResult err = vkAcquireNextImageKHR(Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
		SwapChainRebuild = true;
	if (err == VK_ERROR_OUT_OF_DATE_KHR)
		return;
	//if (err != VK_SUBOPTIMAL_KHR)
	//	check_vk_result(err);

	ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
	{
		err = vkWaitForFences(Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
		//check_vk_result(err);

		err = vkResetFences(Device, 1, &fd->Fence);
		//check_vk_result(err);
	}
	{
		err = vkResetCommandPool(Device, fd->CommandPool, 0);
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
		err = vkQueueSubmit(Queue, 1, &info, fd->Fence);
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
	VkResult err = vkQueuePresentKHR(Queue, &info);
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
	vkDestroyDescriptorPool(Device, DescriptorPool, Allocator);

#ifdef APP_USE_VULKAN_DEBUG_REPORT
	// Remove the debug report callback
	auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(Instance, "vkDestroyDebugReportCallbackEXT");
	f_vkDestroyDebugReportCallbackEXT(Instance, DebugReport, Allocator);
#endif // APP_USE_VULKAN_DEBUG_REPORT

	vkDestroyDevice(Device, Allocator);
	vkDestroyInstance(Instance, Allocator);
}
//=============================================================================
static void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd)
{
	ImGui_ImplVulkanH_DestroyWindow(Instance, Device, wd, Allocator);
	vkDestroySurfaceKHR(Instance, wd->Surface, Allocator);
}
//=============================================================================
bool gpu::Init(const CreateInfo& createInfo)
{
	ImVector<const char*> extensions;
	extensions.push_back("VK_KHR_surface");
	extensions.push_back("VK_KHR_win32_surface");
	if (!SetupVulkan(extensions))
		return false;

	// Create Window Surface
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hwnd = createInfo.hwnd;
	surfaceCreateInfo.hinstance = createInfo.instance;
	if (vkCreateWin32SurfaceKHR(Instance, &surfaceCreateInfo, nullptr, &surface) != VK_SUCCESS)
	{
		printf("Failed to create Vulkan surface.\n");
		return 1;
	}

	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
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
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(createInfo.hwnd);
	ImGui_ImplVulkan_InitInfo init_info = {};
	//init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
	init_info.Instance = Instance;
	init_info.PhysicalDevice = PhysicalDevice;
	init_info.Device = Device;
	init_info.QueueFamily = QueueFamily;
	init_info.Queue = Queue;
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
	// Cleanup
	VkResult err = vkDeviceWaitIdle(Device);
	//check_vk_result(err);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupVulkanWindow(&MainWindowData);
	CleanupVulkan();
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
			ImGui_ImplVulkanH_CreateOrResizeWindow(Instance, PhysicalDevice, Device, &MainWindowData, QueueFamily, Allocator, fbWidth, fbHeight, MinImageCount, 0);
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