#pragma once

#include "core_utils.h"

/*--
* SamplerPool: deduplicated sampler management.
*
* Vulkan limits the number of live samplers per device, so reusing the same
* VkSampler for identical VkSamplerCreateInfo is essential at scale. This
* class exposes TWO independent paths, each with its own backing map:
*
*  Legacy / VkSampler path:
*    acquireSampler()        -> VkSampler (deduplicated by VkSamplerCreateInfo)
*    releaseSampler()
*  Used by callers that need an actual sampler handle, e.g. ImGui textures
*  and the offscreen RenderTarget display sampler.
*
*  Descriptor-heap path:
*    acquireSamplerDescriptor()  -> uint32_t heap slot index (ref-counted)
*    releaseSamplerDescriptor()
*  Used when writing into the VK_EXT_descriptor_heap sampler heap. The
*  driver creates the underlying sampler internally inside
*  vkWriteSamplerDescriptorsEXT; this class only allocates a stable slot
*  index that the shader uses to address it.
*
* The two paths are intentionally separate (different storage, different
* lifetimes) because their consumers are different: ImGui doesn't know about
* the heap, and the heap doesn't expose VkSampler handles.
-*/

class SamplerPool final
{
public:
	SamplerPool() = default;
	~SamplerPool() { assert(m_device == VK_NULL_HANDLE && "Missing deinit()"); }
	// Initialize the sampler pool with the device reference, then we can later acquire samplers
	void Init(VkDevice device);
	// Destroy internal resources and reset its initial state
	void Close();
	// Get or create VkSampler based on VkSamplerCreateInfo
	VkSampler AcquireSampler(const VkSamplerCreateInfo& createInfo);
	void ReleaseSampler(VkSampler sampler);

	// Descriptor heap variant: returns a deduplicated heap index for a given VkSamplerCreateInfo.
	// Identical create-infos share the same index (ref-counted).
	uint32_t AcquireSamplerDescriptor(const VkSamplerCreateInfo& createInfo);
	// Release a previously acquired descriptor heap index.
	// When the last reference is released, the index is recycled for future use.
	void ReleaseSamplerDescriptor(uint32_t index);

private:
	VkDevice m_device{};

	struct SamplerCreateInfoHash final
	{
		std::size_t operator()(const VkSamplerCreateInfo& info) const
		{
			std::size_t seed{ 0 };
			seed = core::HashCombine(seed, info.magFilter);
			seed = core::HashCombine(seed, info.minFilter);
			seed = core::HashCombine(seed, info.mipmapMode);
			seed = core::HashCombine(seed, info.addressModeU);
			seed = core::HashCombine(seed, info.addressModeV);
			seed = core::HashCombine(seed, info.addressModeW);
			seed = core::HashCombine(seed, info.mipLodBias);
			seed = core::HashCombine(seed, info.anisotropyEnable);
			seed = core::HashCombine(seed, info.maxAnisotropy);
			seed = core::HashCombine(seed, info.compareEnable);
			seed = core::HashCombine(seed, info.compareOp);
			seed = core::HashCombine(seed, info.minLod);
			seed = core::HashCombine(seed, info.maxLod);
			seed = core::HashCombine(seed, info.borderColor);
			seed = core::HashCombine(seed, info.unnormalizedCoordinates);

			return seed;
		}
	};

	struct SamplerCreateInfoEqual final
	{
		bool operator()(const VkSamplerCreateInfo& lhs, const VkSamplerCreateInfo& rhs) const
		{
			return std::memcmp(&lhs, &rhs, sizeof(VkSamplerCreateInfo)) == 0;
		}
	};

	// Stores unique samplers with their corresponding VkSamplerCreateInfo
	std::unordered_map<VkSamplerCreateInfo, VkSampler, SamplerCreateInfoHash, SamplerCreateInfoEqual> m_samplerMap;

	// --- Descriptor heap index management ---
	struct DescriptorEntry final
	{
		uint32_t index{};
		uint32_t refCount{};
	};
	std::unordered_map<VkSamplerCreateInfo, DescriptorEntry, SamplerCreateInfoHash, SamplerCreateInfoEqual> m_descriptorMap;
	std::unordered_map<uint32_t, VkSamplerCreateInfo> m_descriptorReverseMap;
	std::vector<uint32_t>                             m_freeDescriptorIndices;
	uint32_t                                          m_nextDescriptorIndex = 0;

	// Internal function to create a new VkSampler
	const VkSampler createSampler(const VkSamplerCreateInfo& createInfo) const;
};