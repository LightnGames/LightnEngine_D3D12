#include "Camera.h"
#include "DebugGeometry.h"

void Camera::setFieldOfView(float fieldOfViewDegree){
	_fieldOfView = radianFromDegree(fieldOfViewDegree);
}

void Camera::setAspectRate(float aspectRate){
	_aspectRate = aspectRate;
}

void Camera::setAspectRate(uint32 width, uint32 height){
	_aspectRate = width / static_cast<float>(height);
}

void Camera::setNearZ(float nearZ){
	_nearZ = nearZ;
}

void Camera::setFarZ(float farZ){
	_farZ = farZ;
}

void Camera::setPosition(const Vector3& position){
	_position = position;
}

void Camera::setPosition(float x, float y, float z){
	_position = Vector3(x, y, z);
}

void Camera::setRotation(const Quaternion& rotation){
	_rotation = rotation;
}

void Camera::setRotationEuler(float x, float y, float z, float isRadian){
	_rotation = Quaternion::euler(Vector3(x, y, z), isRadian);
}

void Camera::computeProjectionMatrix(){
	_mtxProj = Matrix4::perspectiveFovLH(_fieldOfView, _aspectRate, _nearZ, _farZ);
	_mtxProjTransposed = _mtxProj.transpose();
}

void Camera::computeViewMatrix(){
	_mtxView = Matrix4::createWorldMatrix(_position, _rotation, Vector3::one).inverse();
	_mtxViewTransposed = _mtxView.transpose();
}

void Camera::computeFlustomNormals(){
	const Vector2 fTan = getTanHeightXY();

	Vector3 leftNormal = Vector3::cross(Vector3(-fTan.x, 0, 1), -Vector3::up).normalize();
	Vector3 rightNormal = Vector3::cross(Vector3(fTan.x, 0, 1), Vector3::up).normalize();
	Vector3 bottomNormal = Vector3::cross(Vector3(0, fTan.y, 1), -Vector3::right).normalize();
	Vector3 topNormal = Vector3::cross(Vector3(0, -fTan.y, 1), Vector3::right).normalize();
	leftNormal = Quaternion::rotVector(_rotation, leftNormal);
	rightNormal = Quaternion::rotVector(_rotation, rightNormal);
	bottomNormal = Quaternion::rotVector(_rotation, bottomNormal);
	topNormal = Quaternion::rotVector(_rotation, topNormal);

	_frustumPlanes.normals[0] = leftNormal;
	_frustumPlanes.normals[1] = rightNormal;
	_frustumPlanes.normals[2] = bottomNormal;
	_frustumPlanes.normals[3] = topNormal;
}

void Camera::debugDrawFlustom(){
	DebugGeometryRender& debugGeometryRender = DebugGeometryRender::instance();
	const Vector2 fTan = getTanHeightXY();
	Color lineColor = Color::blue;

	Vector3 forwardNear = Quaternion::rotVector(_rotation, Vector3::forward);
	Vector3 topLeft = Quaternion::rotVector(_rotation, Vector3(-fTan.x, fTan.y, 1));
	Vector3 topRight = Quaternion::rotVector(_rotation, Vector3(fTan.x, fTan.y, 1));
	Vector3 bottomLeft = Quaternion::rotVector(_rotation, Vector3(-fTan.x, -fTan.y, 1));
	Vector3 bottomRight = Quaternion::rotVector(_rotation, Vector3(fTan.x, -fTan.y, 1));

	Vector3 farTopLeft = topLeft * _farZ + _position;
	Vector3 farTopRight = topRight * _farZ + _position;
	Vector3 farBottomLeft = bottomLeft * _farZ + _position;
	Vector3 farBottomRight = bottomRight * _farZ + _position;

	Vector3 nearTopLeft = topLeft * _nearZ + _position;
	Vector3 nearTopRight = topRight * _nearZ + _position;
	Vector3 nearBottomLeft = bottomLeft * _nearZ + _position;
	Vector3 nearBottomRight = bottomRight * _nearZ + _position;

	debugGeometryRender.debugDrawLine(_position, farTopLeft, lineColor);
	debugGeometryRender.debugDrawLine(_position, farTopRight, lineColor);
	debugGeometryRender.debugDrawLine(_position, farBottomLeft, lineColor);
	debugGeometryRender.debugDrawLine(_position, farBottomRight, lineColor);

	debugGeometryRender.debugDrawLine(farTopRight, farTopLeft, lineColor);
	debugGeometryRender.debugDrawLine(farTopLeft, farBottomLeft, lineColor);
	debugGeometryRender.debugDrawLine(farBottomRight, farBottomLeft, lineColor);
	debugGeometryRender.debugDrawLine(farBottomRight, farTopRight, lineColor);

	debugGeometryRender.debugDrawLine(nearTopRight, nearTopLeft, lineColor);
	debugGeometryRender.debugDrawLine(nearTopLeft, nearBottomLeft, lineColor);
	debugGeometryRender.debugDrawLine(nearBottomRight, nearBottomLeft, lineColor);
	debugGeometryRender.debugDrawLine(nearBottomRight, nearTopRight, lineColor);

	for (uint32 i = 0; i < 4; ++i) {
		debugGeometryRender.debugDrawLine(_position, _position + _frustumPlanes.normals[i], Color::yellow);
	}
}

Matrix4 Camera::getViewMatrixTransposed() const{
	return _mtxViewTransposed;
}

Matrix4 Camera::getProjectionMatrixTransposed() const{
	return _mtxProjTransposed;
}

Matrix4 Camera::getViewMatrix() const{
	return _mtxView;
}

Matrix4 Camera::getProjectionMatrix() const{
	return _mtxProj;
}

Vector3 Camera::getPosition() const{
	return _position;
}

Quaternion Camera::getRotation() const{
	return _rotation;
}

float Camera::getFarZ() const{
	return _farZ;
}

float Camera::getNearZ() const{
	return _nearZ;
}

const FrustumPlanes& Camera::getFrustumPlaneNormals() const{
	return _frustumPlanes;
}

Vector3 Camera::getFrustumPlaneNormal(uint32 index) const{
	return _frustumPlanes.normals[index];
}

Vector2 Camera::getTanHeightXY() const{
	return Vector2(1 / _mtxProj[0][0], 1 / _mtxProj[1][1]);
}
