#pragma once

#include "Vector3.h"
#include "MathStructHelper.h"

struct Matrix4;

struct Quaternion {

	constexpr Quaternion() :x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, w{ 1.0f }{
	}

	constexpr Quaternion(float x, float y, float z, float w) : x{ x }, y{ y }, z{ z }, w{ w }{
	}

	Quaternion(const Vector3& axis, float angle); // 初期化が初期化子で完結しないのでとりあえず 非constexpr

	//クォータニオン同士の内積
	static float dot(const Quaternion& q1, const Quaternion& q2);

	//クォータニオンを球面線形補完する
	static Quaternion slerp(const Quaternion q1, const Quaternion& q2, float t, bool nearRoute = true);

	//オイラー角からクォータニオンを生成
	static Quaternion euler(const Vector3& euler, bool valueIsRadian = false);
	static Quaternion euler(float pitch, float yaw, float roll, bool valueIsRadian = false);

	//ベクトルをクォータニオンで回転
	static Vector3 rotVector(const Quaternion& q, const Vector3& v);

	//directionを向く回転を作成	TODO 内部で inverse を使ってる部分を修正する必要あり
	static Quaternion lookRotation(const Vector3& direction, const Vector3& up = Vector3::up);

	//逆クォータニオン
	Quaternion inverse() const;

	//共役クォータニオン
	Quaternion conjugate() const;

	//オイラー角度を取得
	Vector3 toEulerAngle() const;

	//戻り値はラジアン
	float getRoll(bool reprojectAxis = true) const;
	float getPitch(bool reprojectAxis = true) const;
	float getYaw(bool reprojectAxis = true) const;

	static const Quaternion identity;


	union
	{
		struct
		{
			float x, y, z, w;
		};
		MathUnionArray<4> _array;
	};
};

Quaternion operator - (const Quaternion& q);
bool operator == (const Quaternion& q1, const Quaternion& q2);


Quaternion& operator += (Quaternion& q1, const Quaternion& q2);

Quaternion& operator -= (Quaternion& q1, const Quaternion& q2);

Quaternion& operator *= (Quaternion& q1, const Quaternion& q2);

Quaternion& operator *= (Quaternion& q1, float s);

Quaternion& operator /= (Quaternion& q, float s);

Quaternion operator + (const Quaternion& q1, const Quaternion& q2);

Quaternion operator - (const Quaternion& q1, const Quaternion& q2);

Quaternion operator * (const Quaternion& q1, const Quaternion& q2);

Quaternion operator * (const Quaternion& q, float s);

Quaternion operator * (float s, const Quaternion& q);

Quaternion operator / (const Quaternion & q, float s);
