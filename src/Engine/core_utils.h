#pragma once

namespace core
{
	// Combines hash values using the FNV-1a based algorithm
	inline std::size_t HashCombine(std::size_t seed, auto const& value)
	{
		return seed ^ (std::hash<std::decay_t<decltype(value)>>{}(value)+0x9e3779b9 + (seed << 6) + (seed >> 2));
	}
} // namespace core