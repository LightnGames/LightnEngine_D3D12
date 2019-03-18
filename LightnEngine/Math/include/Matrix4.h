#pragma once

#include "Vector4.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "MathStructHelper.h"

struct Matrix4 {

	constexpr Matrix4() :
		m{
			{0.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 0.0f}
	} {}

	constexpr Matrix4(float m11, float m12, float m13, float m14,
		float m21, float m22, float m23, float m24,
		float m31, float m32, float m33, float m34,
		float m41, float m42, float m43, float m44) :m{
			{ m11, m12, m13, m14},
			{ m21, m22, m23, m24},
			{ m31, m32, m33, m34},
			{ m41, m42, m43, m44 } } {}

	//��]�l�����]�s����쐬
	static Matrix4 rotateX(float pitch);
	static Matrix4 rotateY(float yaw);
	static Matrix4 rotateZ(float roll);
	static Matrix4 rotateXYZ(float pitch, float yaw, float roll);
	static Matrix4 rotateAxis(const Vector3& axis);

	//XYZ����g��k���s��𐶐�
	static Matrix4 scaleXYZ(float x, float y, float z);
	static Matrix4 scaleXYZ(const Vector3& v);

	//XYZ���畽�s�ړ��s��𐶐�
	static Matrix4 translateXYZ(float x, float y, float z);
	static Matrix4 translateXYZ(const Vector3& v);

	//�s��̏�Z
	static Matrix4 multiply(const Matrix4& m1, const Matrix4& m2);
	Matrix4 multiply(const Matrix4& m2) const;

	//�x�N�g���ƍs��̏�Z
	static Vector3 transform(const Vector3& v, const Matrix4& m);

	//�]�u�s��
	static Matrix4 transpose(const Matrix4& m);
	Matrix4 transpose() const;

	//�t�s��
	static Matrix4 inverse(const Matrix4& m);

	//������W�n�ˉe�ϊ��s��𐶐�
	static Matrix4 perspectiveFovLH(float FovAngleY, float AspectHByW, float NearZ, float FarZ);

	//���蕽�t���e�ˉe�Ԋҍs��𐶐�
	static Matrix4 orthographicProjectionLH(float width, float height, float nearZ, float farZ, float farOffset = 0.0f);

	//���s�ړ��s����擾
	Vector3 translate() const;

	//�N�H�[�^�j�I�����擾
	Quaternion rotation() const;

	//�g��k���l���擾
	Vector3 scale() const;

	//�t�s��
	Matrix4 inverse() const;

	//���[���h�s�񂩂烏�[���h���W���擾
	Vector3 positionFromWorld() const;
	Vector3 scaleFromWorld() const;

	//�N�H�[�^�j�I�������]�s��𐶐�
	static Matrix4 matrixFromQuaternion(const Quaternion& q);

	//�ʒu�A��]�A�傫�����烏�[���h�s����쐬
	static Matrix4 createWorldMatrix(const Vector3& position, const Quaternion& rotation, const Vector3& scale);

	const float* operator[](const size_t i) const;
	bool operator == (const Matrix4& m) const;

	Matrix4 operator -();

	Matrix4& operator += (const Matrix4& m);
	Matrix4& operator -= (const Matrix4& m);
	Matrix4& operator *= (const Matrix4& m);
	Matrix4& operator *= (float s);
	Matrix4& operator /= (float s);

	Matrix4 operator + (const Matrix4& m) const;
	Matrix4 operator - (const Matrix4& m) const;
	Matrix4 operator * (const Matrix4& m) const;
	Matrix4 operator * (float s) const;
	Matrix4 operator / (float s) const;

	//�P�ʍs��
	static const Matrix4 identity;
	//�f�o�C�X���K�����W�n����TexCoords�ɕϊ�����s��
	static const Matrix4 textureBias;
	union
	{
		float m[4][4];	// 4x4 �}�g���b�N�X
		Vector4 mv[4];	// Vector4 �z��
		MathUnionArray<16> _array;	// �z��
	};
};

inline void scalarSinCos(float* pSin, float* pCos, float Value);

Matrix4 operator * (float s, const Matrix4& q);