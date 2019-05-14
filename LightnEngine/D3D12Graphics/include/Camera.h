#pragma once

#include <LMath.h>
#include <Type.h>

class Camera {
public:
	Camera() :
		_mtxProj(Matrix4::identity),
		_mtxView(Matrix4::identity),
		_position(Vector3::zero),
		_rotation(Quaternion::identity),
		_fieldOfView(0),
		_aspectRate(0),
		_nearZ(0),
		_farZ(0) {
	}

	void setFieldOfView(float fieldOfViewDegree) {
		_fieldOfView = radianFromDegree(fieldOfViewDegree);
	}

	void setAspectRate(float aspectRate) {
		_aspectRate = aspectRate;
	}

	//�����Əc�̉𑜓x����A�X�y�N�g����v�Z
	void setAspectRate(uint32 width, uint32 height) {
		_aspectRate = width / static_cast<float>(height);
	}

	void setNearZ(float nearZ) {
		_nearZ = nearZ;
	}

	void setFarZ(float farZ) {
		_farZ = farZ;
	}

	void setPosition(const Vector3& position) {
		_position = position;
	}

	void setPosition(float x, float y, float z) {
		_position = Vector3(x, y, z);
	}

	void setRotation(const Quaternion& rotation) {
		_rotation = rotation;
	}

	//�I�C���[�p�x�����]��ݒ�
	void setRotationEuler(float x, float y, float z, float isRadian = false) {
		_rotation = Quaternion::euler(Vector3(x, y, z), isRadian);
	}

	//�ˉe�ϊ��s��𐶐�
	void computeProjectionMatrix() {
		_mtxProj = Matrix4::perspectiveFovLH(_fieldOfView, _aspectRate, _nearZ, _farZ);
		_mtxProjTransposed = _mtxProj.transpose();
	}

	//�r���[�s����v�Z
	void computeViewMatrix() {
		_mtxView = Matrix4::createWorldMatrix(_position, _rotation, Vector3::one).inverse();
		_mtxViewTransposed = _mtxView.transpose();
	}

	void computeFlustomNormals() {
		Matrix4 virtualProj = getProjectionMatrix();
		float x = 1 / virtualProj[0][0];
		float y = 1 / virtualProj[1][1];

		Vector3 leftNormal = Vector3::cross(Vector3(-x, 0, 1), -Vector3::up).normalize();
		Vector3 rightNormal = Vector3::cross(Vector3(x, 0, 1), Vector3::up).normalize();
		Vector3 bottomNormal = Vector3::cross(Vector3(0, y, 1), -Vector3::right).normalize();
		Vector3 topNormal = Vector3::cross(Vector3(0, -y, 1), Vector3::right).normalize();
		leftNormal = Quaternion::rotVector(_rotation, leftNormal);
		rightNormal = Quaternion::rotVector(_rotation, rightNormal);
		bottomNormal = Quaternion::rotVector(_rotation, bottomNormal);
		topNormal = Quaternion::rotVector(_rotation, topNormal);


		_frustumPlanes[0] = leftNormal;
		_frustumPlanes[1] = rightNormal;
		_frustumPlanes[2] = bottomNormal;
		_frustumPlanes[3] = topNormal;
	}

	//�]�u�ς݃r���[�s����擾
	Matrix4 getViewMatrixTransposed() const {
		return _mtxViewTransposed;
	}

	//�]�u�ς݃v���W�F�N�V�����s����擾
	Matrix4 getProjectionMatrixTransposed() const {
		return _mtxProjTransposed;
	}

	Matrix4 getViewMatrix() const {
		return _mtxView;
	}

	Matrix4 getProjectionMatrix() const {
		return _mtxProj;
	}

	Vector3 getPosition() const {
		return _position;
	}

	Quaternion getRotation() const {
		return _rotation;
	}

	float getFarZ() const {
		return _farZ;
	}

	float getNearZ() const {
		return _nearZ;
	}

	Vector3 _frustumPlanes[4];
private:
	float _fieldOfView;
	float _aspectRate;
	float _nearZ;
	float _farZ;

	Matrix4 _mtxProj;
	Matrix4 _mtxView;
	Matrix4 _mtxProjTransposed;
	Matrix4 _mtxViewTransposed;
	Vector3 _position;
	Quaternion _rotation;
};