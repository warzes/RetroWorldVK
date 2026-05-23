#pragma once

#include "_vk_core.h"

/*--
* Swapchain: presents rendered images to the screen.
*
* Two distinct counts must NOT be conflated:
*
*   imageCount      -- how many images the swapchain owns (typically 2-4).
*                      Triple-buffering (3) is the common modern default: it
*                      lets the GPU work on image N+1 while N is being scanned
*                      out by the display, with a third in the present queue.
*                      vSync (FIFO) and tear-free (MAILBOX) both benefit.
*
*   framesInFlight  -- how many CPU frame slots are recorded ahead of the GPU
*                      (typically 2). This sizes the per-frame command buffers,
*                      fences, and "image available" semaphores. Going higher
*                      adds memory and input lag with no throughput gain.
*
* Resource ownership maps onto these two counts:
*
*   Per swapchain image  --> presentSemaphore. The semaphore that
*     vkQueuePresentKHR waits on must follow the image, because
*     vkAcquireNextImageKHR can return images out of order (especially with
*     MAILBOX): a per-slot binary semaphore would race itself.
*
*   Per in-flight slot   --> acquireSemaphore. Consumed by acquire and
*     reused once the slot's previous frame completes on the GPU. Also: the
*     command buffer, command pool, and timeline-value tracker that live in
*     MinimalLatest::m_frameData are sized by framesInFlight, not imageCount.
*
* Two indices, both real, both needed:
*   m_frameResourceIndex -- cycles [0, m_framesInFlight); selects the in-flight slot.
*   m_frameImageIndex    -- whatever vkAcquireNextImageKHR gave us; selects an image.
*
* Naming: these two binary semaphores are named by the Vulkan call they
* pair with -- acquireSemaphore is signaled by vkAcquireNextImageKHR, and
* presentSemaphore is waited on by vkQueuePresentKHR. This makes every
* call site self-documenting ("wait on acquire, signal for present") and
* mirrors the two API calls directly.
*
* Older samples and the Khronos tutorial often name them "imageAvailable"
* and "renderFinished" instead -- describing the state each semaphore
* represents rather than the call it pairs with. Both schemes are valid;
* if you're porting from that convention, the mapping is one-to-one:
*   imageAvailable  <-> acquireSemaphore   (signaled by acquire)
*   renderFinished  <-> presentSemaphore   (waited on by present)
-*/
class Swapchain final
{
public:
	Swapchain() = default;
	~Swapchain() { assert(m_swapChain == VK_NULL_HANDLE && "Missing deinit()"); }

	// Initialize the swapchain with the provided context and surface, then we can create and re-create it
	void Init(VkPhysicalDevice physicalDevice, VkDevice device, const QueueInfo& queue, VkSurfaceKHR surface, VkCommandPool cmdPool);
	// Destroy internal resources and reset its initial state
	void Close();

	void        RequestRebuild() { m_needRebuild = true; }
	bool        NeedRebuilding() const { return m_needRebuild; }
	VkImage     GetImage() const { return m_nextImages[m_frameImageIndex].image; }
	VkImageView GetImageView() const { return m_nextImages[m_frameImageIndex].imageView; }
	VkFormat    GetImageFormat() const { return m_imageFormat; }

	// Number of swapchain images (presentation parallelism).
	uint32_t GetImageCount() const { return m_imageCount; }

	// Number of CPU frame slots (= concurrent frames in flight).
	uint32_t GetFramesInFlight() const { return m_framesInFlight; }

	// The current in-flight slot; cycles [0, framesInFlight). Use this to index any per-frame CPU resource (command buffers, timeline values, etc.).
	uint32_t GetFrameResourceIndex() const { return m_frameResourceIndex; }

	// acquireSemaphore is per in-flight slot (consumed by acquire).
	VkSemaphore GetAcquireSemaphore() const { return m_inFlightSlots[m_frameResourceIndex].acquireSemaphore; }

	// presentSemaphore semaphore is per *image* (consumed by present, must follow the image because acquire can return images out of order).
	VkSemaphore GetPresentSemaphore() const { return m_nextImages[m_frameImageIndex].presentSemaphore; }

	/*--
	* Create the swapchain using the provided context, surface, and vSync option. The actual window size is returned.
	* Queries the GPU capabilities, selects the best surface format and present mode, and creates the swapchain accordingly.
	-*/
	VkExtent2D InitResources(bool vSync = true);

	/*--
	* Recreate the swapchain, typically after a window resize or when it becomes invalid.
	* This waits for all rendering to be finished before destroying the old swapchain and creating a new one.
	-*/
	VkExtent2D ReInitResources(bool vSync = true);

	/*--
	* Destroy the swapchain and its associated resources.
	* This function is also called when the swapchain needs to be recreated.
	-*/
	void CloseResources();

	/*--
	* Prepares the command buffer for recording rendering commands.
	* This function handles synchronization with the previous frame and acquires the next image from the swapchain.
	* The command buffer is reset, ready for new rendering commands.
	-*/
	VkResult AcquireNextImage(VkDevice device);

	/*--
	* Presents the rendered image to the screen.
	* The semaphore ensures that the image is presented only after rendering is complete.
	* Advances to the next frame in the cycle.
	-*/
	void PresentFrame(VkQueue queue);

private:
	/*-- Per-swapchain-image resources -------------------------------------------
	* One entry per image returned by vkGetSwapchainImagesKHR. The
	* presentSemaphore lives here (not on the in-flight slot) because
	* vkQueuePresentKHR consumes it for a specific image, and the presentation
	* engine may hand images back out of order.
	-*/
	struct SwapchainImage final
	{
		VkImage     image{};             // Swapchain image (owned by the swapchain)
		VkImageView imageView{};         // 2D view of the image
		VkSemaphore presentSemaphore{};  // Binary semaphore: signaled when rendering done, waited on by present
	};

	/*-- Per-in-flight-slot resources --------------------------------------------
	* One entry per "frame in flight" -- typically 2, regardless of the image
	* count. Holds resources tied to the CPU's submission cadence rather than
	* the displayed image. The acquireSemaphore is recycled here.
	-*/
	struct InFlightSlot final
	{
		VkSemaphore acquireSemaphore{};  // Binary semaphore signaled by vkAcquireNextImageKHR
	};

	// We choose the format that is the most common, and that is supported by* the physical device.
	VkSurfaceFormat2KHR selectSwapSurfaceFormat(const std::vector<VkSurfaceFormat2KHR>& availableFormats) const;

	/*--
	* The present mode is chosen based on the vSync option
	* The FIFO mode is the most common, and is used when vSync is enabled.
	* The MAILBOX mode is used when vSync is disabled, and is the best mode for triple buffering.
	* The IMMEDIATE mode is used when vSync is disabled, and is the best mode for low latency.
	-*/
	VkPresentModeKHR selectSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vSync = true);

private:
	VkPhysicalDevice m_physicalDevice{};  // The physical device (GPU)
	VkDevice         m_device{};          // The logical device (interface to the physical device)
	QueueInfo        m_queue{};           // The queue used to submit command buffers to the GPU
	VkSwapchainKHR   m_swapChain{};       // The swapchain
	VkFormat         m_imageFormat{};     // The format of the swapchain images
	VkSurfaceKHR     m_surface{};         // The surface to present images to
	VkCommandPool    m_cmdPool{};         // The command pool for the swapchain

	std::vector<SwapchainImage> m_nextImages;              // Sized by m_imageCount
	std::vector<InFlightSlot>   m_inFlightSlots;           // Sized by m_framesInFlight
	uint32_t                    m_frameResourceIndex = 0;  // Cycles [0, m_framesInFlight)
	uint32_t                    m_frameImageIndex = 0;  // Whatever the swapchain returns
	bool                        m_needRebuild = false;

	// Default targets, clamped at runtime in initResources() to device limits.
	static constexpr uint32_t kPreferredImageCount = 3;                         // Triple buffering for presentation
	static constexpr uint32_t kPreferredFramesInFlight = 2;                      // CPU-side double buffering
	uint32_t                  m_imageCount = kPreferredImageCount;      // From vkGetSwapchainImagesKHR
	uint32_t                  m_framesInFlight = kPreferredFramesInFlight;  // <= m_imageCount
};