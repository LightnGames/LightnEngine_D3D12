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

	//�]�u�ς݃r���[�s����擾
	Matrix4 getViewMatrixTransposed() const {
		return _mtxViewTransposed;
	}

	//�]�u�ς݃v���W�F�N�V�����s����擾
	Matrix4 getProjectionMatrixTransposed() const {
		return _mtxProjTransposed;
	}

	Matrix4 getProjectionMatrix() const {
		return _mtxProj;
	}

	Vector3 getPosition() const {
		return _position;
	}

	float getFarZ() const {
		return _farZ;
	}

	float getNearZ() const {
		return _nearZ;
	}

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