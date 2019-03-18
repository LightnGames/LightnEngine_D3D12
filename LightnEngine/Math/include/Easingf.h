#pragma once

#include "MathLib.h"

namespace Easingf {
constexpr float Linear(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	return (e - s)*t / tt + s;
}

namespace Quad {
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	return e * t*t + s;
}
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	return -e * t*(t - 2) + s;
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	if (t / 2 < 1)
		return e / 2 * t * t + s;
	--t;
	return -e * (t * (t - 2) - 1) + s;
}
}
namespace Cubic {
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	return e * t*t*t + s;
}
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t = t / tt - 1;
	return e * (t*t*t + 1) + s;
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	if (t / 2 < 1)
		return e / 2 * t*t*t + s;
	t -= 2;
	return e / 2 * (t*t*t + 2) + s;
}
}
namespace Quart {
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	return e * t*t*t*t + s;
}
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t = t / tt - 1;
	return -e * (t*t*t*t - 1) + s;
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	if (t / 2 < 1)
		return e / 2 * t*t*t*t + s;
	t -= 2;
	return -e / 2 * (t*t*t*t - 2) + s;
}
}
namespace Quint {
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	return e * t*t*t*t*t + s;
}
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t = t / tt - 1;
	return e * (t*t*t*t*t + 1) + s;
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	if (t / 2 < 1)
		return e / 2 * t*t*t*t*t + s;
	t -= 2;
	return e / 2 * (t*t*t*t*t + 2) + s;
}
}
namespace Sine {
float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	return -e * Mathf::cos(t * Mathf::degToRad * 90.f / tt) + e + s;
}
float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	return e * Mathf::sin(t * Mathf::degToRad * 90.f / tt) + s;
}
float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	return -e / 2 * (Mathf::cos(t * Mathf::Pi / tt) - 1) + s;
}
}
namespace Exp {
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	return t == 0.0 ? s : e * Mathf::pow(2, 10 * (t / tt - 1)) + s;
}
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	return t == tt ? e + s : e * (-Mathf::pow(2, -10 * t / tt) + 1) + s;
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	if (t == 0.0)
		return s;
	if (t == tt)
		return e;
	e -= s;
	t /= tt;

	if (t / 2 < 1)
		return e / 2 * Mathf::pow(2, 10 * (t - 1)) + s;
	--t;
	return e / 2 * (-Mathf::pow(2, -10 * t) + 2) + s;

}
}
namespace Circ {
float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	return -e * (Mathf::sqrt(1 - t * t) - 1) + s;
}
float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t = t / tt - 1;
	return e * Mathf::sqrt(1 - t * t) + s;
}
float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;
	if (t / 2 < 1)
		return -e / 2 * (Mathf::sqrt(1 - t * t) - 1) + s;
	t -= 2;
	return e / 2 * (Mathf::sqrt(1 - t * t) + 1) + s;
}
}
namespace Back {
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f, float a = 1.70158f) {
	e -= s;
	t /= tt;
	return e * t*t*((a + 1)*t - a) + s;
}
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f, float a = 1.70158f) {
	e -= s;
	t = t / tt - 1;
	return e * (t * t * ((a + 1) * t * a) + 1) + s;
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f, float a = 1.70158f) {
	e -= s;
	a *= 1.525f;
	if (t / 2 < 1) {
		return e * (t*t*((a + 1)*t - a)) + s;
	}
	t -= 2;
	return e / 2 * (t*t*((a + 1)*t + a) + 2) + s;
}
}
namespace Bounce {
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	e -= s;
	t /= tt;

	if (t < 1 / 2.75f)
		return e * (7.5625f*t*t) + s;
	else if (t < 2 / 2.75f) {
		t -= 1.5f / 2.75f;
		return e * (7.5625f*t*t + 0.75f) + s;
	} else if (t < 2.5f / 2.75f) {
		t -= 2.25f / 2.75f;
		return e * (7.5625f*t*t + 0.9375f) + s;
	} else {
		t -= 2.625f / 2.75f;
		return e * (7.5625f*t*t + 0.984375f) + s;
	}
}
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	return e - out(tt - t, tt, e - s, 0) + s;
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f) {
	if (t < tt / 2)
		return in(t * 2, tt, e - s, e)*0.5f + s;
	else
		return out(t * 2 - tt, tt, e - s, 0)*0.5f + s + (e - s)*0.5f;
}
}
namespace Elastic {
constexpr float defaultPow = 10.f;
constexpr float in(float t, float tt = 1.f, float s = 0.f, float e = 1.f, float pow = defaultPow) {
	if (t == 0) return s;  if ((t /= tt) == 1) return s + e;
	float p = tt * 0.3f;
	float qp = p / 4;
	float postFix = e * Mathf::pow(2, pow*(t -= 1)); // this is a fix, again, with post-increment operators
	return -(postFix * sin((t*tt - qp)*(2 * float(Mathf::Pi)) / p)) + s;
}
constexpr float out(float t, float tt = 1.f, float s = 0.f, float e = 1.f, float pow = defaultPow) {
	if (t == 0) return s;  if ((t /= tt) == 1) return s + e;
	float p = tt * 0.3f;
	float qp = p / 4;
	return (e*std::powf(2, -pow * t) * sin((t*tt - qp)*(2 * Mathf::Pi) / p) + e + s);
}
constexpr float inOut(float t, float tt = 1.f, float s = 0.f, float e = 1.f, float pow = defaultPow) {
	if (t == 0) return s;  if ((t /= tt / 2) == 2) return s + e;
	float p = tt * (0.3f*1.5f);
	float qp = p / 4;
	if (t < 1) {
		float postFix = e * std::powf(2, pow*(t -= 1)); // postIncrement is evil
		return -.5f*(postFix* sin((t*tt - qp)*(2 * Mathf::Pi) / p)) + s;
	}
	float postFix = e * std::powf(2, -pow * (t -= 1)); // postIncrement is evil
	return postFix * sin((t*tt - qp)*(2 * Mathf::Pi) / p)*.5f + e + s;
}
}
}