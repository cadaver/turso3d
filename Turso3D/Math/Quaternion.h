// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Matrix3.h"

namespace Turso3D
{

/// Rotation represented as a four-dimensional normalized vector.
class TURSO3D_API Quaternion
{
public:
    /// Construct an identity quaternion.
    Quaternion() :
        w(1.0f),
        x(0.0f),
        y(0.0f),
        z(0.0f)
    {
    }
    
    /// Copy-construct.
    Quaternion(const Quaternion& quat) :
        w(quat.w),
        x(quat.x),
        y(quat.y),
        z(quat.z)
    {
    }
    
    /// Construct from values.
    Quaternion(float w_, float x_, float y_, float z_) :
        w(w_),
        x(x_),
        y(y_),
        z(z_)
    {
    }
    
    /// Construct from a float array.
    Quaternion(const float* data) :
        w(data[0]),
        x(data[1]),
        y(data[2]),
        z(data[3])
    {
    }
    
    /// Construct from an angle (in degrees) and axis.
    Quaternion(float angle, const Vector3& axis)
    {
        FromAngleAxis(angle, axis);
    }

    /// Construct from a rotation angle (in degrees) about the Z axis.
    Quaternion(float angle)
    {
        FromAngleAxis(angle, Vector3::FORWARD);
    }
    
    /// Construct from Euler angles (in degrees.)
    Quaternion(float x, float y, float z)
    {
        FromEulerAngles(x, y, z);
    }
    
    /// Construct from the rotation difference between two direction vectors.
    Quaternion(const Vector3& start, const Vector3& end)
    {
        FromRotationTo(start, end);
    }
    
    /// Construct from orthonormal axes.
    Quaternion(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
    {
        FromAxes(xAxis, yAxis, zAxis);
    }

    /// Construct from a rotation matrix.
    Quaternion(const Matrix3& matrix)
    {
        FromRotationMatrix(matrix);
    }
    
    /// Construct by parsing a string.
    Quaternion(const String& str)
    {
        FromString(str);
    }
    
    /// Construct by parsing a C string.
    Quaternion(const char* str)
    {
        FromString(str);
    }
    
    /// Assign from another quaternion.
    Quaternion& operator = (const Quaternion& rhs)
    {
        w = rhs.w;
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        return *this;
    }
    
    /// Add-assign a quaternion.
    Quaternion& operator += (const Quaternion& rhs)
    {
        w += rhs.w;
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
  
    /// Multiply-assign a scalar.
    Quaternion& operator *= (float rhs)
    {
        w *= rhs;
        x *= rhs;
        y *= rhs;
        z *= rhs;
        return *this;
    }
    
    /// Test for equality with another quaternion without epsilon.
    bool operator == (const Quaternion& rhs) const { return w == rhs.w && x == rhs.x && y == rhs.y && z == rhs.z; }
    /// Test for inequality with another quaternion without epsilon.
    bool operator != (const Quaternion& rhs) const { return !(*this == rhs); }
    /// Multiply with a scalar.
    Quaternion operator * (float rhs) const { return Quaternion(w * rhs, x * rhs, y * rhs, z * rhs); }
    /// Return negation.
    Quaternion operator - () const { return Quaternion(-w, -x, -y, -z); }
    /// Add a quaternion.
    Quaternion operator + (const Quaternion& rhs) const { return Quaternion(w + rhs.w, x + rhs.x, y + rhs.y, z + rhs.z); }
    /// Subtract a quaternion.
    Quaternion operator - (const Quaternion& rhs) const { return Quaternion(w - rhs.w, x - rhs.x, y - rhs.y, z - rhs.z); }
    
    /// Multiply a quaternion.
    Quaternion operator * (const Quaternion& rhs) const
    {
        return Quaternion(
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y + y * rhs.w + z * rhs.x - x * rhs.z,
            w * rhs.z + z * rhs.w + x * rhs.y - y * rhs.x
        );
    }
    
    /// Multiply a Vector3.
    Vector3 operator * (const Vector3& rhs) const
    {
        Vector3 qVec(x, y, z);
        Vector3 cross1(qVec.CrossProduct(rhs));
        Vector3 cross2(qVec.CrossProduct(cross1));
        
        return rhs + 2.0f * (cross1 * w + cross2);
    }
    
    /// Define from an angle (in degrees) and axis.
    void FromAngleAxis(float angle, const Vector3& axis);
    /// Define from Euler angles (in degrees.)
    void FromEulerAngles(float x, float y, float z);
    /// Define from the rotation difference between two direction vectors.
    void FromRotationTo(const Vector3& start, const Vector3& end);
    /// Define from orthonormal axes.
    void FromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis);
    /// Define from a rotation matrix.
    void FromRotationMatrix(const Matrix3& matrix);
    /// Define from a direction to look in and an up direction. Return true on success, or false if would result in a NaN, in which case the current value remains.
    bool FromLookRotation(const Vector3& direction, const Vector3& up = Vector3::UP);
    /// Parse from a string. Return true on success.
    bool FromString(const String& str);
    /// Parse from a C string. Return true on success.
    bool FromString(const char* str);

    /// Normalize to unit length.
    void Normalize()
    {
        float lenSquared = LengthSquared();
        if (!Turso3D::Equals(lenSquared, 1.0f) && lenSquared > 0.0f)
        {
            float invLen = 1.0f / sqrtf(lenSquared);
            w *= invLen;
            x *= invLen;
            y *= invLen;
            z *= invLen;
        }
    }
    
    /// Return normalized to unit length.
    Quaternion Normalized() const
    {
        float lenSquared = LengthSquared();
        if (!Turso3D::Equals(lenSquared, 1.0f) && lenSquared > 0.0f)
        {
            float invLen = 1.0f / sqrtf(lenSquared);
            return *this * invLen;
        }
        else
            return *this;
    }
    
    /// Return inverse.
    Quaternion Inverse() const
    {
        float lenSquared = LengthSquared();
        if (lenSquared == 1.0f)
            return Conjugate();
        else if (lenSquared >= M_EPSILON)
            return Conjugate() * (1.0f / lenSquared);
        else
            return IDENTITY;
    }
    
    /// Return squared length.
    float LengthSquared() const { return w * w + x * x + y * y + z * z; }
    /// Calculate dot product.
    float DotProduct(const Quaternion& rhs) const { return w * rhs.w + x * rhs.x + y * rhs.y + z * rhs.z; }
    /// Test for equality with another quaternion with epsilon.
    bool Equals(const Quaternion& rhs) const { return Turso3D::Equals(w, rhs.w) && Turso3D::Equals(x, rhs.x) && Turso3D::Equals(y, rhs.y) && Turso3D::Equals(z, rhs.z); }
    /// Return whether is NaN.
    bool IsNaN() const { return Turso3D::IsNaN(w) || Turso3D::IsNaN(x) || Turso3D::IsNaN(y) || Turso3D::IsNaN(z); }
    /// Return conjugate.
    Quaternion Conjugate() const { return Quaternion(w, -x, -y, -z); }
    
    /// Return Euler angles in degrees.
    Vector3 EulerAngles() const;
    /// Return yaw angle in degrees.
    float YawAngle() const;
    /// Return pitch angle in degrees.
    float PitchAngle() const;
    /// Return roll angle in degrees.
    float RollAngle() const;
    /// Return the rotation matrix that corresponds to this quaternion.
    Matrix3 RotationMatrix() const;
    /// Spherical interpolation with another quaternion.
    Quaternion Slerp(Quaternion rhs, float t) const;
    /// Normalized linear interpolation with another quaternion.
    Quaternion Nlerp(Quaternion rhs, float t, bool shortestPath = false) const;
    /// Return float data.
    const float* Data() const { return &w; }
    /// Return as string.
    String ToString() const;
    
    /// W coordinate.
    float w;
    /// X coordinate.
    float x;
    /// Y coordinate.
    float y;
    /// Z coordinate.
    float z;
    
    /// Identity quaternion.
    static const Quaternion IDENTITY;
};

}
