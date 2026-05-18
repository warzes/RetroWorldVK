#pragma once

#include "core_log.h"

inline const char* VkResultStr(VkResult input_value)
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

// TODO: удалить VK_CHECK_FALSE
#define VK_CHECK_FALSE(vkFnc)                                               \
	do                                                                      \
	{                                                                       \
		if(const VkResult checkResult = (vkFnc); checkResult != VK_SUCCESS) \
		{                                                                   \
			std::string errMsg = VkResultStr(checkResult);                  \
			core::Error("Vulkan error at " + std::string(__FILE__) + ":"    \
						+ std::to_string(__LINE__) + ": " + errMsg);        \
			assert((checkResult == VK_SUCCESS) && errMsg.c_str());          \
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

// RESOURCE TYPES

/*--
 * A buffer is a region of memory used to store data.
 * It is used to store vertex data, index data, uniform data, and other types of data.
 * There is a VkBuffer object that represents the buffer, and a VmaAllocation object that represents the memory allocation.
 * The address is used to access the buffer in the shader.
-*/
struct Buffer final
{
	VkBuffer        buffer{};      // Vulkan Buffer
	VmaAllocation   allocation{};  // Memory associated with the buffer
	VkDeviceAddress address{};     // Address of the buffer in the shader
};

/*--
 * An image is a region of memory used to store image data.
 * It is used to store texture data, framebuffer data, and other types of data.
-*/
struct Image final
{
	VkImage       image{};       // Vulkan Image
	VmaAllocation allocation{};  // Memory associated with the image
};

/*--
 * The image resource is an image with an image view and a layout.
 * and other information like format and extent.
-*/
struct ImageResource final
{
	VkImage       image{};       // Vulkan Image
	VmaAllocation allocation{};  // Memory associated with the image

	VkExtent2D    extent{};      // Size of the image
	VkFormat      format{};      // Format of the image (e.g. VK_FORMAT_R8G8B8A8_UNORM)
	VkImageLayout layout{};      // Layout of the image (color attachment, shader read, ...)
};

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