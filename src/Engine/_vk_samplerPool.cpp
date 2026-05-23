#include "stdafx.h"
#include "_vk_samplerPool.h"
#include "_vk_core.h"
#include "core_log.h"
//=============================================================================
void SamplerPool::Init(VkDevice device)
{
	m_device = device;
}
//=============================================================================
void SamplerPool::Close()
{
	for (const auto& entry : m_samplerMap)
	{
		vkDestroySampler(m_device, entry.second, nullptr);
	}
	m_samplerMap.clear();
	*this = {};
}
//=============================================================================
VkSampler SamplerPool::AcquireSampler(const VkSamplerCreateInfo& createInfo)
{
	if (auto it = m_samplerMap.find(createInfo); it != m_samplerMap.end())
	{
		// If found, return existing sampler
		return it->second;
	}

	// Otherwise, create a new sampler
	VkSampler newSampler = createSampler(createInfo);
	m_samplerMap[createInfo] = newSampler;
	return newSampler;
}
//=============================================================================
void SamplerPool::ReleaseSampler(VkSampler sampler)
{
	for (auto it = m_samplerMap.begin(); it != m_samplerMap.end();)
	{
		if (it->second == sampler)
		{
			vkDestroySampler(m_device, it->second, nullptr);
			it = m_samplerMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}
//=============================================================================
uint32_t SamplerPool::AcquireSamplerDescriptor(const VkSamplerCreateInfo& createInfo)
{
	if (auto it = m_descriptorMap.find(createInfo); it != m_descriptorMap.end())
	{
		++it->second.refCount;
		return it->second.index;
	}

	uint32_t newIndex{};
	if (!m_freeDescriptorIndices.empty())
	{
		newIndex = m_freeDescriptorIndices.back();
		m_freeDescriptorIndices.pop_back();
	}
	else
	{
		newIndex = m_nextDescriptorIndex++;
	}

	m_descriptorMap[createInfo] = { newIndex, 1 };
	m_descriptorReverseMap[newIndex] = createInfo;
	return newIndex;
}
//=============================================================================
void SamplerPool::ReleaseSamplerDescriptor(uint32_t index)
{
	auto revIt = m_descriptorReverseMap.find(index);
	assert(revIt != m_descriptorReverseMap.end() && "releaseSamplerDescriptor: unknown index");

	auto fwdIt = m_descriptorMap.find(revIt->second);
	assert(fwdIt != m_descriptorMap.end() && "releaseSamplerDescriptor: inconsistent state");

	if (--fwdIt->second.refCount == 0)
	{
		m_descriptorMap.erase(fwdIt);
		m_descriptorReverseMap.erase(revIt);
		m_freeDescriptorIndices.push_back(index);
	}
}
//=============================================================================
const VkSampler SamplerPool::createSampler(const VkSamplerCreateInfo& createInfo) const
{
	assert(m_device && "Initialization was missing");
	VkSampler sampler{};
	VkResult result = vkCreateSampler(m_device, &createInfo, nullptr, &sampler);
	if (result != VK_SUCCESS)
	{
		core::Fatal("vkCreateSampler failed: " + VkResultStr(result));
		// TODO: return null
	}
	return sampler;
}
//=============================================================================