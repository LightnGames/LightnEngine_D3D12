#include "include/Vector2.h"
#include "include/MathLib.h"
#include <cmath>

constexpr Vector2 Vector2::zero{ 0.f,0.f };
constexpr Vector2  Vector2::one{ 1.f,1.f };
constexpr Vector2  Vector2::right{ 1.f,0.f };
constexpr Vector2  Vector2::left{ -1.f,0.f };
constexpr Vector2  Vector2::up{ 0.f,1.f };
constexpr Vector2 Vector2::down{ 0.f,-1.f };

float Vector2::length() const {
	return length(*this);
}

float Vector2::length(Vector2 v) {
	return std::sqrtf(v.x*v.x + v.y*v.y);
}

float Vector2::distance(Vector2 v) const {
	return distance(*this, v);
}

float Vector2::distance(Vector2 v1, Vector2 v2) {
	Vector2 v = v2 - v1;

	return v.length();
}

float Vector2::dot(Vector2 v) const {
	return dot(*this, v);
}

float Vector2::dot(Vector2 v1, Vector2 v2) {
	return v1.x * v2.x + v1.y * v2.y;
}

float Vector2::cross(Vector2 v) const {
	return cross(*this, v);
}

float Vector2::cross(Vector2 v1, Vector2 v2) {
	return v1.x*v2.y - v1.y*v2.x;
}

Vector2 Vector2::normalize() {
	float value = 1.0f / length();

	*this *= value;

	return *this;

}

Vector2 Vector2::normalize(Vector2 v) {
	return v.normalize();
}

float Vector2::angle(Vector2 v) const {
	return angle(*this, v);
}

float Vector2::angle(Vector2 v1, Vector2 v2) {	//ベクトルAとBの長さを計算する
	float length_A = length(v1);
	float length_B = length(v2);

	//内積とベクトル長さを使ってcosθを求める
	float cos_sita = dot(v1, v2) / (length_A * length_B);
	//cosθからθを求める
	float sita = acos(cos_sita);

	return sita;
}

bool Vector2::inside(Vector2 min, Vector2 max) const {
	if (min.x > x || max.x < x) return false;
	if (min.y > y || max.y < y) return false;
	return true;
}

bool Vector2::inside(Vector2 p, Vector2 min, Vector2 max) {
	return p.inside(min, max);
}

Vector2 Vector2::Vector2::lerp(const Vector2 & begin, const Vector2 & end, float rate) {
	return Vector2(
		Mathf::lerp(begin.x, end.x, rate),
		Mathf::lerp(begin.y, end.y, rate)
	);
}

Vector2 Vector2::lerp(const Vector2 & target, float t) const {
	return Vector2::lerp(*this, target, t);
}

Vector2 Vector2::operator-()const {
	return Vector2(-x, -y);
}

Vector2 Vector2::operator+(const Vector2 & v) const {
	return Vector2(x + v.x, y + v.y);
}

Vector2 Vector2::operator-(const Vector2 & v) const {
	return Vector2(x - v.x, y - v.y);
}

Vector2 Vector2::operator*(const float s) const {
	return Vector2(x * s, y * s);
}

Vector2 Vector2::operator/(const float s) const {
	return Vector2(x / s, y / s);
}

Vector2 & Vector2::operator+=(const Vector2 & v) {
	x += v.x;
	y += v.y;
	return *this;
}

Vector2 & Vector2::operator-=(const Vector2 & v) {
	x -= v.x;
	y -= v.y;
	return *this;
}

Vector2 & Vector2::operator*=(const float & s) {
	x *= s;
	y *= s;
	return *this;
}

Vector2 & Vector2::operator/=(const float & s) {
	x /= s;
	y /= s;
	return *this;
}

bool Vector2::operator == (const Vector2& v2) const{
	return approximately(x, v2.x)
		&& approximately(y, v2.y);
}

Vector2 & operator*(const float & s, Vector2 & v) {
	v *= s;
	return v;
}