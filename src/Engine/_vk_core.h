#pragma once

#include "core_log.h"

inline std::string VkResultStr(VkResult input_value)
{
	switch (input_value)
	{
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_NOT_READY: return "VK_NOT_READY";
	case VK_TIMEOUT: return "VK_TIMEOUT";
	case VK_EVENT_SET: return "VK_EVENT_SET";
	case VK_EVENT_RESET: return "VK_EVENT_RESET";
	case VK_INCOMPLETE: return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
	case VK_ERROR_VALIDATION_FAILED: return "VK_ERROR_VALIDATION_FAILED";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
	case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
	case VK_ERROR_NOT_PERMITTED: return "VK_ERROR_NOT_PERMITTED";
	case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
	case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT: return "VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
	case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
	case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
	case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
	case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
	case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
	case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
	case VK_PIPELINE_BINARY_MISSING_KHR: return "VK_PIPELINE_BINARY_MISSING_KHR";
	case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
	default: return "Unhandled VkResult";
	}
}

#define VK_CHECK_FALSE(vkFnc)                                               \
	do                                                                      \
	{                                                                       \
		if(const VkResult checkResult = (vkFnc); checkResult != VK_SUCCESS) \
		{                                                                   \
			std::string errMsg = VkResultStr(checkResult);                  \
			core::Fatal("Vulkan error at " + std::string(__FILE__) + ":"    \
						+ std::to_string(__LINE__) + ": " + errMsg);        \
			return false;                                                   \
		}                                                                   \
	} while(0)

// Helper to chain Vulkan structures to the pNext chain
// Uses VkBaseOutStructure for type-safe chaining following Vulkan conventions
template <typename MainT, typename NewT>
inline void pNextChainPushFront(MainT* mainStruct, NewT* newStruct)
{
	// Cast to VkBaseOutStructure for proper pNext handling
	auto* newBase = reinterpret_cast<VkBaseOutStructure*>(newStruct);
	auto* mainBase = reinterpret_cast<VkBaseOutStructure*>(mainStruct);

	newBase->pNext = mainBase->pNext;
	mainBase->pNext = newBase;
}

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
	VkBaseInStructure* BuildPNextChain()
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

/*--
* A queue is a sequence of commands that are executed in order.
* The queue is used to submit command buffers to the GPU.
* The family index is used to identify the queue family (graphic, compute, transfer, ...) .
* The queue index is used to identify the queue in the family, multiple queues can be in the same family.
-*/
struct QueueInfo final
{
	uint32_t familyIndex = ~0U; // Family index of the queue (graphic, compute, transfer, ...)
	uint32_t queueIndex = ~0U;  // Index of the queue in the family
	VkQueue  queue{};           // The queue object
};

//--- Format queries ------------------------------------------------------------

/*--
* A helper function to find a supported format from a list of candidates.
* For example, we can use this function to find a supported depth format.
-*/
inline VkFormat FindSupportedFormat(
	VkPhysicalDevice             physicalDevice,
	const std::vector<VkFormat>& candidates,
	VkImageTiling                tiling,
	VkFormatFeatureFlags2        features)
{
	for (const VkFormat format : candidates)
	{
		VkFormatProperties2 props{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
		vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.formatProperties.linearTilingFeatures & features) == features)
		{
			return format;
		}
		if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.formatProperties.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}
	assert(false && "failed to find supported format!");
	return VK_FORMAT_UNDEFINED;
}

/*--
* A helper function to find the depth format that is supported by the physical device.
-*/
inline VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice)
{
	return FindSupportedFormat(physicalDevice,
		{ VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}


//--- Image layout helpers ------------------------------------------------------

/*--
 * Initialize a newly created image to GENERAL layout (used for color/depth buffers)
-*/
inline void cmdInitImageLayout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
{
	const VkImageMemoryBarrier2 barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {aspectMask, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS} };

	const VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

/*--
* Transition swapchain image layout for the presentation/rendering cycle:
* - UNDEFINED -> PRESENT_SRC_KHR (swapchain initialization)
* - PRESENT_SRC_KHR <-> GENERAL (rendering cycle)
-*/
inline void cmdTransitionSwapchainLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkPipelineStageFlags2 srcStage = 0, dstStage = 0;
	VkAccessFlags2        srcAccess = 0, dstAccess = 0;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		// Swapchain initialization
		srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		srcAccess = VK_ACCESS_2_NONE;
		dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstAccess = VK_ACCESS_2_NONE;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		// Before rendering
		srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		srcAccess = VK_ACCESS_2_NONE;
		dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		// After rendering
		srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstAccess = VK_ACCESS_2_NONE;
	}
	else
	{
		assert(false && "Unsupported swapchain layout transition!");
		srcStage = dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		srcAccess = dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	}

	const VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
										.srcStageMask = srcStage,
										.srcAccessMask = srcAccess,
										.dstStageMask = dstStage,
										.dstAccessMask = dstAccess,
										.oldLayout = oldLayout,
										.newLayout = newLayout,
										.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
										.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
										.image = image,
										.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} };

	const VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };

	vkCmdPipelineBarrier2(cmd, &depInfo);
}


//--- Memory barriers -----------------------------------------------------------

/*--
*  This helper returns the access mask for a given stage mask.
-*/
inline VkAccessFlags2 InferAccessMaskFromStage(VkPipelineStageFlags2 stage, bool src)
{
	VkAccessFlags2 access = 0;

	// Shader stages: default to READ|WRITE for src (to flush writes), READ for dst (to consume)
	const bool hasCompute = (stage & VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT) != 0;
	const bool hasFragment = (stage & VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT) != 0;
	const bool hasVertex = (stage & VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT) != 0;
	if (hasCompute || hasFragment || hasVertex)
	{
		access |= src ? (VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT) : VK_ACCESS_2_SHADER_READ_BIT;
	}

	if ((stage & VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT) != 0)
		access |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;  // Always read-only
	if ((stage & VK_PIPELINE_STAGE_2_TRANSFER_BIT) != 0)
		access |= src ? VK_ACCESS_2_TRANSFER_READ_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT;
	assert(access != 0 && "Missing stage implementation");
	return access;
}

/*--
 * This useful function simplifies the addition of buffer barriers, by inferring
 * the access masks from the stage masks, and adding the buffer barrier to the command buffer.
-*/
inline void cmdBufferMemoryBarrier(
	VkCommandBuffer       commandBuffer,
	VkBuffer              buffer,
	VkPipelineStageFlags2 srcStageMask,
	VkPipelineStageFlags2 dstStageMask,
	VkAccessFlags2        srcAccessMask = 0,  // Default to infer if not provided
	VkAccessFlags2        dstAccessMask = 0,  // Default to infer if not provided
	VkDeviceSize          offset = 0,
	VkDeviceSize          size = VK_WHOLE_SIZE,
	uint32_t              srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	uint32_t              dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
{
	// Infer access masks if not explicitly provided
	if (srcAccessMask == 0) srcAccessMask = InferAccessMaskFromStage(srcStageMask, true);
	if (dstAccessMask == 0) dstAccessMask = InferAccessMaskFromStage(dstStageMask, false);

	const std::array<VkBufferMemoryBarrier2, 1> bufferBarrier{ {{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.srcStageMask = srcStageMask,
		.srcAccessMask = srcAccessMask,
		.dstStageMask = dstStageMask,
		.dstAccessMask = dstAccessMask,
		.srcQueueFamilyIndex = srcQueueFamilyIndex,
		.dstQueueFamilyIndex = dstQueueFamilyIndex,
		.buffer = buffer,
		.offset = offset,
		.size = size}} };

	const VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
									.bufferMemoryBarrierCount = uint32_t(bufferBarrier.size()),
									.pBufferMemoryBarriers = bufferBarrier.data() };
	vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

//--- Shader objects ------------------------------------------------------------

/*--
* Bind a (vertex, fragment) shader-object pair with VK_NULL_HANDLE for every
* other graphics stage.
*
* VK_EXT_shader_object requires that, when shader objects are in use, every
* graphics stage on the device is explicitly bound -- even unused ones, which
* must be cleared to VK_NULL_HANDLE. Forgetting this is a common footgun; keep
* this helper as the canonical call site.
-*/
inline void cmdBindGraphicsShaders(VkCommandBuffer cmd, VkShaderEXT vert, VkShaderEXT frag)
{
	const std::array<VkShaderStageFlagBits, 5> stages = {
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	const std::array<VkShaderEXT, 5> shaders = { vert, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, frag };
	vkCmdBindShadersEXT(cmd, uint32_t(stages.size()), stages.data(), shaders.data());
}

//--- Command buffer ------------------------------------------------------------

/*--
* Create a command pool on the given queue family.
*
* Common flags:
*   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT            -- short-lived command buffers
*   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT -- reset individual buffers
*
* Pass 0 for persistent pools that are reset whole-pool each frame (the
* frames-in-flight pattern).
-*/
inline bool CreateCommandPool(VkCommandPool& pool, VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
{
	const VkCommandPoolCreateInfo commandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = flags,
		.queueFamilyIndex = queueFamilyIndex,
	};
	VkResult result = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &pool);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkCreateCommandPool failed: " + VkResultStr(result));
		return false;
	}
	return true;
}

/*-- Simple helper for the creation of a temporary command buffer, use to record the commands to upload data, or transition images. -*/
inline VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool cmdPool)
{
	const VkCommandBufferAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = cmdPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1 };
	VkCommandBuffer cmd{};
	VkResult result = vkAllocateCommandBuffers(device, &allocInfo, &cmd);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkAllocateCommandBuffers failed: " + VkResultStr(result));
		// TODO: return null
	}
	const VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	result = vkBeginCommandBuffer(cmd, &beginInfo);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkBeginCommandBuffer failed: " + VkResultStr(result));
		// TODO: return null
	}
	return cmd;
}

/*--
* Submit the temporary command buffer, wait until the command is finished, and clean up.
* This is a blocking function and should be used only for small operations
--*/
inline bool EndSingleTimeCommands(VkCommandBuffer cmd, VkDevice device, VkCommandPool cmdPool, VkQueue queue)
{
	// Submit and clean up
	VkResult result = vkEndCommandBuffer(cmd);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkEndCommandBuffer failed: " + VkResultStr(result));
		return false;
	}

	// Create fence for synchronization
	const VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	std::array<VkFence, 1>  fence{};
	result = vkCreateFence(device, &fenceInfo, nullptr, fence.data());
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkCreateFence failed: " + VkResultStr(result));
		return false;
	}

	const VkCommandBufferSubmitInfo cmdBufferInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = cmd };
	const std::array<VkSubmitInfo2, 1> submitInfo{
		{{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, .commandBufferInfoCount = 1, .pCommandBufferInfos = &cmdBufferInfo}} };
	result = vkQueueSubmit2(queue, uint32_t(submitInfo.size()), submitInfo.data(), fence[0]);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkQueueSubmit2 failed: " + VkResultStr(result));
		return false;
	}
	result = vkWaitForFences(device, uint32_t(fence.size()), fence.data(), VK_TRUE, UINT64_MAX);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkWaitForFences failed: " + VkResultStr(result));
		return false;
	}

	// Cleanup
	vkDestroyFence(device, fence[0], nullptr);
	vkFreeCommandBuffers(device, cmdPool, 1, &cmd);

	return true;
}
