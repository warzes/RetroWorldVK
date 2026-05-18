#include "stdafx.h"
#include "gpu_system.h"
#include "_vk_core.h"
#include "app_window.h"
#include "_vk_context.h"
#include "_vk_resourceAllocator.h"
//=============================================================================
namespace
{
	Context           context{};          // The Vulkan context
	ResourceAllocator allocator{};        // The VMA allocator
	VkSurfaceKHR      surface{ nullptr }; // The window surface
	VkExtent2D        windowSize{ 0 };    // The window size
}
//=============================================================================
bool gpu::Init(const CreateInfo& createInfo)
{
	// Vulkan feature structs - allocated on the stack
	VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeature{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR };
	// Descriptor heap replaces traditional descriptor sets/pools with GPU buffer-based bindless descriptors. Samplers and images are written into heap buffers and accessed by index in the shaders.
	VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptorHeapFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT };
	// Untyped pointers: required by descriptor heap
	VkPhysicalDeviceShaderUntypedPointersFeaturesKHR untypedPtrFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR };
	// Shader objects: replace VkPipeline for graphics with linkable, reusable VkShaderEXT objects bound via vkCmdBindShadersEXT. Pairs naturally with the layout=NULL design: there is no graphics pipeline object at all, only shader objects + dynamic state.
	VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT };
	// Extended dynamic state 3: required by shader objects for blend/rasterization state that no longer lives in a pipeline object.
	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamicState3Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT };
	// Vertex input dynamic state: required by shader objects (vertex bindings/attributes become a vkCmdSetVertexInputEXT call instead of pipeline state).
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

	// Create the Vulkan context with configuration
	bool isContextCreateResult = context.Init(contextConfig);

	// After context.Init(), vkGetPhysicalDeviceFeatures2 has populated every feature struct in the pNext chain. Assert the bools we actually depend on -- vkCreateDevice will already have failed if a required feature is missing, but these asserts give a clearer diagnostic on non-conformant drivers and document the hard dependencies at a glance.
	if (!unifiedImageLayoutsFeature.unifiedImageLayouts) core::Fatal("unifiedImageLayouts required (GENERAL attachment usage)");
	if (!descriptorHeapFeatures.descriptorHeap) core::Fatal("descriptorHeap required");
	if (!untypedPtrFeatures.shaderUntypedPointers) core::Fatal("shaderUntypedPointers required (by descriptorHeap)");
	if (!shaderObjectFeatures.shaderObject) core::Fatal("shaderObject required (graphics path)");
	if (!dynamicState3Features.extendedDynamicState3ColorBlendEnable) core::Fatal("extendedDynamicState3 required (shader objects)");
	if (!vertexInputDynamicStateFeatures.vertexInputDynamicState) core::Fatal("vertexInputDynamicState required (shader objects)");

	if (!isContextCreateResult) return false;

	// Initialize the VMA allocator. Pass the *device* API version (not the instance loader
	// version) clamped to the highest version VMA understands in this build (1.4).
	if (!allocator.Init(VmaAllocatorCreateInfo{
		.physicalDevice = context.GetPhysicalDevice(),
		.device = context.GetDevice(),
		.instance = context.GetInstance(),
		.vulkanApiVersion = std::min(context.GetDeviceApiVersion(), VK_API_VERSION_1_4),
		}))
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

	windowSize.width = window::GetWidth();
	windowSize.height = window::GetHeight();

	return true;
}
//=============================================================================
void gpu::Close()
{
	allocator.Close();
	context.Close();
}
//=============================================================================
void gpu::BeginFrame()
{
	if (windowSize.width != window::GetWidth() || windowSize.height != window::GetHeight())
	{
		windowSize.width = window::GetWidth();
		windowSize.height = window::GetHeight();
	}
}
//=============================================================================
void gpu::EndFrame()
{
}
//=============================================================================