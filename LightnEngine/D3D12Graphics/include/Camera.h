#pragma once

#include <Utility.h>
#include <LMath.h>
#include <Type.h>

struct FrustumPlanes {
	Vector3 normals[4];
};

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

	//����p��ݒ�(�f�O���[)
	void setFieldOfView(float fieldOfViewDegree);

	//�����Əc�̉𑜓x����A�X�y�N�g����v�Z
	void setAspectRate(uint32 width, uint32 height);
	void setAspectRate(float aspectRate);

	void setNearZ(float nearZ);
	void setFarZ(float farZ);

	void setPosition(const Vector3& position);
	void setPosition(float x, float y, float z);

	void setRotation(const Quaternion& rotation);

	//�I�C���[�p�x�����]��ݒ�
	void setRotationEuler(float x, float y, float z, float isRadian = false);

	//�ˉe�ϊ��s��𐶐�
	void computeProjectionMatrix();

	//�r���[�s����v�Z
	void computeViewMatrix();

	//�����䕽�ʂ̖@���x�N�g�������݂̃r���[�s�񂩂�v�Z����
	void computeFlustomNormals();

	void debugDrawFlustom();

	//�]�u�ς݃r���[�s����擾
	Matrix4 getViewMatrixTransposed() const;

	//�]�u�ς݃v���W�F�N�V�����s����擾
	Matrix4 getProjectionMatrixTransposed() const;

	//�J���������擾
	Matrix4 getViewMatrix() const;
	Matrix4 getProjectionMatrix() const;
	Vector3 getPosition() const;
	Quaternion getRotation() const;

	float getFarZ() const;
	float getNearZ() const;

	//�����䕽�ʂ̖@���x�N�g�����擾
	const FrustumPlanes& getFrustumPlaneNormals() const;
	Vector3 getFrustumPlaneNormal(uint32 index) const;

	//���݂�FOV�ɂ�����1/Tan(fov/2)���擾����
	Vector2 getTanHeightXY() const;

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
	FrustumPlanes _frustumPlanes;
};