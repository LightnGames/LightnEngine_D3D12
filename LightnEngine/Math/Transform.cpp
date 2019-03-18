#include "include/Transform.h"

TransformQ::TransformQ(const Matrix4 mat)
	: position{ mat.positionFromWorld() }
	, rotation{ mat.rotation() }
	, scale{ mat.scale() }{}

Matrix4 TransformQ::toMatrix4() const {
	return Matrix4::createWorldMatrix(position, rotation, scale);
}

Vector3 TransformQ::InvTransformPosition(const Vector3 & pos) {
	// 位置を考慮
	const Vector3 transrated = pos - position;
	// 向きを考慮
	const Vector3 rotated = Quaternion::rotVector(rotation.inverse(), transrated);
	// スケールを考慮
	return rotated * scale.safeReciprocal();
}

Quaternion TransformQ::InvTransformRotation(const Quaternion & rot) {
	return rotation.inverse() * rot;
}

Vector3 TransformQ::InvTransformScale(const Vector3 & scl) {
	return scl * scale.safeReciprocal();
}
