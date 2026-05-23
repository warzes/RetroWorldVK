#include "stdafx.h"
#include "_vk_resurces.h"
#include "_vk_core.h"
#include "core_log.h"
//=============================================================================
bool ResourceAllocator::Init(VmaAllocatorCreateInfo allocatorInfo)
{	
	allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; // allow querying for the GPU address of a buffer
	allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
	allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT; // allow using VkBufferUsageFlags2CreateInfo

	m_device = allocatorInfo.device;

	// Because we use VMA_DYNAMIC_VULKAN_FUNCTIONS
	const VmaVulkanFunctions functions = {
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
	};
	allocatorInfo.pVulkanFunctions = &functions;

	VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vmaCreateAllocator failed: " + VkResultStr(result));
		return false;
	}

	return true;
}
//=============================================================================
void ResourceAllocator::Close()
{
	if (!m_stagingBuffers.empty())
		core::Warning("Staging buffers were not freed before destroying the allocator");
	FreeStagingBuffers();
	vmaDestroyAllocator(m_allocator);
	*this = {};
}
//=============================================================================
Buffer ResourceAllocator::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags2 usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags, VkDeviceSize minAlignment)
{
	const bool wantsAddress = (usage & VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT) != 0;

	// VkBufferUsageFlags2CreateInfo (Maintenance5) replaces the legacy 32-bit usage field with a 64-bit one. The CreateInfo's .usage stays 0; the real usage flags ride in the chained struct.
	const VkBufferUsageFlags2CreateInfo bufferUsageFlags2CreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
		.usage = usage,
	};

	const VkBufferCreateInfo bufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = &bufferUsageFlags2CreateInfo,
		.size = size,
		.usage = 0,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,  // Only one queue family will access it
	};

	// Note: we deliberately do NOT auto-promote large allocations to dedicated memory. VMA's own heuristics (and the VMA_MEMORY_USAGE_AUTO* policies) are far better tuned than a one-size-fits-all threshold. If you need a dedicated allocation for a specific buffer, pass VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT in `flags`.
	VmaAllocationCreateInfo allocInfo = { .flags = flags, .usage = memoryUsage };

	// Create the buffer
	Buffer            resultBuffer;
	VmaAllocationInfo allocInfoOut{};
	VkResult result = vmaCreateBufferWithAlignment(m_allocator, &bufferInfo, &allocInfo, minAlignment, &resultBuffer.buffer, &resultBuffer.allocation, &allocInfoOut);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vmaCreateBufferWithAlignment failed: " + VkResultStr(result));
		// TODO: 
	}

	// Query the GPU address only if the caller asked for one (BDA opt-in).
	if (wantsAddress)
	{
		const VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = resultBuffer.buffer };
		resultBuffer.address = vkGetBufferDeviceAddress(m_device, &info);
	}

	return resultBuffer;
}
//=============================================================================
void ResourceAllocator::DestroyBuffer(Buffer buffer)
{
	vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
}
//=============================================================================
Image ResourceAllocator::CreateImage(const VkImageCreateInfo& imageInfo)
{
	const VmaAllocationCreateInfo createInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };

	Image             image;
	VmaAllocationInfo allocInfo{};
	VkResult result = vmaCreateImage(m_allocator, &imageInfo, &createInfo, &image.image, &image.allocation, &allocInfo);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vmaCreateImage failed: " + VkResultStr(result));
		// TODO: 
	}
	return image;
}
//=============================================================================
void ResourceAllocator::DestroyImage(Image& image)
{
	vmaDestroyImage(m_allocator, image.image, image.allocation);
}
//=============================================================================
void ResourceAllocator::DestroyImageResource(ImageResource& imageResource)
{
	DestroyImage(imageResource);
}
//=============================================================================
void ResourceAllocator::FreeStagingBuffers()
{
	for (const auto& buffer : m_stagingBuffers)
		DestroyBuffer(buffer);
	m_stagingBuffers.clear();
}
//=============================================================================