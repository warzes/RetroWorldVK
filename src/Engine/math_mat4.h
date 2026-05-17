#pragma once

namespace math
{
	struct mat4 final
	{
		mat4() noexcept = default;

		mat4(
			float _11, float _12, float _13, float _14,
			float _21, float _22, float _23, float _24,
			float _31, float _32, float _33, float _34,
			float _41, float _42, float _43, float _44) noexcept :
			_11(_11), _12(_12), _13(_13), _14(_14),
			_21(_21), _22(_22), _23(_23), _24(_24),
			_31(_31), _32(_32), _33(_33), _34(_34),
			_41(_41), _42(_42), _43(_43), _44(_44)
		{}

		mat4(
			const vec4& row1,
			const vec4& row2,
			const vec4& row3,
			const vec4& row4) noexcept :
			_11(row1.x), _12(row1.y), _13(row1.z), _14(row1.w),
			_21(row2.x), _22(row2.y), _23(row2.z), _24(row2.w),
			_31(row3.x), _32(row3.y), _33(row3.z), _34(row3.w),
			_41(row4.x), _42(row4.y), _43(row4.z), _44(row4.w)
		{}

		mat4 operator*(const mat4& rhs) const noexcept
		{
			return mat4(
				_11 * rhs._11 + _12 * rhs._21 + _13 * rhs._31 + _14 * rhs._41,
				_11 * rhs._12 + _12 * rhs._22 + _13 * rhs._32 + _14 * rhs._42,
				_11 * rhs._13 + _12 * rhs._23 + _13 * rhs._33 + _14 * rhs._43,
				_11 * rhs._14 + _12 * rhs._24 + _13 * rhs._34 + _14 * rhs._44,

				_21 * rhs._11 + _22 * rhs._21 + _23 * rhs._31 + _24 * rhs._41,
				_21 * rhs._12 + _22 * rhs._22 + _23 * rhs._32 + _24 * rhs._42,
				_21 * rhs._13 + _22 * rhs._23 + _23 * rhs._33 + _24 * rhs._43,
				_21 * rhs._14 + _22 * rhs._24 + _23 * rhs._34 + _24 * rhs._44,

				_31 * rhs._11 + _32 * rhs._21 + _33 * rhs._31 + _34 * rhs._41,
				_31 * rhs._12 + _32 * rhs._22 + _33 * rhs._32 + _34 * rhs._42,
				_31 * rhs._13 + _32 * rhs._23 + _33 * rhs._33 + _34 * rhs._43,
				_31 * rhs._14 + _32 * rhs._24 + _33 * rhs._34 + _34 * rhs._44,

				_41 * rhs._11 + _42 * rhs._21 + _43 * rhs._31 + _44 * rhs._41,
				_41 * rhs._12 + _42 * rhs._22 + _43 * rhs._32 + _44 * rhs._42,
				_41 * rhs._13 + _42 * rhs._23 + _43 * rhs._33 + _44 * rhs._43,
				_41 * rhs._14 + _42 * rhs._24 + _43 * rhs._34 + _44 * rhs._44);
		}

		union
		{
			struct
			{
				float _11, _12, _13, _14;
				float _21, _22, _23, _24;
				float _31, _32, _33, _34;
				float _41, _42, _43, _44;
			};
			float m[4][4]; // [row][column]
		};
	};

	inline mat4 RotationY(float angle)
	{
		const float s = sin(angle), c = cos(angle);
		return mat4(
			c, 0.f, -s, 0.f,
			0.f, 1.f, 0.f, 0.f,
			s, 0.f, c, 0.f,
			0.f, 0.f, 0.f, 1.f);
	}

	inline mat4 Perspective(float fovY, float aspectRatio, float zNear, float zFar)
	{
		float yScale = 1.0f / tan(fovY * 0.5f);
		float xScale = yScale / aspectRatio;
		return mat4(
			xScale, 0.0f, 0.0f, 0.0f,
			0.0f, yScale, 0.0f, 0.0f,
			0.0f, 0.0f, zFar / (zFar - zNear), 1.0f,
			0.0f, 0.0f, -zNear * zFar / (zFar - zNear), 0.0f);
	}

	inline mat4 LookAt(vec3 at, vec3 eye, vec3 up)
	{
		vec3 zAxis = (at - eye).Normalized();
		vec3 xAxis = Cross(up, zAxis).Normalized();
		vec3 yAxis = Cross(zAxis, xAxis);
		return mat4(
			xAxis.x, yAxis.x, zAxis.x, 0.0f,
			xAxis.y, yAxis.y, zAxis.y, 0.0f,
			xAxis.z, yAxis.z, zAxis.z, 0.0f,
			-Dot(xAxis, eye), -Dot(yAxis, eye), -Dot(zAxis, eye), 1.0f);
	}
} // namespace math