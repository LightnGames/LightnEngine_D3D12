#include "include/Quaternion.h"
#include "include/MathLib.h"
#include "include/Matrix4.h"

constexpr Quaternion Quaternion::identity{ 0.0f,0.0f,0.0f,1.0f };

Quaternion::Quaternion(const Vector3 & axis, float angle) {
	const float sin = Mathf::sin(angle / 2.0f);
	x = sin * axis.x;
	y = sin * axis.y;
	z = sin * axis.z;
	w = Mathf::cos(angle / 2.0f);
}



Quaternion Quaternion::slerp(const Quaternion q1, const Quaternion & q2, float t, bool nearRoute) {
	t = clamp(t, 0, 1);
	float cos = dot(q1, q2);
	Quaternion t2 = q2;
	if (cos < 0.0f) {
		cos = -cos;
		t2 = -q2;
	}

	float k0 = 1.0f - t;
	float k1 = t;
	if ((1.0f - cos) > 1.192092896e-07F) {
		float theta = (float)std::acos(cos);
		k0 = (float)(std::sin(theta*k0) / std::sin(theta));
		k1 = (float)(std::sin(theta*k1) / std::sin(theta));
	}
	return q1 * k0 + t2 * k1;
}

Quaternion Quaternion::euler(const Vector3 & euler, bool valueIsRadian) {
	return Quaternion::euler(euler.x, euler.y, euler.z, valueIsRadian);
}

Quaternion Quaternion::euler(float pitch, float yaw, float roll, bool valueIsRadian) {
	pitch = valueIsRadian ? pitch : (pitch * M_PI / 180.0f);
	yaw = valueIsRadian ? yaw : (yaw   * M_PI / 180.0f);
	roll = valueIsRadian ? roll : (roll  * M_PI / 180.0f);
	Quaternion q;

	float cy = cos(roll * 0.5f);
	float sy = sin(roll * 0.5f);
	float cr = cos(pitch * 0.5f);
	float sr = sin(pitch * 0.5f);
	float cp = cos(yaw * 0.5f);
	float sp = sin(yaw * 0.5f);

	q.w = cy * cr * cp + sy * sr * sp;
	q.x = cy * sr * cp - sy * cr * sp;
	q.y = cy * cr * sp + sy * sr * cp;
	q.z = sy * cr * cp - cy * sr * sp;
	return q;
}

Vector3 Quaternion::rotVector(const Quaternion & q, const Vector3 & v) {

	// nVidia SDK implementation
	Vector3 uv, uuv;
	Vector3 qvec(q.x, q.y, q.z);
	//uv = qvec.crossProduct(v);
	uv = Vector3::cross(qvec, v);
	//uuv = qvec.crossProduct(uv);
	uuv = Vector3::cross(qvec, uv);
	uv *= (2.0f * q.w);
	uuv *= 2.0f;

	return v + uv + uuv;

	const Vector3 u(q.x, q.y, q.z);
	float s = q.w;

	Vector3 result = 2.0f *Vector3::dot(u, v) * u
		+ (s*s - Vector3::dot(u, u)) * v
		+ 2.0f * s * Vector3::cross(u, v);

	return result;
}

Quaternion Quaternion::lookRotation(const Vector3 & direction, const Vector3 & up) {
	Vector3 z = direction.normalize();
	Vector3 x = Vector3::cross(up, z).normalize();
	Vector3 y = Vector3::cross(z, x);
	auto rotMat = Matrix4(
		x.x, y.x, z.x, 0.0f,
		x.y, y.y, z.y, 0.0f,
		x.z, y.z, z.z, 0.0f,
		0, 0, 0, 1
	);
	return rotMat.transpose().rotation();
}

float Quaternion::dot(const Quaternion & q1, const Quaternion & q2) {
	return q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
}

Quaternion Quaternion::inverse() const {

	Quaternion result;

	const float lengthSq = x * x + y * y + z * z + w * w;

	result.x = -x / lengthSq;
	result.y = -y / lengthSq;
	result.z = -z / lengthSq;
	result.w = w / lengthSq;

	return result;
}

Quaternion Quaternion::conjugate() const {
	return { -x, -y, -z, w };
}

Vector3 Quaternion::toEulerAngle() const {

	Vector3 result;
	float test = x * y + z * w;
	if (test > 0.499) { // singularity at north pole
		result.y = 2 * atan2(x, w);
		result.x = M_PI / 2;
		result.z = 0;
		return result;
	}
	if (test < -0.499) { // singularity at south pole
		result.y = -2 * atan2(x, w);
		result.x = -M_PI / 2;
		result.z = 0;
		return result;
	}
	float sqx = x * x;
	float sqy = y * y;
	float sqz = z * z;
	result.y = atan2f(2 * y*w - 2 * x*z, 1 - 2 * sqy - 2 * sqz);
	result.x = asinf(2 * test);
	result.z = atan2f(2 * x*w - 2 * y*z, 1 - 2 * sqx - 2 * sqz);

	return result;

	//Vector3 result;
	//// roll (x-axis rotation)
	//const float sinr = +2.0f * (w * x + y * z);
	//const float cosr = +1.0f - 2.0f * (x * x + y * y);
	//result.x = atan2(sinr, cosr);

	//// pitch (y-axis rotation)
	//float sinp = +2.0f * (w * y - z * x);
	//if (fabs(sinp) >= 1.0f) {
	//	result.y = copysign(M_PI / 2.0f, sinp); // use 90 degrees if out of range
	//}
	//else {
	//	result.y = asin(sinp);
	//}

	//// yaw (z-axis rotation)
	//float siny = +2.0f * (w * z + x * y);
	//float cosy = +1.0f - 2.0f * (y * y + z * z);
	//result.z = atan2(siny, cosy);

	//return result;
}

float Quaternion::getRoll(bool reprojectAxis) const {
	if (reprojectAxis) {
		// roll = atan2(localx.y, localx.x)
		// pick parts of xAxis() implementation that we need
		//			Real fTx  = 2.0*x;
		float fTy = 2.0f*y;
		float fTz = 2.0f*z;
		float fTwz = fTz * w;
		float fTxy = fTy * x;
		float fTyy = fTy * y;
		float fTzz = fTz * z;

		// Vector3(1.0-(fTyy+fTzz), fTxy+fTwz, fTxz-fTwy);

		return atan2(fTxy + fTwz, 1.0f - (fTyy + fTzz));

	} else {
		return atan2(2 * (x*y + w * z), w*w + x * x - y * y - z * z);
	}
}
//-----------------------------------------------------------------------
float Quaternion::getPitch(bool reprojectAxis) const {
	if (reprojectAxis) {
		// pitch = atan2(localy.z, localy.y)
		// pick parts of yAxis() implementation that we need
		float fTx = 2.0f*x;
		//			Real fTy  = 2.0f*y;
		float fTz = 2.0f*z;
		float fTwx = fTx * w;
		float fTxx = fTx * x;
		float fTyz = fTz * y;
		float fTzz = fTz * z;

		// Vector3(fTxy-fTwz, 1.0-(fTxx+fTzz), fTyz+fTwx);
		return atan2(fTyz + fTwx, 1.0f - (fTxx + fTzz));
	} else {
		// internal version
		return atan2(2 * (y*z + w * x), w*w - x * x - y * y + z * z);
	}
}
//-----------------------------------------------------------------------
float Quaternion::getYaw(bool reprojectAxis) const {
	if (reprojectAxis) {
		// yaw = atan2(localz.x, localz.z)
		// pick parts of zAxis() implementation that we need
		float fTx = 2.0f*x;
		float fTy = 2.0f*y;
		float fTz = 2.0f*z;
		float fTwy = fTy * w;
		float fTxx = fTx * x;
		float fTxz = fTz * x;
		float fTyy = fTy * y;

		// Vector3(fTxz+fTwy, fTyz-fTwx, 1.0-(fTxx+fTyy));

		return atan2(fTxz + fTwy, 1.0f - (fTxx + fTyy));

	} else {
		// internal version
		return asin(-2 * (x*z - w * y));
	}
}

Quaternion operator-(const Quaternion & q) {
	Quaternion result = { -q.x, -q.y, -q.z, -q.w };
	return result;
}

bool operator==(const Quaternion & q1, const Quaternion & q2) {
	return q1.x == q2.x
		&& q1.y == q2.y
		&& q1.z == q2.z
		&& q1.w == q2.w;
}

Quaternion & operator+=(Quaternion & q1, const Quaternion & q2) {
	q1.x += q2.x;
	q1.y += q2.y;
	q1.z += q2.z;
	q1.w += q2.w;
	return q1;
}

Quaternion & operator-=(Quaternion & q1, const Quaternion & q2) {
	q1.x -= q2.x;
	q1.y -= q2.y;
	q1.z -= q2.z;
	q1.w -= q2.w;
	return q1;
}

Quaternion & operator*=(Quaternion & q1, const Quaternion & q2) {
	Quaternion result = {
		q1.x * q2.w + q1.y * q2.z - q1.z * q2.y + q1.w * q2.x,
		-q1.x * q2.z + q1.y * q2.w + q1.z * q2.x + q1.w * q2.y,
		q1.x * q2.y - q1.y * q2.x + q1.z * q2.w + q1.w * q2.z,
		-q1.x * q2.x - q1.y * q2.y - q1.z * q2.z + q1.w * q2.w
	};

	q1 = result;
	return q1;
}

Quaternion & operator*=(Quaternion & q1, float s) {
	q1.x *= s;
	q1.y *= s;
	q1.z *= s;
	q1.w *= s;
	return q1;
}

Quaternion & operator/=(Quaternion & q, float s) {
	return q *= 1.0f / s;
}

Quaternion operator+(const Quaternion & q1, const Quaternion & q2) {
	Quaternion result = q1;
	return result += q2;
}

Quaternion operator-(const Quaternion & q1, const Quaternion & q2) {
	Quaternion result = q1;
	return result -= q2;
}

Quaternion operator*(const Quaternion & q1, const Quaternion & q2) {
	Quaternion result = q1;
	return result *= q2;
}

Quaternion operator*(const Quaternion & q, float s) {
	Quaternion result = q;
	return result *= s;
}

Quaternion operator*(float s, const Quaternion & q) {
	Quaternion result = q;
	return result *= s;
}

Quaternion operator/(const Quaternion & q, float s) {
	Quaternion result = q;
	return result /= s;
}
