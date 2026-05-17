#pragma once

#if defined(_WIN32)
#	define NOMINMAX
#	define WIN32_LEAN_AND_MEAN
#endif

#if defined(_WIN32)
#	define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1