#pragma once

#include "math_vec3.h"

namespace math
{
	struct vec4 final
	{
		vec4() noexcept = default;
		vec4(float n) noexcept : x(n), y(n), z(n), w(n) {}
		vec4(float x, float y, float z, float w) noexcept : x(x), y(y), z(z), w(w) {}
		vec4(const vec3& v, float w) noexcept : x(v.x), y(v.y), z(v.z), w(w) {}

		float& operator[](size_t index) noexcept { return *(&x + index); }
		const float& operator[](size_t index) const noexcept { return *(&x + index); }

		float x, y, z, w;
	};

} // namespace math