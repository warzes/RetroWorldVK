#pragma once

namespace math
{
	struct vec3 final
	{
		vec3() noexcept = default;
		vec3(float x, float y, float z) noexcept : x(x), y(y), z(z) {}

		float& operator[](uint32_t index) noexcept { return *(&x + index); }
		const float& operator[](uint32_t index) const noexcept { return *(&x + index); }

		vec3 operator+(const vec3& rhs) const noexcept { return vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
		vec3 operator-(const vec3& rhs) const noexcept { return vec3(x - rhs.x, y - rhs.y, z - rhs.z); }
		vec3 operator*(float s) const noexcept { return vec3(x * s, y * s, z * s); }

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