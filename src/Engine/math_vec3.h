#pragma once

namespace math
{
	struct vec3 final
	{
		vec3() noexcept = default;
		vec3(float n) noexcept : x(n), y(n), z(n) {}
		vec3(float x, float y, float z) noexcept : x(x), y(y), z(z) {}
		vec3(const vec3&) noexcept = default;

		float& operator[](size_t index) noexcept { return *(&x + index); }
		const float& operator[](size_t index) const noexcept { return *(&x + index); }

		vec3 operator+(const vec3& rhs) const noexcept { return { x + rhs.x, y + rhs.y, z + rhs.z }; }
		vec3 operator-(const vec3& rhs) const noexcept { return { x - rhs.x, y - rhs.y, z - rhs.z }; }
		vec3 operator*(float scale) const noexcept { return { x * scale, y * scale, z * scale }; }
		vec3 operator+(float a) const noexcept { return { x + a, y + a, z + a }; }
		vec3 operator-(float a) const noexcept { return { x - a, y - a, z - a }; }
		vec3 operator-() const noexcept { return { -x, -y, -z }; }

		vec3 operator+=(const vec3& v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
		vec3 operator-=(const vec3& v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
		vec3 operator*=(float scale) noexcept { x *= scale; y *= scale; z *= scale; return *this; }

		vec3 Normalized() const
		{
			return (*this) * (1.f / sqrtf(x * x + y * y + z * z));
		}

		float x, y, z;
	};

	inline float Dot(const vec3& lhs, const vec3& rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
	}

	inline vec3 Cross(const vec3& lhs, const vec3& rhs)
	{
		return vec3(
			lhs.y * rhs.z - lhs.z * rhs.y,
			lhs.z * rhs.x - lhs.x * rhs.z,
			lhs.x * rhs.y - lhs.y * rhs.x);
	}

} // namespace math