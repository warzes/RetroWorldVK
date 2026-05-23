#pragma once

#include "_vk_core.h"

class Context final
{
public:
	Context() noexcept = default;
	~Context() { assert(m_device == VK_NULL_HANDLE && "Missing deinit()"); }

	bool Init();
	void Close();

	VkDevice         GetDevice() const { return m_device; }
	VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
	VkInstance       GetInstance() const { return m_instance; }
	const QueueInfo& GetGraphicsQueue() const
	{
		assert(!m_queues.empty() && "No queues created. Call init() first.");
		return m_queues[0];
	}
	uint32_t GetApiVersion() const { return m_apiVersion; }              // Instance loader version
	uint32_t GetDeviceApiVersion() const { return m_deviceApiVersion; }  // Selected physical device version

private:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*);

	bool initInstance();
	bool selectPhysicalDevice();
	QueueInfo getQueue(VkQueueFlagBits flags) const;
	bool initLogicalDevice();

	bool getAvailableInstanceExtensions();
	bool getAvailableDeviceExtensions();

	// Validation layers
#ifdef NDEBUG
	bool enableValidationLayers = false;
#else
	bool enableValidationLayers = true;
#endif

	uint32_t m_apiVersion{ 0 };        // The Vulkan instance API version (from vkEnumerateInstanceVersion)
	uint32_t m_deviceApiVersion{ 0 };  // The selected device's API version (from VkPhysicalDeviceProperties2)

	VkInstance                         m_instance{};        // The Vulkan instance
	VkPhysicalDevice                   m_physicalDevice{};  // The physical device (GPU)
	VkDevice                           m_device{};          // The logical device (interface to the physical device)
	std::vector<QueueInfo>             m_queues;            // The queue used to submit command buffers to the GPU
	VkDebugUtilsMessengerEXT           m_callback{ VK_NULL_HANDLE };  // The debug callback
	std::vector<VkExtensionProperties> m_instanceExtensionsAvailable;
	std::vector<VkExtensionProperties> m_deviceExtensionsAvailable;

	// Core features
	VkPhysicalDeviceFeatures2        m_deviceFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	VkPhysicalDeviceVulkan11Features m_features11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	VkPhysicalDeviceVulkan12Features m_features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	VkPhysicalDeviceVulkan13Features m_features13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	VkPhysicalDeviceVulkan14Features m_features14{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
};