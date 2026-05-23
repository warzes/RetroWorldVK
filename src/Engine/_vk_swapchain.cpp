#include "stdafx.h"
#include "_vk_swapchain.h"
//=============================================================================
void Swapchain::Init(VkPhysicalDevice physicalDevice, VkDevice device, const QueueInfo& queue, VkSurfaceKHR surface, VkCommandPool cmdPool)
{
	m_physicalDevice = physicalDevice;
	m_device = device;
	m_queue = queue;
	m_surface = surface;
	m_cmdPool = cmdPool;
}
//=============================================================================
void Swapchain::Close()
{
	CloseResources();
	*this = {};
}
//=============================================================================
VkExtent2D Swapchain::InitResources(bool vSync)
{
	VkExtent2D outWindowSize;

	// Query the physical device's capabilities for the given surface.
	const VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
	.surface = m_surface };
	VkSurfaceCapabilities2KHR             capabilities2{ .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };
	/*VK_CHECK*/(vkGetPhysicalDeviceSurfaceCapabilities2KHR(m_physicalDevice, &surfaceInfo2, &capabilities2));

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo2, &formatCount, nullptr);
	std::vector<VkSurfaceFormat2KHR> formats(formatCount, { .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR });
	vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo2, &formatCount, formats.data());

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());

	// Choose the best available surface format and present mode
	const VkSurfaceFormat2KHR surfaceFormat2 = selectSwapSurfaceFormat(formats);
	const VkPresentModeKHR    presentMode = selectSwapPresentMode(presentModes, vSync);
	// Set the window size according to the surface's current extent
	outWindowSize = capabilities2.surfaceCapabilities.currentExtent;

	// Pick a swapchain image count: prefer triple-buffering, but honour the
	// surface's [minImageCount, maxImageCount] bounds.
	uint32_t minImageCount = capabilities2.surfaceCapabilities.minImageCount;  // Vulkan-defined minimum
	uint32_t preferredImageCount = std::max(kPreferredImageCount, minImageCount);

	// Handle the maxImageCount case where 0 means "no upper limit"
	uint32_t maxImageCount = (capabilities2.surfaceCapabilities.maxImageCount == 0) ? preferredImageCount :  // No upper limit, use preferred
		capabilities2.surfaceCapabilities.maxImageCount;

	// Clamp preferredImageCount to valid range [minImageCount, maxImageCount]
	m_imageCount = std::clamp(preferredImageCount, minImageCount, maxImageCount);

	// Store the chosen image format
	m_imageFormat = surfaceFormat2.surfaceFormat.format;

	// Create the swapchain itself
	const VkSwapchainCreateInfoKHR swapchainCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_surface,
		.minImageCount = m_imageCount,
		.imageFormat = surfaceFormat2.surfaceFormat.format,
		.imageColorSpace = surfaceFormat2.surfaceFormat.colorSpace,
		.imageExtent = capabilities2.surfaceCapabilities.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = capabilities2.surfaceCapabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = presentMode,
		.clipped = VK_TRUE,
	};
	/*VK_CHECK*/(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapChain));
	//DBG_VK_NAME(m_swapChain);

	// Retrieve the swapchain images
	{
		uint32_t imageCount = 0;
		vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
		assert(m_imageCount <= imageCount && "Wrong swapchain setup");
		m_imageCount = imageCount;  // Use the number of images the swapchain actually created
	}
	std::vector<VkImage> swapImages(m_imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_imageCount, swapImages.data());

	// Frames-in-flight: how many CPU frame slots run concurrently with the GPU.
	// Default of 2 is the canonical modern choice (one being recorded on the
	// CPU while the previous one executes on the GPU). Capped at imageCount
	// because we can never have more frames in flight than swapchain images.
	m_framesInFlight = std::min(kPreferredFramesInFlight, m_imageCount);

	// Per-image storage: VkImage, VkImageView, and the presentSemaphore
	// (binary semaphore that present waits on; must follow the image).
	m_nextImages.resize(m_imageCount);
	VkImageViewCreateInfo imageViewCreateInfo{
	.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	.viewType = VK_IMAGE_VIEW_TYPE_2D,
	.format = m_imageFormat,
	.components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY, .g = VK_COMPONENT_SWIZZLE_IDENTITY, .b = VK_COMPONENT_SWIZZLE_IDENTITY, .a = VK_COMPONENT_SWIZZLE_IDENTITY},
	.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
	};
	const VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	for (uint32_t i = 0; i < m_imageCount; i++)
	{
		m_nextImages[i].image = swapImages[i];
		//DBG_VK_NAME(m_nextImages[i].image);
		imageViewCreateInfo.image = m_nextImages[i].image;
		/*VK_CHECK*/(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_nextImages[i].imageView));
		//DBG_VK_NAME(m_nextImages[i].imageView);
		/*VK_CHECK*/(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_nextImages[i].presentSemaphore));
		//DBG_VK_NAME(m_nextImages[i].presentSemaphore);
	}

	// Per-in-flight-slot storage: acquireSemaphore (consumed by acquire).
	m_inFlightSlots.resize(m_framesInFlight);
	for (size_t i = 0; i < m_framesInFlight; ++i)
	{
		/*VK_CHECK*/(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_inFlightSlots[i].acquireSemaphore));
		//DBG_VK_NAME(m_inFlightSlots[i].acquireSemaphore);
	}

	// Transition images to present layout
	{
		VkCommandBuffer cmd = BeginSingleTimeCommands(m_device, m_cmdPool);
		for (uint32_t i = 0; i < m_imageCount; i++)
		{
			cmdTransitionSwapchainLayout(cmd, m_nextImages[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}
		EndSingleTimeCommands(cmd, m_device, m_cmdPool, m_queue.queue);
	}

	return outWindowSize;
}
//=============================================================================
VkExtent2D Swapchain::ReInitResources(bool vSync)
{
	// Wait for all frames to finish rendering before recreating the swapchain
	vkQueueWaitIdle(m_queue.queue);

	m_frameResourceIndex = 0;
	m_needRebuild = false;
	CloseResources();
	return InitResources(vSync);
}
//=============================================================================
void Swapchain::CloseResources()
{
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	for (auto& slot : m_inFlightSlots)
	{
		vkDestroySemaphore(m_device, slot.acquireSemaphore, nullptr);
	}
	for (auto& image : m_nextImages)
	{
		vkDestroyImageView(m_device, image.imageView, nullptr);
		vkDestroySemaphore(m_device, image.presentSemaphore, nullptr);
	}
}
//=============================================================================
VkResult Swapchain::AcquireNextImage(VkDevice device)
{
	assert(m_needRebuild == false && "Swapbuffer need to call reinitResources()");

	// Acquire the next image from the swapchain. The acquireSemaphore is per
	// in-flight slot (the slot's prior frame has finished, so the semaphore is
	// unsignaled and ready to be re-signaled by the swapchain).
	const VkSemaphore signalSem = m_inFlightSlots[m_frameResourceIndex].acquireSemaphore;
	const VkResult result = vkAcquireNextImageKHR(device, m_swapChain, std::numeric_limits<uint64_t>::max(), signalSem,
		VK_NULL_HANDLE, &m_frameImageIndex);
#ifdef NVVK_SEMAPHORE_DEBUG
	LOGI("AcquireNextImage: \t frameRes=%u imageIndex=%u", m_frameResourceIndex, m_frameImageIndex);
#endif
	// Handle special case if the swapchain is out of date (e.g., window resize)
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_needRebuild = true;  // Swapchain must be rebuilt on the next frame
	}
	else
	{
		assert((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) && "Couldn't aquire swapchain image");
	}
	return result;
}
void Swapchain::PresentFrame(VkQueue queue)
{
	// Present must wait on the presentSemaphore that follows the image (not the
	// in-flight slot), because vkAcquireNextImageKHR can return images out of
	// order.
	const VkSemaphore waitSem = m_nextImages[m_frameImageIndex].presentSemaphore;

	const VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,                   // Wait for rendering to finish
		.pWaitSemaphores = &waitSem,            // Per-image semaphore
		.swapchainCount = 1,                   // Swapchain to present the image
		.pSwapchains = &m_swapChain,        // Pointer to the swapchain
		.pImageIndices = &m_frameImageIndex,  // Index of the image to present
	};

	// Present the image and handle potential resizing issues
	const VkResult result = vkQueuePresentKHR(queue, &presentInfo);
#ifdef NVVK_SEMAPHORE_DEBUG
	LOGI("PresentFrame: \t\t slot=%u imageIndex=%u", m_frameResourceIndex, m_frameImageIndex);
#endif
	// If the swapchain is out of date (e.g., window resized), it needs to be rebuilt
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_needRebuild = true;
	}
	else
	{
		assert((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) && "Couldn't present swapchain image");
	}

	// Advance to the next CPU in-flight slot (NOT the next image -- images are
	// chosen by the presentation engine).
	m_frameResourceIndex = (m_frameResourceIndex + 1) % m_framesInFlight;
}
//=============================================================================
VkSurfaceFormat2KHR Swapchain::selectSwapSurfaceFormat(const std::vector<VkSurfaceFormat2KHR>& availableFormats) const
{
	// If there's only one available format and it's undefined, return a default format.
	if (availableFormats.size() == 1 && availableFormats[0].surfaceFormat.format == VK_FORMAT_UNDEFINED)
	{
		VkSurfaceFormat2KHR result{ .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR,
		.surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} };
		return result;
	}

	const auto preferredFormats = std::to_array<VkSurfaceFormat2KHR>({
	{.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, .surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
	{.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, .surfaceFormat = {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
		});

	// Check available formats against the preferred formats.
	for (const auto& preferredFormat : preferredFormats)
	{
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.surfaceFormat.format == preferredFormat.surfaceFormat.format
				&& availableFormat.surfaceFormat.colorSpace == preferredFormat.surfaceFormat.colorSpace)
			{
				return availableFormat;  // Return the first matching preferred format.
			}
		}
	}

	// If none of the preferred formats are available, return the first available format.
	return availableFormats[0];
}
//=============================================================================
VkPresentModeKHR Swapchain::selectSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vSync)
{
	if (vSync)
	{
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	bool mailboxSupported = false, immediateSupported = false;

	for (VkPresentModeKHR mode : availablePresentModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			mailboxSupported = true;
		if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			immediateSupported = true;
	}

	if (mailboxSupported)
	{
		return VK_PRESENT_MODE_MAILBOX_KHR;
	}

	if (immediateSupported)
	{
		return VK_PRESENT_MODE_IMMEDIATE_KHR;  // Best mode for low latency
	}

	return VK_PRESENT_MODE_FIFO_KHR;  // Fallback to FIFO if neither MAILBOX nor IMMEDIATE is available
}
//=============================================================================