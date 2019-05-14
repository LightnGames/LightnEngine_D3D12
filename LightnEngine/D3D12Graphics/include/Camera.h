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

	//視野角を設定(デグリー)
	void setFieldOfView(float fieldOfViewDegree);

	//横幅と縦の解像度からアスペクト比を計算
	void setAspectRate(uint32 width, uint32 height);
	void setAspectRate(float aspectRate);

	void setNearZ(float nearZ);
	void setFarZ(float farZ);

	void setPosition(const Vector3& position);
	void setPosition(float x, float y, float z);

	void setRotation(const Quaternion& rotation);

	//オイラー角度から回転を設定
	void setRotationEuler(float x, float y, float z, float isRadian = false);

	//射影変換行列を生成
	void computeProjectionMatrix();

	//ビュー行列を計算
	void computeViewMatrix();

	//視錐台平面の法線ベクトルを現在のビュー行列から計算する
	void computeFlustomNormals();

	void debugDrawFlustom();

	//転置済みビュー行列を取得
	Matrix4 getViewMatrixTransposed() const;

	//転置済みプロジェクション行列を取得
	Matrix4 getProjectionMatrixTransposed() const;

	//カメラ情報を取得
	Matrix4 getViewMatrix() const;
	Matrix4 getProjectionMatrix() const;
	Vector3 getPosition() const;
	Quaternion getRotation() const;

	float getFarZ() const;
	float getNearZ() const;

	//視錐台平面の法線ベクトルを取得
	const FrustumPlanes& getFrustumPlaneNormals() const;
	Vector3 getFrustumPlaneNormal(uint32 index) const;

	//現在のFOVにおける1/Tan(fov/2)を取得する
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