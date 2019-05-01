#pragma once

#include <float.h>
#include <cmath>
#include <utility>

constexpr float EPSILON = 0.000001f;
constexpr float M_PI = 3.141592654f;
constexpr float M_2PI = 6.283185307f;
constexpr float M_1DIVPI = 0.318309886f;
constexpr float M_1DIV2PI = 0.159154943f;
constexpr float M_PIDIV2 = 1.570796327f;
constexpr float M_PIDIV4 = 0.785398163f;

//度数からラジアンに変換
constexpr inline float radianFromDegree(const float degree) {
	return degree * (M_PI / 180.0f);
}

float clamp(float value, float min, float max);
float lerp(float value, float min, float max);
bool approximately(float a, float b);

namespace Mathf
{
constexpr float Pi = 3.14159265358979f;
constexpr float Eps = FLT_EPSILON;
constexpr float Max = FLT_MAX;
constexpr float Pi2 = 6.283185307f;
constexpr float HalfPI = 1.570796327f;
constexpr float QuatPI = 0.785398163f;
constexpr float DivPI = 0.318309886f;
constexpr float DivHPI = 0.159154943f;
constexpr float radToDeg{ 180.f / Mathf::Pi };
constexpr float degToRad{ Mathf::Pi / 180.f };
constexpr float gravity = 9.80665f; // m/ss

template <class T, class... Args>
constexpr T cmax(const T& head, const Args&... args) {
	// わかりやすいが 汎用性に欠ける(int , float)のようなパターンに対応できない。
	/*std::initializer_list<int> list = { args... };
	auto&& m = *std::max_element(list.begin(), list.end());
	return head > m ? head : m;*/
	T result = head;
	using swallow = std::initializer_list<T>;	//縮小キャストを強引に回避
	(void)swallow {
		(void(result = result > args ? result : args), T())...
	};
	return result;
}
template <class T, class... Args>
constexpr T cmin(const T& head, const Args&... args) {
	T result = head;
	using swallow = std::initializer_list<T>;
	(void)swallow {
		(void(result = result < args ? result : args), T())...
	};
	return result;
}
template <class T>
constexpr T clamp(const T& value, const T& min, const T& max) {
	return	Mathf::cmax(min, Mathf::cmin(value, max));
}
template <class T>
constexpr T abs(const T& value) {
	return std::abs(value);
}
template <class T>
constexpr T distance(T n1, T n2) {
	return abs(n1 - n2);
}
constexpr float lerp(float value1, float value2, float amount) {
	return (value1 * (1.0f - amount)) + (value2 * amount);
}
inline float sin(float radian) {
	return std::sin(radian);
}
inline float cos(float radian) {
	return std::cos(radian);
}
inline float tan(float radian) {
	return std::tan(radian);
}
inline float asin(float s) {
	return std::asin(s);
}
inline float acos(float c) {
	return std::acos(c);
}
inline float atan(float y, float x) {
	return std::atan2(y, x);
}
inline float atan(float t) {
	return std::atan(t);
}

inline float sqrt(float f) {
	return std::sqrtf(f);
}

inline float pow(float x, float y) {
	return std::pow(x, y);
}

inline bool approximately(float l, float r) {
	return std::fabs(l - r) < FLT_EPSILON;
}

};