#include "stdafx.h"
#include "_vk_context.h"
#include "_vk_debugUtils.h"
//=============================================================================
// Validation settings: to fine tune what is checked
struct ValidationSettings final
{
	VkBool32 fine_grained_locking{ VK_TRUE };
	VkBool32 validate_core{ VK_TRUE };
	VkBool32 check_image_layout{ VK_TRUE };
	VkBool32 check_command_buffer{ VK_TRUE };
	VkBool32 check_object_in_use{ VK_TRUE };
	VkBool32 check_query{ VK_TRUE };
	VkBool32 check_shaders{ VK_TRUE };
	VkBool32 check_shaders_caching{ VK_TRUE };
	VkBool32 unique_handles{ VK_TRUE };
	VkBool32 object_lifetime{ VK_TRUE };
	VkBool32 stateless_param{ VK_TRUE };
	std::vector<const char*> debug_action{ "VK_DBG_LAYER_ACTION_LOG_MSG" };  // "VK_DBG_LAYER_ACTION_DEBUG_OUTPUT", "VK_DBG_LAYER_ACTION_BREAK"
	std::vector<const char*> report_flags{ "error", "warn" };  // Enable both errors and warnings
	std::vector<const char*> message_id_filter{ "WARNING-legacy-gpdp2" };  // Filter: legacy vkGetPhysicalDeviceProperties warning from third-party libs (ImGui/VMA)

	/*--
	 * Build the pNext chain to enable these settings on the validation layer.
	 *
	 * IMPORTANT: the returned pointer is only valid for the lifetime of *this.
	 * It points into m_layerSettingsCreateInfo (and transitively into
	 * m_layerSettings), both of which are members. Callers MUST ensure the
	 * ValidationSettings object outlives any Vulkan call that consumes the
	 * chain (typically: keep it on the stack until after vkCreateInstance).
	-*/
	VkBaseInStructure* buildPNextChain()
	{
		layerSettings = std::vector<VkLayerSettingEXT>{
			{layerName, "fine_grained_locking", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &fine_grained_locking},
			{layerName, "validate_core", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &validate_core},
			{layerName, "check_image_layout", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &check_image_layout},
			{layerName, "check_command_buffer", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &check_command_buffer},
			{layerName, "check_object_in_use", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &check_object_in_use},
			{layerName, "check_query", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &check_query},
			{layerName, "check_shaders", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &check_shaders},
			{layerName, "check_shaders_caching", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &check_shaders_caching},
			{layerName, "unique_handles", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &unique_handles},
			{layerName, "object_lifetime", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &object_lifetime},
			{layerName, "stateless_param", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &stateless_param},
			{layerName, "debug_action", VK_LAYER_SETTING_TYPE_STRING_EXT, uint32_t(debug_action.size()), debug_action.data()},
			{layerName, "report_flags", VK_LAYER_SETTING_TYPE_STRING_EXT, uint32_t(report_flags.size()), report_flags.data()},
			{layerName, "message_id_filter", VK_LAYER_SETTING_TYPE_STRING_EXT, uint32_t(message_id_filter.size()),
			 message_id_filter.data()},

		};
		layerSettingsCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
			.settingCount = uint32_t(layerSettings.size()),
			.pSettings = layerSettings.data(),
		};

		return reinterpret_cast<VkBaseInStructure*>(&layerSettingsCreateInfo);
	}

	static constexpr const char* layerName{ "VK_LAYER_KHRONOS_validation" };
	std::vector<VkLayerSettingEXT> layerSettings;
	VkLayerSettingsCreateInfoEXT   layerSettingsCreateInfo{};
};
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
bool Context::Init(const ContextCreateInfo& createInfo)
{
	m_createInfo = createInfo;
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
	if (m_instance && m_createInfo.enableValidationLayers && vkDestroyDebugUtilsMessengerEXT)
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_callback, nullptr);

	if (m_device) vkDestroyDevice(m_device, nullptr);
	if (m_instance) vkDestroyInstance(m_instance, nullptr);
	*this = {};
}
//=============================================================================
VKAPI_ATTR VkBool32 VKAPI_CALL Context::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
{
	if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
		core::Error(callbackData->pMessage);
	else if((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
		core::Warning(callbackData->pMessage);
	else
		core::Print(callbackData->pMessage);

	if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
	{
#if defined(_MSVC_LANG)
		__debugbreak();
#elif defined(__linux__)
		raise(SIGTRAP);
#endif
	}
	return VK_FALSE;
}
//=============================================================================
bool Context::initInstance()
{
	vkEnumerateInstanceVersion(&m_apiVersion);
	core::Print("VULKAN API: " + std::to_string(VK_VERSION_MAJOR(m_apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(m_apiVersion)));
	if (m_apiVersion < VK_MAKE_API_VERSION(0, 1, 4, 0))
	{
		core::Fatal("Require Vulkan 1.4 loader");
		return false;
	}

	// Build instance extensions list from config
	std::vector<const char*> instanceExtensions = m_createInfo.instanceExtensions;
	
	// KHR surface extensions
	instanceExtensions.push_back("VK_KHR_surface");
#if defined(_WIN32)
	instanceExtensions.push_back("VK_KHR_win32_surface");
#else
#error "not support"
#endif

	if (!getAvailableInstanceExtensions())
		return false;

	// Add optional instance extensions if available
	if (extensionIsAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, m_instanceExtensionsAvailable))
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	if (extensionIsAvailable(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME, m_instanceExtensionsAvailable))
		instanceExtensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);

	// Build instance layers list from config
	std::vector<const char*> instanceLayers = m_createInfo.instanceLayers;

	// Adding the validation layer
	if (m_createInfo.enableValidationLayers)
		instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

	const VkApplicationInfo applicationInfo{
		.pApplicationName   = "minimal_latest",
		.applicationVersion = 1,
		.pEngineName        = "minimal_latest",
		.engineVersion      = 1,
		.apiVersion         = m_apiVersion,
	};

	// Setting for the validation layer
	ValidationSettings validationSettings{ .validate_core = VK_TRUE };  // modify default value

	const VkInstanceCreateInfo instanceCreateInfo{
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext                   = validationSettings.buildPNextChain(),
		.pApplicationInfo        = &applicationInfo,
		.enabledLayerCount       = uint32_t(instanceLayers.size()),
		.ppEnabledLayerNames     = instanceLayers.data(),
		.enabledExtensionCount   = uint32_t(instanceExtensions.size()),
		.ppEnabledExtensionNames = instanceExtensions.data(),
	};

	// Actual Vulkan instance creation
	VK_CHECK_FALSE(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

	// Load all Vulkan functions
	volkLoadInstance(m_instance);

	// Add the debug callback
	if (m_createInfo.enableValidationLayers && vkCreateDebugUtilsMessengerEXT)
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
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		core::Fatal("failed to find GPUs with Vulkan support!");
		return false;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data());

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
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = m_queues[0].familyIndex,
		.queueCount       = 1,
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

	std::vector<const char*> deviceExtensions;
	for (const auto& extConfig : m_createInfo.deviceExtensions)
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

	// Validate required features - these are mandatory in Vulkan 1.4, but some drivers
	// claim 1.4 support without full conformance. Check to catch non-conformant drivers early.
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
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext                   = &m_deviceFeatures,
		.queueCreateInfoCount    = 1,
		.pQueueCreateInfos       = &queueCreateInfo,
		.enabledExtensionCount   = uint32_t(deviceExtensions.size()),
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