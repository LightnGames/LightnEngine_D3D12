#pragma once

#include "MathStructHelper.h"
#include "Vector2.h"
#include <string>

//3次元ベクトルクラス
struct Vector3 {

	constexpr Vector3() :x{ 0.0f }, y{ 0.0f }, z{ 0.0f }{
	}

	constexpr Vector3(const float x, const float y, const float z) : x{ x }, y{ y }, z{ z }{
	}

	/*explicit Vector3(const float xyz) :x{ xyz }, y{ xyz }, z{ xyz }{
	}*/

	//外積
	static Vector3 cross(const Vector3& v1, const Vector3& v2);

	//内積
	static float dot(const Vector3& v1, const Vector3& v2);
	float dot(const Vector3& v) const;

	Vector3 reciprocal() const;
	Vector3 safeReciprocal() const;

	//長さ
	static float length(const Vector3& v);
	float length() const;
	//長さの二乗
	static float sqrLength(const Vector3& v);
	float sqrLength() const;

	//二点間の距離
	static float distance(const Vector3& v1, const Vector3& v2);
	float distance(const Vector3& other) const;
	//二点間の距離の二乗
	static float sqrDistance(const Vector3& v1, const Vector3& v2);
	float sqrDistance(const Vector3& other) const;

	//単位ベクトル化
	static Vector3 normalize(const Vector3& v);
	Vector3 normalize() const;

	//二つの角度差を計算
	static float Angle(const Vector3& v1, const Vector3& v2);
	float angle(const Vector3& v) const;

	//二つのベクトルの向きが等しいか
	static float EqualRotator(const Vector3& v1, const Vector3& v2);
	float equalRotation(const Vector3& v) const;

	//線形補完
	static Vector3 lerp(const Vector3& start, const Vector3& end, float t);
	Vector3 lerp(const Vector3& target, float t) const;

	static Vector3 mulComp(const Vector3& v1, const Vector3& v2);
	Vector3 mulComp(const Vector3& other);
	static Vector3 diviComp(const Vector3& v1, const Vector3& v2);
	Vector3 diviComp(const Vector3& other);

	Vector2 toVec2XZ() const;

	std::string toString() const;

	// 添字演算子
	float operator [] (std::size_t index) const;
	float & operator [] (std::size_t index);

	//定数
	static const Vector3 up;
	static const Vector3 right;
	static const Vector3 forward;
	static const Vector3 zero;
	static const Vector3 one;

	union
	{
		struct {
			float x;
			float y;
			float z;
		};
		MathUnionArray<3> _array;
	};
};

// 単項演算子オーバーロード
Vector3 operator + (const Vector3& v);
Vector3 operator - (const Vector3& v);

// 代入演算子オーバーロード
Vector3& operator += (Vector3& v1, const Vector3& v2);
Vector3& operator -= (Vector3& v1, const Vector3& v2);
Vector3& operator *= (Vector3& v, const Vector3& v2);
Vector3& operator *= (Vector3& v, float s);
Vector3& operator /= (Vector3& v, float s);

// ２項演算子オーバーロード
Vector3 operator + (const Vector3& v1, const Vector3& v2);
Vector3 operator - (const Vector3& v1, const Vector3& v2);
Vector3 operator * (const Vector3& v1, const Vector3& v2);
Vector3 operator * (const Vector3& v, float s);
Vector3 operator * (float s, const Vector3& v);
Vector3 operator / (const Vector3& v, float s);

bool operator ==(const Vector3& v1, const Vector3& v2);
bool operator !=(const Vector3& v1, const Vector3& v2);

