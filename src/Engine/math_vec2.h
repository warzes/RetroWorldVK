#pragma once

namespace math
{
	struct vec2 final
	{
		vec2() noexcept = default;
		vec2(float n) noexcept : x(n), y(n) {}
		vec2(float x, float y) noexcept : x(x), y(y) {}
		vec2(const vec2&) noexcept = default;

		float& operator[](size_t index) noexcept { return *(&x + index); }
		const float& operator[](size_t index) const noexcept { return *(&x + index); }

		vec2 operator+(const vec2& rhs) const noexcept { return { x + rhs.x, y + rhs.y }; }
		vec2 operator-(const vec2& rhs) const noexcept { return { x - rhs.x, y - rhs.y }; }
		vec2 operator*(float scale) const noexcept { return { x * scale, y * scale }; }
		vec2 operator+(float a) const noexcept { return { x + a, y + a }; }
		vec2 operator-(float a) const noexcept { return { x - a, y - a }; }
		vec2 operator-() const noexcept { return { -x, -y }; }

		vec2 operator+=(const vec2& v) noexcept { x += v.x; y += v.y; return *this; }
		vec2 operator-=(const vec2& v) noexcept { x -= v.x; y -= v.y; return *this; }
		vec2 operator*=(float scale) noexcept { x *= scale; y *= scale; return *this; }

		float x, y;
	};

} // namespace math