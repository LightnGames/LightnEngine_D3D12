#include "include/Vector3.h"
#include "include/Vector2.h"
#include "include/MathLib.h"

constexpr Vector3 Vector3::up{ 0.0f, 1.0f, 0.0f };
constexpr Vector3 Vector3::right{ 1.0f, 0.0f, 0.0f };
constexpr Vector3 Vector3::forward{ 0.0f, 0.0f, 1.0f };
constexpr Vector3 Vector3::zero{ 0.0f, 0.0f, 0.0f };
constexpr Vector3 Vector3::one{ 1.0f, 1.0f, 1.0f };

Vector3 Vector3::cross(const Vector3 & v1, const Vector3 & v2) {

	Vector3 result = {
		v1.y * v2.z - v1.z * v2.y,
		v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x
	};

	return result;
}

float Vector3::dot(const Vector3 & v1, const Vector3 & v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float Vector3::dot(const Vector3 & v) const {
	return Vector3::dot(*this, v);
}

Vector3 Vector3::reciprocal() const {
	return Vector3(1.f / x, 1.f / y, 1.f / z);
}

#include<assert.h>
Vector3 Vector3::safeReciprocal() const {
	if (x*y*z < Mathf::Eps) {
		assert(x*y*z);
		return Vector3::zero;
	}
	return reciprocal();
}

float Vector3::length(const Vector3 & v) {
	return (float)std::sqrt(dot(v, v));
}

float Vector3::length() const {
	return Vector3::length(*this);
}

float Vector3::sqrLength(const Vector3& v) {
	return dot(v, v);
}

float Vector3::sqrLength() const {
	return Vector3::sqrLength(*this);
}

float Vector3::distance(const Vector3 & v1, const Vector3 & v2) {
	return length(v2 - v1);
}

float Vector3::distance(const Vector3 & other) const {
	return Vector3::distance(*this, other);
}

float Vector3::sqrDistance(const Vector3 & v1, const Vector3 & v2) {
	return sqrLength(v2 - v1);
}

float Vector3::sqrDistance(const Vector3 & other) const {
	return Vector3::distance(*this, other);
}

Vector3 Vector3::normalize(const Vector3 & v) {
	Vector3 result = v;
	float len = length(result);
	if (len != 0) {
		result /= len;
	}
	return result;
}

Vector3 Vector3::normalize() const {
	return Vector3::normalize(*this);
}

float Vector3::Angle(const Vector3 & v1, const Vector3 & v2) {
	float d = dot(normalize(v1), normalize(v2));
	return acos(clamp(d, -1.0f, 1.0f));
}

float Vector3::angle(const Vector3 & v) const {
	return Vector3::Angle(*this, v);
}

float Vector3::EqualRotator(const Vector3 & v1, const Vector3 & v2) {

	const Vector3 normV1 = normalize(v1);
	const Vector3 normV2 = normalize(v2);

	int result = 0;
	result += normV1.x == normV2.x;
	result += normV1.y == normV2.y;
	result += normV1.z == normV2.z;

	return result == 3;
}

float Vector3::equalRotation(const Vector3 & v) const {
	return Vector3::EqualRotator(*this, v);
}

Vector3 Vector3::lerp(const Vector3 & start, const Vector3 & end, float t) {

	t = clamp(t, 0.0f, 1.0f);

	return start * (1.0f - t) + end * t;
}

Vector3 Vector3::lerp(const Vector3 & target, float t) const {
	return Vector3::lerp(*this, target, t);
}

Vector3 Vector3::mulComp(const Vector3 & v1, const Vector3 & v2) {
	return { v1.x * v2.x ,v1.y * v2.y,v1.z * v2.z };
}

Vector3 Vector3::mulComp(const Vector3 & other) {
	return Vector3::mulComp(*this, other);
}

Vector3 Vector3::diviComp(const Vector3 & v1, const Vector3 & v2) {
	return Vector3(v1.x / v2.x, v1.y / v2.y, v1.z / v2.z);
}

Vector3 Vector3::diviComp(const Vector3 & other) {
	return Vector3(Vector3::diviComp(*this, other));
}

Vector2 Vector3::toVec2XZ() const {
	return Vector2(x, z);
}

std::string Vector3::toString() const {
	return "X:" + std::to_string(x) + " Y:" + std::to_string(y) + " Z:" + std::to_string(z);
}

float Vector3::operator[](std::size_t index) const {
	return _array[index];
}

float & Vector3::operator[](std::size_t index) {
	return _array[index];
}

Vector3 operator + (const Vector3& v) {
	return v;
}

Vector3 operator - (const Vector3& v) {
	Vector3 result = { -v.x, -v.y, -v.z };
	return result;
}

Vector3& operator += (Vector3& v1, const Vector3& v2) {
	v1.x += v2.x;
	v1.y += v2.y;
	v1.z += v2.z;
	return v1;
}

Vector3& operator -= (Vector3& v1, const Vector3& v2) {
	v1.x -= v2.x;
	v1.y -= v2.y;
	v1.z -= v2.z;
	return v1;
}

Vector3& operator *= (Vector3& v, float s) {
	v.x *= s;
	v.y *= s;
	v.z *= s;
	return v;
}

Vector3& operator*=(Vector3 & v, const Vector3 & v2) {
	v.x = v.x * v2.x;
	v.y = v.y * v2.y;
	v.z = v.z * v2.z;
	return v;
};

Vector3& operator /= (Vector3& v, float s) {
	v *= 1.0f / s;
	return v;
}

Vector3 operator + (const Vector3& v1, const Vector3& v2) {
	Vector3 result = v1;
	return result += v2;
}

Vector3 operator - (const Vector3& v1, const Vector3& v2) {
	Vector3 result = v1;
	return result -= v2;
}

Vector3 operator*(const Vector3 & v1, const Vector3 & v2) {
	return Vector3(v1) *= v2;
}

Vector3 operator * (const Vector3& v, float s) {
	Vector3 result = v;
	return result *= s;
}

Vector3 operator * (float s, const Vector3& v) {
	Vector3 result = v;
	return result *= s;
}

Vector3 operator / (const Vector3& v, float s) {
	Vector3 result = v;
	return result /= s;
}

bool operator == (const Vector3& v1, const Vector3& v2) {
	return approximately(v1.x, v2.x)
		&& approximately(v1.y, v2.y)
		&& approximately(v1.z, v2.z);
}

bool operator != (const Vector3& v1, const Vector3& v2) {
	return !(v1 == v2);
}