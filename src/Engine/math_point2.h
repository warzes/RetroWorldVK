#pragma once

namespace math
{
	struct point2 final
	{
		point2() noexcept = default;
		point2(int x, int y) noexcept : x(x), y(y) {}

		int& operator[](uint32_t index) noexcept { return *(&x + index); }
		const int& operator[](uint32_t index) const noexcept { return *(&x + index); }

		point2 operator+(const point2& rhs) const noexcept { return point2(x + rhs.x, y + rhs.y); }
		point2 operator-(const point2& rhs) const noexcept { return point2(x - rhs.x, y - rhs.y); }
		point2 operator*(float s) const noexcept { return point2(x * s, y * s); }

		int x, y;
	};

} // namespace math