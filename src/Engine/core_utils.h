#pragma once

namespace core
{
	/*--
	* Aligns a value up to the next multiple of the alignment.
	* If the alignment is 0, it returns the original value.
	*/
	inline VkDeviceSize AlignUp(VkDeviceSize value, VkDeviceSize alignment)
	{
		if (alignment == 0) return value;
		return ((value + alignment - 1) / alignment) * alignment;
	}

	// Combines hash values using the FNV-1a based algorithm
	inline std::size_t HashCombine(std::size_t seed, auto const& value)
	{
		return seed ^ (std::hash<std::decay_t<decltype(value)>>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
	}
} // namespace core