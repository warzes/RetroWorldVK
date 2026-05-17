#pragma once

namespace math
{
	struct point2 final
	{
		point2() noexcept = default;
		point2(int n) noexcept : x(n), y(n) {}
		point2(int x, int y) noexcept : x(x), y(y) {}
		point2(const point2&) noexcept = default;

		int& operator[](size_t index) noexcept { return *(&x + index); }
		const int& operator[](size_t index) const noexcept { return *(&x + index); }

		point2 operator+(const point2& rhs) const noexcept { return { x + rhs.x, y + rhs.y }; }
		point2 operator-(const point2& rhs) const noexcept { return { x - rhs.x, y - rhs.y }; }
		point2 operator-() const noexcept { return { -x, -y }; }

		int x, y;
	};

} // namespace math