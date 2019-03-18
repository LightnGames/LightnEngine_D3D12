#pragma once

#include "LMath.h"

struct Transform {

	Vector3 position;
	Vector3 rotation;
	Vector3 scale;

	Transform() :position{ 0.0f,0.0f,0.0f }, rotation{ 0.0f,0.0f,0.0f, }, scale{ 1.0f,1.0f,1.0f }{}
};

struct TransformQ {

	Vector3 position;
	Quaternion rotation;
	Vector3 scale;

	constexpr TransformQ() :position{ 0.0f,0.0f,0.0f }, rotation{ Quaternion::identity }, scale{ 1.0f,1.0f,1.0f }{}
	constexpr TransformQ(const Vector3& position, const Quaternion& rotation, const Vector3& scale) :position{ position }, rotation{ rotation }, scale{ scale }{}
	TransformQ(const Matrix4 matrix); // îÒconstexprä÷êîÇåƒÇ‘ïKóvÇ™Ç†ÇÈÇÃÇ≈Ç∆ÇËÇ†Ç¶Ç∏ îÒconstexpr
	Matrix4 toMatrix4() const;

	Vector3 InvTransformPosition(const Vector3& pos);
	Quaternion InvTransformRotation(const Quaternion& rot);
	Vector3 InvTransformScale(const Vector3& scl);
};