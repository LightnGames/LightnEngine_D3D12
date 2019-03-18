#pragma once

#include "MathStructHelper.h"

struct Vector2 {

	constexpr Vector2() :x(0.f), y(0.f) {}

	constexpr Vector2(const float x, const float y = 0.f) : x{ x }, y{ y }{
	}
	constexpr Vector2(int x, int y)
		: x{ static_cast<float>(x) }
		, y{ static_cast<float>(y) } {}

	float length() const;
	static float length(Vector2 v);

	float distance(Vector2 v) const;
	static float distance(Vector2 v1, Vector2 v2);

	float dot(Vector2 v) const;
	static float dot(Vector2 v1, Vector2 v2);

	float cross(Vector2 v) const;
	static float cross(Vector2 v1, Vector2 v2);

	Vector2 normalize();
	static Vector2 normalize(Vector2 v);

	float angle(Vector2 v) const;
	static float angle(Vector2 v1, Vector2 v2);

	bool inside(Vector2 min, Vector2 max) const;
	static bool inside(Vector2 p, Vector2 min, Vector2 max);

	static Vector2 lerp(const Vector2& begin, const Vector2& end, float t);
	Vector2 lerp(const Vector2& target, float t) const;

	Vector2 operator -()const;

	Vector2 operator +(const Vector2& v) const;
	Vector2 operator -(const Vector2& v) const;
	Vector2 operator *(const float s) const;
	Vector2 operator /(const float s) const;

	Vector2& operator +=(const Vector2& v);
	Vector2& operator -=(const Vector2& v);
	Vector2& operator *=(const float& s);
	Vector2& operator /=(const float& s);

	bool operator ==(const Vector2& v2) const;

	//íËêî
	static const Vector2 zero;
	static const Vector2 one;
	static const Vector2 right;
	static const Vector2 left;
	static const Vector2 up;
	static const Vector2 down;

	union
	{
		struct
		{
			float x;
			float y;
		};
		MathUnionArray<2> _array;
	};
};

Vector2& operator *(const float& s, Vector2& v);