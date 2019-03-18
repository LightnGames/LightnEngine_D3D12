#pragma once

#include "Vector3.h"
#include "MathStructHelper.h"
#include "MathLib.h"

class Vector4 {

public:

	constexpr Vector4() :x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, w{ 0.0f }{
	}

	constexpr Vector4(const float x, const float y, const float z, const float w) : x{ x }, y{ y }, z{ z }, w{ w }{
	}

	constexpr Vector4(const Vector3& v, const float w = 0.0f) : x{ v.x }, y{ v.y }, z{ v.z }, w{ w }{
	}

	constexpr Vector3 toVector3() {
		return Vector3(x, y, z);
	}

	constexpr bool operator == (const Vector4& v) const { return x == v.x&&y == v.y&&z == v.z&&w == v.w; }

	// ’P€‰‰Z
	constexpr Vector4 operator + () const {
		return *this;
	}
	constexpr Vector4 operator - () const {
		return	Vector4(-x, -y, -z, -w);
	}

	// ‘ã“ü‰‰Z
	constexpr Vector4& operator += (float s) {
		for (auto& i : _array) { i += s; }
		return *this;
	}
	constexpr Vector4& operator -= (float s) {
		for (auto& i : _array) { i -= s; }
		return *this;
	}
	constexpr Vector4& operator *= (float s) {
		for (auto& i : _array) { i *= s; }
		return *this;
	}
	constexpr Vector4& operator /= (float s) {
		for (auto& i : _array) { i /= s; }
		return *this;
	}
	constexpr Vector4& operator += (const Vector4& v) {
		for (size_t i = 0; i < 4; i++) {
			_array[i] += v._array[i];
		}
		return *this;
	}
	constexpr Vector4& operator -= (const Vector4& v) {
		for (size_t i = 0; i < 4; i++) {
			_array[i] -= v._array[i];
		}
		return *this;
	}
	constexpr Vector4& operator *= (const Vector4& v) {
		for (size_t i = 0; i < 4; i++) {
			_array[i] *= v._array[i];
		}
		return *this;
	}
	constexpr Vector4& operator /= (const Vector4& v) {
		for (size_t i = 0; i < 4; i++) {
			_array[i] /= v._array[i];
		}
		return *this;
	}

	//üŒ`•âŠ®
	static Vector4 lerp(const Vector4& start, const Vector4& end, float t) {
		t = clamp(t, 0.0f, 1.0f);

		return start * (1.0f - t) + end * t;
	}

	Vector4 lerp(const Vector4& target, float t) const {
		return Vector4::lerp(*this, target, t);
	}

	// “ñ€‰‰ZiŒvZj
	constexpr Vector4 operator + (const float s) const {
		return Vector4(*this) += s;
	}
	constexpr Vector4 operator - (const float s) const {
		return Vector4(*this) -= s;
	}
	constexpr Vector4 operator * (const float s) const {
		return Vector4(*this) *= s;
	}
	constexpr Vector4 operator / (const float s) const {
		return Vector4(*this) /= s;
	}
	constexpr Vector4 operator + (const Vector4& v) const {
		return Vector4(*this) += v;
	}
	constexpr Vector4 operator - (const Vector4& v) const {
		return Vector4(*this) -= v;
	}
	constexpr Vector4 operator * (const Vector4& v) const {
		return Vector4(*this) *= v;
	}
	constexpr Vector4 operator / (const Vector4& v) const {
		return Vector4(*this) /= v;
	}

	union
	{
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
		MathUnionArray<4> _array; //std::array‚¾‚Æconstexpr ‚ªg‚¦‚È‚©‚Á‚½‚Ì‚Å
	};
};



