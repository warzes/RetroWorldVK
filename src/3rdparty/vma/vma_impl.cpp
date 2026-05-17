#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
//#define VMA_MIN_ALIGNMENT 32
#define VMA_LEAK_LOG_FORMAT(format, ...) \
	{                                    \
		printf((format), __VA_ARGS__);   \
		printf("\n");                    \
	}
#include "volk/volk.h"
#include "vk_mem_alloc.h"