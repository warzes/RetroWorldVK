#pragma once

#include "_vk_core.h"

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
struct Image
{
	VkImage       image{};       // Vulkan Image
	VmaAllocation allocation{};  // Memory associated with the image
};

/*--
* The image resource is an image with an image view and a layout.
* and other information like format and extent.
-*/
struct ImageResource final : Image
{
	VkExtent2D    extent{};  // Size of the image
	VkFormat      format{};  // Format of the image (e.g. VK_FORMAT_R8G8B8A8_UNORM)
	VkImageLayout layout{};  // Layout of the image (color attachment, shader read, ...)
};

/*--
* Vulkan Memory Allocator (VMA) is a library that helps to manage memory in Vulkan.
* This should be used to manage the memory of the resources instead of using the Vulkan API directly.
-*/

class ResourceAllocator final
{
public:
	ResourceAllocator() = default;
	~ResourceAllocator() { assert(m_allocator == nullptr && "Missing deinit()"); }
	operator VmaAllocator() const { return m_allocator; }

	bool Init(VmaAllocatorCreateInfo allocatorInfo);
	void Close();

	/*--
	* Create a buffer.
	*
	* Buffer device address (BDA) is *opt-in*: pass
	* VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT in `usage` if you need
	* Buffer.address to be populated. Otherwise the address stays 0 and no
	* vkGetBufferDeviceAddress call is made -- which is the correct mental model
	* (BDA enables some implementation overhead and isn't always wanted, e.g.
	* staging buffers).
	*
	* Modern VMA uses VMA_MEMORY_USAGE_AUTO* + HOST_ACCESS flags; the legacy
	* VMA_MEMORY_USAGE_GPU_ONLY / CPU_TO_GPU / GPU_TO_CPU enums are deprecated.
	*
	* Examples:
	*   UBO, CPU writes / GPU reads:
	*       VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
	*       + VMA_MEMORY_USAGE_AUTO
	*       + VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
	*
	*   SSBO, GPU-only (via staging):
	*       VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT
	*       [+ VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT for BDA]
	*       + VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
	*
	*   SSBO, per-frame CPU writes:
	*       VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT
	*       + VMA_MEMORY_USAGE_AUTO
	*       + VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
	*
	*   Readback:
	*       VK_BUFFER_USAGE_2_TRANSFER_DST_BIT
	*       + VMA_MEMORY_USAGE_AUTO
	*       + VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
	*
	* Allocation flags (combine as needed):
	*   VMA_ALLOCATION_CREATE_MAPPED_BIT                         -- persistent mapping
	*   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT   -- CPU streaming writes
	*   VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT             -- CPU readback
	*
	* Note: the best-practices validation layer may warn about small dedicated
	* allocations; that is expected for a sample of this scale and not worth
	* working around. Production engines should configure a VmaPool with a
	* larger blockSize to sub-allocate small resources together.
	-*/
	Buffer CreateBuffer(
		VkDeviceSize             size,
		VkBufferUsageFlags2      usage,
		VmaMemoryUsage           memoryUsage = VMA_MEMORY_USAGE_AUTO,
		VmaAllocationCreateFlags flags = {},
		VkDeviceSize             minAlignment = {});

	void DestroyBuffer(Buffer buffer);

	/*--
	* Create a staging buffer, copy data into it, and track it.
	* This method accepts data, handles the mapping, copying, and unmapping
	* automatically.
	-*/
	template <typename T>
	Buffer CreateStagingBuffer(const std::span<T>& vectorData)
	{
		const VkDeviceSize bufferSize = sizeof(T) * vectorData.size();

		// Create a staging buffer (host-visible, CPU-writes-then-GPU-reads).
		Buffer stagingBuffer = CreateBuffer(bufferSize, VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

		// Track the staging buffer for later cleanup
		m_stagingBuffers.push_back(stagingBuffer);

		// Map and copy data to the staging buffer
		void* data = nullptr;
		vmaMapMemory(m_allocator, stagingBuffer.allocation, &data);
		memcpy(data, vectorData.data(), (size_t)bufferSize);
		vmaUnmapMemory(m_allocator, stagingBuffer.allocation);
		return stagingBuffer;
	}

	/*--
	* Create a buffer (GPU only) with data, this is done using a staging buffer
	* The staging buffer is a buffer that is used to transfer data from the CPU
	* to the GPU.
	* and cannot be freed until the data is transferred. So the command buffer
	* must be submitted, then
	* the staging buffer can be cleared using the freeStagingBuffers function.
	-*/
	template <typename T>
	Buffer CreateBufferAndUploadData(
		VkCommandBuffer          cmd,
		const std::span<T>&      vectorData,
		VkBufferUsageFlags2      usageFlags,
		VmaAllocationCreateFlags flags = {},
		VkDeviceSize             minAlignment = {})
	{
		// Create staging buffer and upload data
		Buffer stagingBuffer = CreateStagingBuffer(vectorData);

		// Create the final buffer in GPU memory
		const VkDeviceSize bufferSize = sizeof(T) * vectorData.size();
		Buffer             buffer = CreateBuffer(bufferSize, usageFlags | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, flags, minAlignment);

		const std::array<VkBufferCopy, 1> copyRegion{ {{.size = bufferSize}} };
		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer.buffer, uint32_t(copyRegion.size()), copyRegion.data());

		return buffer;
	}

	/*--
	* Create an image in GPU memory. This does not adding data to the image.
	* This is only creating the image in GPU memory.
	* See createImageAndUploadData for creating an image and uploading data.
	-*/
	Image CreateImage(const VkImageCreateInfo& imageInfo);
	void DestroyImage(Image& image);
	void DestroyImageResource(ImageResource& imageResource);

	/*-- Create an image and upload data using a staging buffer --*/
	template <typename T>
	ImageResource CreateImageAndUploadData(VkCommandBuffer cmd, const std::span<T>& vectorData, const VkImageCreateInfo& _imageInfo, VkImageLayout finalLayout)
	{
		// Create staging buffer and upload data
		Buffer stagingBuffer = CreateStagingBuffer(vectorData);

		// Create image in GPU memory
		VkImageCreateInfo imageInfo = _imageInfo;
		imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // We will copy data to this image
		Image image = CreateImage(imageInfo);

		// Transition image layout for copying data
		cmdInitImageLayout(cmd, image.image);

		// Copy buffer data to the image
		const std::array<VkBufferImageCopy, 1> copyRegion{
			{{.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1}, .imageExtent = imageInfo.extent}} };

		vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, image.image, VK_IMAGE_LAYOUT_GENERAL, uint32_t(copyRegion.size()), copyRegion.data());

		ImageResource resultImage(image);
		resultImage.layout = finalLayout;
		return resultImage;
	}

	/*--
	* The staging buffers are buffers that are used to transfer data from the CPU to the GPU.
	* They cannot be freed until the data is transferred. So the command buffer must be completed, then the staging buffer can be cleared.
	-*/
	void FreeStagingBuffers();

private:
	VmaAllocator        m_allocator{};
	VkDevice            m_device{};
	std::vector<Buffer> m_stagingBuffers;
};