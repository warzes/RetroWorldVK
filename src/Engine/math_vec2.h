#pragma once

namespace math
{
	struct vec2 final
	{
		vec2() noexcept = default;
		vec2(float x, float y) noexcept : x(x), y(y) {}

		float& operator[](uint32_t index) noexcept { return *(&x + index); }
		const float& operator[](uint32_t index) const noexcept { return *(&x + index); }

		vec2 operator+(const vec2& rhs) const noexcept { return vec2(x + rhs.x, y + rhs.y); }
		vec2 operator-(const vec2& rhs) const noexcept { return vec2(x - rhs.x, y - rhs.y); }
		vec2 operator*(float s) const noexcept { return vec2(x * s, y * s); }

		float x, y;
	};

} // namespace math