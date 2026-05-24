#include "stdafx.h"
#include "_vk_context.h"
#include "_vk_debugUtils.h"
#include "core_log.h"
//=============================================================================
inline bool extensionIsAvailable(const std::string& name, const std::vector<VkExtensionProperties>& extensions)
{
	for (auto& ext : extensions)
	{
		if (name == ext.extensionName)
			return true;
	}
	return false;
}
//=============================================================================
bool Context::Init()
{
	VK_CHECK_FALSE(volkInitialize());
	if (!initInstance()) return false;
	if (!selectPhysicalDevice()) return false;
	if (!initLogicalDevice()) return false;

	return true;
}
//=============================================================================
void Context::Close()
{
	if (m_device) vkDeviceWaitIdle(m_device);
	if (m_instance && enableValidationLayers && vkDestroyDebugUtilsMessengerEXT)
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_callback, nullptr);

	if (m_device) vkDestroyDevice(m_device, nullptr);
	if (m_instance) vkDestroyInstance(m_instance, nullptr);
	*this = {};
}
//=============================================================================
VKAPI_ATTR VkBool32 VKAPI_CALL Context::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
{
	if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
		core::Fatal(callbackData->pMessage);
	else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
		core::Warning(callbackData->pMessage);
	else
		core::Print(callbackData->pMessage);

	return VK_FALSE;
}
//=============================================================================
bool Context::initInstance()
{
	VK_CHECK_FALSE(vkEnumerateInstanceVersion(&m_apiVersion));

	core::Print("VULKAN API: " + std::to_string(VK_VERSION_MAJOR(m_apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(m_apiVersion)));
	if (m_apiVersion < VK_MAKE_API_VERSION(0, 1, 4, 0))
	{
		core::Fatal("Require Vulkan 1.4 loader");
		return false;
	}

	if (!getAvailableInstanceExtensions())
		return false;

	// Build instance extensions list from config
	std::vector<const char*> instanceExtensions = {
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
		"VK_KHR_win32_surface"
#else
#error "not support"
#endif
	};

	// Add optional instance extensions if available
	if (extensionIsAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, m_instanceExtensionsAvailable))
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	if (extensionIsAvailable(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME, m_instanceExtensionsAvailable))
		instanceExtensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);

	// Build instance layers list from config
	std::vector<const char*> instanceLayers;
	// Adding the validation layer
	if (enableValidationLayers)
		instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

	const VkApplicationInfo applicationInfo{
		.pApplicationName = "GameApp",
		.applicationVersion = 1,
		.pEngineName = "Engine",
		.engineVersion = 1,
		.apiVersion = m_apiVersion,
	};

	// Setting for the validation layer
	ValidationSettings validationSettings{ .validate_core = VK_TRUE };  // modify default value

	const VkInstanceCreateInfo instanceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = validationSettings.BuildPNextChain(),
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = uint32_t(instanceLayers.size()),
		.ppEnabledLayerNames = instanceLayers.data(),
		.enabledExtensionCount = uint32_t(instanceExtensions.size()),
		.ppEnabledExtensionNames = instanceExtensions.data(),
	};

	// Actual Vulkan instance creation
	VK_CHECK_FALSE(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

	// Load all Vulkan functions
	volkLoadInstance(m_instance);

	// Add the debug callback
	if (enableValidationLayers && vkCreateDebugUtilsMessengerEXT)
	{
		const VkDebugUtilsMessengerCreateInfoEXT dbg_messenger_create_info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		.pfnUserCallback = Context::debugCallback,  // <-- The callback function
		};
		VK_CHECK_FALSE(vkCreateDebugUtilsMessengerEXT(m_instance, &dbg_messenger_create_info, nullptr, &m_callback));
		core::Print("Validation Layers: ON");
	}

	return true;
}
//=============================================================================
bool Context::selectPhysicalDevice()
{
	size_t chosenDevice = 0;

	uint32_t deviceCount = 0;
	VK_CHECK_FALSE(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
	if (deviceCount == 0)
	{
		core::Fatal("failed to find GPUs with Vulkan support!");
		return false;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	VK_CHECK_FALSE(vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data()));

	VkPhysicalDeviceProperties2 properties2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	for (size_t i = 0; i < physicalDevices.size(); i++)
	{
		vkGetPhysicalDeviceProperties2(physicalDevices[i], &properties2);
		if (properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			chosenDevice = i;
			break;
		}
	}

	m_physicalDevice = physicalDevices[chosenDevice];
	vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties2);
	m_deviceApiVersion = properties2.properties.apiVersion;
	core::Print("Selected GPU: " + std::string(properties2.properties.deviceName));  // Show the name of the GPU
	core::Print("Driver: "
		+ std::to_string(VK_VERSION_MAJOR(properties2.properties.driverVersion)) + "."
		+ std::to_string(VK_VERSION_MINOR(properties2.properties.driverVersion)) + "."
		+ std::to_string(VK_VERSION_PATCH(properties2.properties.driverVersion)));
	core::Print("Vulkan API: "
		+ std::to_string(VK_VERSION_MAJOR(properties2.properties.apiVersion)) + "."
		+ std::to_string(VK_VERSION_MINOR(properties2.properties.apiVersion)) + "."
		+ std::to_string(VK_VERSION_PATCH(properties2.properties.apiVersion)));

	if (properties2.properties.apiVersion < VK_MAKE_API_VERSION(0, 1, 4, 0))
	{
		core::Fatal("Require Vulkan 1.4 device, update driver!");
		return false;
	}

	return true;
}
//=============================================================================
QueueInfo Context::getQueue(VkQueueFlagBits flags) const
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount, { .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
	vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

	QueueInfo queueInfo;
	for (uint32_t i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFamilyProperties.queueFlags & flags)
		{
			queueInfo.familyIndex = i;
			queueInfo.queueIndex = 0;  // We only request one queue per family; for multiple
			// queues, raise queueCount in VkDeviceQueueCreateInfo
			// and pick the desired queueIndex here.
			// queueInfo.queue is filled in after creating the logical device.
			break;
		}
	}

	return queueInfo;
}
//=============================================================================
bool Context::initLogicalDevice()
{
	const float queuePriority = 1.0F;
	m_queues.clear();
	m_queues.emplace_back(getQueue(VK_QUEUE_GRAPHICS_BIT));

	// Request only one queue : graphic
	// User could request more specific queues: compute, transfer
	const VkDeviceQueueCreateInfo queueCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = m_queues[0].familyIndex,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};

	// Chaining all features up to Vulkan 1.4
	pNextChainPushFront(&m_features11, &m_features12);
	pNextChainPushFront(&m_features11, &m_features13);
	pNextChainPushFront(&m_features11, &m_features14);

	/*--
	 * Process device extensions from configuration:
	 * - Check if each extension is available on the device
	 * - Enable required extensions (assert if not available)
	 * - Enable optional extensions (skip if not available)
	 * - Link provided feature structs to the pNext chain
	-*/
	if (!getAvailableDeviceExtensions())
		return false;

	struct ExtensionConfig final
	{
		const char* name = nullptr;
		bool        required = false;
		void* featureStruct = nullptr;
	};
	std::vector<ExtensionConfig> reqDeviceExtensions;

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

	// Required extensions (with their feature struct pointers)
	reqDeviceExtensions.push_back({ VK_KHR_SWAPCHAIN_EXTENSION_NAME, true, nullptr });
	reqDeviceExtensions.push_back({ VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME, true, &unifiedImageLayoutsFeature });
	reqDeviceExtensions.push_back({ VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME, true, &descriptorHeapFeatures });  // Bindless descriptor heap for textures and samplers
	reqDeviceExtensions.push_back({ VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME, true, &untypedPtrFeatures });  // Required by bindless
	reqDeviceExtensions.push_back({ VK_EXT_SHADER_OBJECT_EXTENSION_NAME, true, &shaderObjectFeatures });  // Graphics: shader objects instead of pipelines
	reqDeviceExtensions.push_back({ VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME, true, &dynamicState3Features });  // Required for shader-object blend/rasterization state
	reqDeviceExtensions.push_back({ VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, true,  &vertexInputDynamicStateFeatures });  // Required for shader-object vertex input

	std::vector<const char*> deviceExtensions;
	for (const auto& extConfig : reqDeviceExtensions)
	{
		if (extensionIsAvailable(extConfig.name, m_deviceExtensionsAvailable))
		{
			deviceExtensions.push_back(extConfig.name);

			// Link feature struct if provided via ExtensionConfig::featureStruct
			if (extConfig.featureStruct != nullptr)
			{
				pNextChainPushFront(&m_features11, extConfig.featureStruct);
			}
		}
		else if (extConfig.required)
		{
			// Extension is required but not available - fail with error message
			core::Fatal("Required extension " + std::string(extConfig.name) + " is not available!");
			return false;
		}
		else
		{
			// Extension is optional and not available - skip it
			core::Warning("Optional extension " + std::string(extConfig.name) + " is not available, skipping");
		}
	}

	// Requesting all supported features, which will then be activated in the device
	m_deviceFeatures.pNext = &m_features11;
	vkGetPhysicalDeviceFeatures2(m_physicalDevice, &m_deviceFeatures);

	// Validate required features - these are mandatory in Vulkan 1.4, but some drivers claim 1.4 support without full conformance. Check to catch non-conformant drivers early.
	if (!m_features12.timelineSemaphore)
	{
		core::Fatal("Timeline semaphore required (Vulkan 1.2 core)");
		return false;
	}
	if (!m_features12.bufferDeviceAddress)
	{
		core::Fatal("Buffer device address required (used pervasively in this sample)");
		return false;
	}
	if (!m_features13.synchronization2)
	{
		core::Fatal("Synchronization2 required (Vulkan 1.3 core)");
		return false;
	}
	if (!m_features13.dynamicRendering)
	{
		core::Fatal("Dynamic rendering required (Vulkan 1.3 core)");
		return false;
	}
	if (!m_features14.maintenance5)
	{
		core::Fatal("Maintenance5 required (Vulkan 1.4 core)");
		return false;
	}
	if (!m_features14.maintenance6)
	{
		core::Fatal("Maintenance6 required (Vulkan 1.4 core)");
		return false;
	}

	// Create the logical device
	const VkDeviceCreateInfo deviceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &m_deviceFeatures,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCreateInfo,
		.enabledExtensionCount = uint32_t(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
	};
	VK_CHECK_FALSE(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));
	DBG_VK_NAME(m_device);

	volkLoadDevice(m_device);  // Load all Vulkan device functions

	// Debug utility to name Vulkan objects, great in debugger like NSight
	debugUtilInitialize(m_device);

	// Get the requested queues
	vkGetDeviceQueue(m_device, m_queues[0].familyIndex, m_queues[0].queueIndex, &m_queues[0].queue);
	DBG_VK_NAME(m_queues[0].queue);

	// Log the enabled extensions
	core::Print("Enabled device extensions:");
	for (const auto& ext : deviceExtensions)
		core::Print("  " + std::string(ext));

	return true;
}
//=============================================================================
bool Context::getAvailableInstanceExtensions()
{
	uint32_t count{ 0 };
	VK_CHECK_FALSE(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
	m_instanceExtensionsAvailable.resize(count);
	VK_CHECK_FALSE(vkEnumerateInstanceExtensionProperties(nullptr, &count, m_instanceExtensionsAvailable.data()));

	return true;
}
//=============================================================================
bool Context::getAvailableDeviceExtensions()
{
	uint32_t count{ 0 };
	VK_CHECK_FALSE(vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &count, nullptr));
	m_deviceExtensionsAvailable.resize(count);
	VK_CHECK_FALSE(vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &count, m_deviceExtensionsAvailable.data()));

	return true;
}
//=============================================================================