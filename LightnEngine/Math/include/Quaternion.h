#pragma once

#include "Vector3.h"
#include "MathStructHelper.h"

struct Matrix4;

struct Quaternion {

	constexpr Quaternion() :x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, w{ 1.0f }{
	}

	constexpr Quaternion(float x, float y, float z, float w) : x{ x }, y{ y }, z{ z }, w{ w }{
	}

	Quaternion(const Vector3& axis, float angle); // ���������������q�Ŋ������Ȃ��̂łƂ肠���� ��constexpr

	//�N�H�[�^�j�I�����m�̓���
	static float dot(const Quaternion& q1, const Quaternion& q2);

	//�N�H�[�^�j�I�������ʐ��`�⊮����
	static Quaternion slerp(const Quaternion q1, const Quaternion& q2, float t, bool nearRoute = true);

	//�I�C���[�p����N�H�[�^�j�I���𐶐�
	static Quaternion euler(const Vector3& euler, bool valueIsRadian = false);
	static Quaternion euler(float pitch, float yaw, float roll, bool valueIsRadian = false);

	//�x�N�g�����N�H�[�^�j�I���ŉ�]
	static Vector3 rotVector(const Quaternion& q, const Vector3& v);

	//direction��������]���쐬	TODO ������ inverse ���g���Ă镔�����C������K�v����
	static Quaternion lookRotation(const Vector3& direction, const Vector3& up = Vector3::up);

	//�t�N�H�[�^�j�I��
	Quaternion inverse() const;

	//�����N�H�[�^�j�I��
	Quaternion conjugate() const;

	//�I�C���[�p�x���擾
	Vector3 toEulerAngle() const;

	//�߂�l�̓��W�A��
	float getRoll(bool reprojectAxis = true) const;
	float getPitch(bool reprojectAxis = true) const;
	float getYaw(bool reprojectAxis = true) const;

	static const Quaternion identity;


	union
	{
		struct
		{
			float x, y, z, w;
		};
		MathUnionArray<4> _array;
	};
};

Quaternion operator - (const Quaternion& q);
bool operator == (const Quaternion& q1, const Quaternion& q2);


Quaternion& operator += (Quaternion& q1, const Quaternion& q2);

Quaternion& operator -= (Quaternion& q1, const Quaternion& q2);

Quaternion& operator *= (Quaternion& q1, const Quaternion& q2);

Quaternion& operator *= (Quaternion& q1, float s);

Quaternion& operator /= (Quaternion& q, float s);

Quaternion operator + (const Quaternion& q1, const Quaternion& q2);

Quaternion operator - (const Quaternion& q1, const Quaternion& q2);

Quaternion operator * (const Quaternion& q1, const Quaternion& q2);

Quaternion operator * (const Quaternion& q, float s);

Quaternion operator * (float s, const Quaternion& q);

Quaternion operator / (const Quaternion & q, float s);
