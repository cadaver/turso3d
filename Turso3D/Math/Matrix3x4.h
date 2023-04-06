// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Matrix4.h"

#include <string>

/// 3x4 matrix for scene node transform calculations.
class Matrix3x4
{
public:
    /// Matrix values.
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    
    /// Construct undefined.
    Matrix3x4()
    {
    }
    
    /// Copy-construct.
    Matrix3x4(const Matrix3x4& matrix) :
        m00(matrix.m00), m01(matrix.m01), m02(matrix.m02), m03(matrix.m03),
        m10(matrix.m10), m11(matrix.m11), m12(matrix.m12), m13(matrix.m13),
        m20(matrix.m20), m21(matrix.m21), m22(matrix.m22), m23(matrix.m23)
    {
    }
    
    /// Copy-construct from a 3x3 matrix and set the extra elements to identity.
    Matrix3x4(const Matrix3& matrix) :
        m00(matrix.m00), m01(matrix.m01), m02(matrix.m02), m03(0.0f),
        m10(matrix.m10), m11(matrix.m11), m12(matrix.m12), m13(0.0f),
        m20(matrix.m20), m21(matrix.m21), m22(matrix.m22), m23(0.0f)
    {
    }
    
    /// Copy-construct from a 4x4 matrix which is assumed to contain no projection.
    Matrix3x4(const Matrix4& matrix) :
        m00(matrix.m00), m01(matrix.m01), m02(matrix.m02), m03(matrix.m03),
        m10(matrix.m10), m11(matrix.m11), m12(matrix.m12), m13(matrix.m13),
        m20(matrix.m20), m21(matrix.m21), m22(matrix.m22), m23(matrix.m23)
    {
    }
    
    // Construct from values.
    Matrix3x4(float v00, float v01, float v02, float v03,
            float v10, float v11, float v12, float v13,
            float v20, float v21, float v22, float v23) :
        m00(v00), m01(v01), m02(v02), m03(v03),
        m10(v10), m11(v11), m12(v12), m13(v13),
        m20(v20), m21(v21), m22(v22), m23(v23)
    {
    }
    
    /// Construct from a float array.
    Matrix3x4(const float* data) :
        m00(data[0]), m01(data[1]), m02(data[2]), m03(data[3]),
        m10(data[4]), m11(data[5]), m12(data[6]), m13(data[7]),
        m20(data[8]), m21(data[9]), m22(data[10]), m23(data[11])
    {
    }
    
    /// Construct from translation, rotation and uniform scale.
    Matrix3x4(const Vector3& translation, const Quaternion& rotation, float scale)
    {
        SetTransform(translation, rotation, scale);
    }

    /// Construct from translation, rotation and nonuniform scale.
    Matrix3x4(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
    {
        SetTransform(translation, rotation, scale);
    }

    /// Construct by parsing a string.
    Matrix3x4(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    Matrix3x4(const char* str)
    {
        FromString(str);
    }
    
    /// Assign from another matrix.
    Matrix3x4& operator = (const Matrix3x4& rhs)
    {
        m00 = rhs.m00; m01 = rhs.m01; m02 = rhs.m02; m03 = rhs.m03;
        m10 = rhs.m10; m11 = rhs.m11; m12 = rhs.m12; m13 = rhs.m13;
        m20 = rhs.m20; m21 = rhs.m21; m22 = rhs.m22; m23 = rhs.m23;
        return *this;
    }
    
    /// Assign from a 3x3 matrix and set the extra elements to identity.
    Matrix3x4& operator = (const Matrix3& rhs)
    {
        m00 = rhs.m00; m01 = rhs.m01; m02 = rhs.m02; m03 = 0.0;
        m10 = rhs.m10; m11 = rhs.m11; m12 = rhs.m12; m13 = 0.0;
        m20 = rhs.m20; m21 = rhs.m21; m22 = rhs.m22; m23 = 0.0;
        return *this;
    }
    
    /// Assign from a 4x4 matrix which is assumed to contain no projection.
    Matrix3x4& operator = (const Matrix4& rhs)
    {
        m00 = rhs.m00; m01 = rhs.m01; m02 = rhs.m02; m03 = rhs.m03;
        m10 = rhs.m10; m11 = rhs.m11; m12 = rhs.m12; m13 = rhs.m13;
        m20 = rhs.m20; m21 = rhs.m21; m22 = rhs.m22; m23 = rhs.m23;
        return *this;
    }
    
    /// Test for equality with another matrix without epsilon.
    bool operator == (const Matrix3x4& rhs) const
    {
        const float* leftData = Data();
        const float* rightData = rhs.Data();
        
        for (size_t i = 0; i < 12; ++i)
        {
            if (leftData[i] != rightData[i])
                return false;
        }
        
        return true;
    }
    
    /// Test for inequality with another matrix without epsilon.
    bool operator != (const Matrix3x4& rhs) const { return !(*this == rhs); }
    
    /// Multiply a Vector3 which is assumed to represent position.
    Vector3 operator * (const Vector3& rhs) const
    {
        return Vector3(
            (m00 * rhs.x + m01 * rhs.y + m02 * rhs.z + m03),
            (m10 * rhs.x + m11 * rhs.y + m12 * rhs.z + m13),
            (m20 * rhs.x + m21 * rhs.y + m22 * rhs.z + m23)
        );
    }
    
    /// Multiply a Vector4.
    Vector3 operator * (const Vector4& rhs) const
    {
        return Vector3(
            (m00 * rhs.x + m01 * rhs.y + m02 * rhs.z + m03 * rhs.w),
            (m10 * rhs.x + m11 * rhs.y + m12 * rhs.z + m13 * rhs.w),
            (m20 * rhs.x + m21 * rhs.y + m22 * rhs.z + m23 * rhs.w)
        );
    }
    
    /// Add a matrix.
    Matrix3x4 operator + (const Matrix3x4& rhs) const
    {
        return Matrix3x4(
            m00 + rhs.m00, m01 + rhs.m01, m02 + rhs.m02, m03 + rhs.m03,
            m10 + rhs.m10, m11 + rhs.m11, m12 + rhs.m12, m13 + rhs.m13,
            m20 + rhs.m20, m21 + rhs.m21, m22 + rhs.m22, m23 + rhs.m23
        );
    }
    
    /// Subtract a matrix.
    Matrix3x4 operator - (const Matrix3x4& rhs) const
    {
        return Matrix3x4(
            m00 - rhs.m00, m01 - rhs.m01, m02 - rhs.m02, m03 - rhs.m03,
            m10 - rhs.m10, m11 - rhs.m11, m12 - rhs.m12, m13 - rhs.m13,
            m20 - rhs.m20, m21 - rhs.m21, m22 - rhs.m22, m23 - rhs.m23
        );
    }
    
    /// Multiply with a scalar.
    Matrix3x4 operator * (float rhs) const
    {
        return Matrix3x4(
            m00 * rhs, m01 * rhs, m02 * rhs, m03 * rhs,
            m10 * rhs, m11 * rhs, m12 * rhs, m13 * rhs,
            m20 * rhs, m21 * rhs, m22 * rhs, m23 * rhs
        );
    }
    
    /// Multiply a matrix.
    Matrix3x4 operator * (const Matrix3x4& rhs) const
    {
        return Matrix3x4(
            m00 * rhs.m00 + m01 * rhs.m10 + m02 * rhs.m20,
            m00 * rhs.m01 + m01 * rhs.m11 + m02 * rhs.m21,
            m00 * rhs.m02 + m01 * rhs.m12 + m02 * rhs.m22,
            m00 * rhs.m03 + m01 * rhs.m13 + m02 * rhs.m23 + m03,
            m10 * rhs.m00 + m11 * rhs.m10 + m12 * rhs.m20,
            m10 * rhs.m01 + m11 * rhs.m11 + m12 * rhs.m21,
            m10 * rhs.m02 + m11 * rhs.m12 + m12 * rhs.m22,
            m10 * rhs.m03 + m11 * rhs.m13 + m12 * rhs.m23 + m13,
            m20 * rhs.m00 + m21 * rhs.m10 + m22 * rhs.m20,
            m20 * rhs.m01 + m21 * rhs.m11 + m22 * rhs.m21,
            m20 * rhs.m02 + m21 * rhs.m12 + m22 * rhs.m22,
            m20 * rhs.m03 + m21 * rhs.m13 + m22 * rhs.m23 + m23
        );
    }
    
    /// Multiply a 4x4 matrix.
    Matrix4 operator * (const Matrix4& rhs) const
    {
        return Matrix4(
            m00 * rhs.m00 + m01 * rhs.m10 + m02 * rhs.m20 + m03 * rhs.m30,
            m00 * rhs.m01 + m01 * rhs.m11 + m02 * rhs.m21 + m03 * rhs.m31,
            m00 * rhs.m02 + m01 * rhs.m12 + m02 * rhs.m22 + m03 * rhs.m32,
            m00 * rhs.m03 + m01 * rhs.m13 + m02 * rhs.m23 + m03 * rhs.m33,
            m10 * rhs.m00 + m11 * rhs.m10 + m12 * rhs.m20 + m13 * rhs.m30,
            m10 * rhs.m01 + m11 * rhs.m11 + m12 * rhs.m21 + m13 * rhs.m31,
            m10 * rhs.m02 + m11 * rhs.m12 + m12 * rhs.m22 + m13 * rhs.m32,
            m10 * rhs.m03 + m11 * rhs.m13 + m12 * rhs.m23 + m13 * rhs.m33,
            m20 * rhs.m00 + m21 * rhs.m10 + m22 * rhs.m20 + m23 * rhs.m30,
            m20 * rhs.m01 + m21 * rhs.m11 + m22 * rhs.m21 + m23 * rhs.m31,
            m20 * rhs.m02 + m21 * rhs.m12 + m22 * rhs.m22 + m23 * rhs.m32,
            m20 * rhs.m03 + m21 * rhs.m13 + m22 * rhs.m23 + m23 * rhs.m33,
            rhs.m30,
            rhs.m31,
            rhs.m32,
            rhs.m33
        );
    }
    
    /// Set translation elements.
    void SetTranslation(const Vector3& translation)
    {
        m03 = translation.x;
        m13 = translation.y;
        m23 = translation.z;
    }
    
    /// Set rotation elements from a 3x3 matrix.
    void SetRotation(const Matrix3& rotation)
    {
        m00 = rotation.m00; m01 = rotation.m01; m02 = rotation.m02;
        m10 = rotation.m10; m11 = rotation.m11; m12 = rotation.m12;
        m20 = rotation.m20; m21 = rotation.m21; m22 = rotation.m22;
    }

    /// Set full transform from a translation vector, rotation quaternion and uniform scale
    void SetTransform(const Vector3& translation, const Quaternion& rotation, float scale)
    {
        m00 = (1.0f - 2.0f * rotation.y * rotation.y - 2.0f * rotation.z * rotation.z) * scale;
        m01 = (2.0f * rotation.x * rotation.y - 2.0f * rotation.w * rotation.z) * scale;
        m02 = (2.0f * rotation.x * rotation.z + 2.0f * rotation.w * rotation.y) * scale;
        m03 = translation.x;
        m10 = (2.0f * rotation.x * rotation.y + 2.0f * rotation.w * rotation.z) * scale;
        m11 = (1.0f - 2.0f * rotation.x * rotation.x - 2.0f * rotation.z * rotation.z) * scale;
        m12 = (2.0f * rotation.y * rotation.z - 2.0f * rotation.w * rotation.x) * scale;
        m13 = translation.y;
        m20 = (2.0f * rotation.x * rotation.z - 2.0f * rotation.w * rotation.y) * scale;
        m21 = (2.0f * rotation.y * rotation.z + 2.0f * rotation.w * rotation.x) * scale;
        m22 = (1.0f - 2.0f * rotation.x * rotation.x - 2.0f * rotation.y * rotation.y) * scale;
        m23 = translation.z;
    }

    /// Set full transform from a translation vector, rotation quaternion and scale vector.
    void SetTransform(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
    {
        m00 = (1.0f - 2.0f * rotation.y * rotation.y - 2.0f * rotation.z * rotation.z) * scale.x;
        m01 = (2.0f * rotation.x * rotation.y - 2.0f * rotation.w * rotation.z) * scale.y;
        m02 = (2.0f * rotation.x * rotation.z + 2.0f * rotation.w * rotation.y) * scale.z;
        m03 = translation.x;
        m10 = (2.0f * rotation.x * rotation.y + 2.0f * rotation.w * rotation.z) * scale.x;
        m11 = (1.0f - 2.0f * rotation.x * rotation.x - 2.0f * rotation.z * rotation.z) * scale.y;
        m12 = (2.0f * rotation.y * rotation.z - 2.0f * rotation.w * rotation.x) * scale.z;
        m13 = translation.y;
        m20 = (2.0f * rotation.x * rotation.z - 2.0f * rotation.w * rotation.y) * scale.x;
        m21 = (2.0f * rotation.y * rotation.z + 2.0f * rotation.w * rotation.x) * scale.y;
        m22 = (1.0f - 2.0f * rotation.x * rotation.x - 2.0f * rotation.y * rotation.y) * scale.z;
        m23 = translation.z;
    }
    
    /// Set scaling elements.
    void SetScale(const Vector3& scale)
    {
        m00 = scale.x;
        m11 = scale.y;
        m22 = scale.z;
    }
    
    /// Set uniform scaling elements.
    void SetScale(float scale)
    {
        m00 = scale;
        m11 = scale;
        m22 = scale;
    }
    
    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    
    /// Return the combined rotation and scaling matrix.
    Matrix3 ToMatrix3() const
    {
        return Matrix3(
            m00, m01, m02,
            m10, m11, m12,
            m20, m21, m22
        );
    }
    
    /// Convert to a 4x4 matrix by filling in an identity last row.
    Matrix4 ToMatrix4() const
    {
        return Matrix4(
            m00, m01, m02, m03,
            m10, m11, m12, m13,
            m20, m21, m22, m23,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }
    
    /// Return the rotation matrix with scaling removed.
    Matrix3 RotationMatrix() const
    {
        Vector3 invScale(
            1.0f / sqrtf(m00 * m00 + m10 * m10 + m20 * m20),
            1.0f / sqrtf(m01 * m01 + m11 * m11 + m21 * m21),
            1.0f / sqrtf(m02 * m02 + m12 * m12 + m22 * m22)
        );
        
        return ToMatrix3().Scaled(invScale);
    }
    
    /// Return the translation part.
    Vector3 Translation() const
    {
        return Vector3(
            m03,
            m13,
            m23
        );
    }
    
    /// Return the rotation part.
    Quaternion Rotation() const { return Quaternion(RotationMatrix()); }
    
    /// Return the scaling part.
    Vector3 Scale() const
    {
        return Vector3(
            sqrtf(m00 * m00 + m10 * m10 + m20 * m20),
            sqrtf(m01 * m01 + m11 * m11 + m21 * m21),
            sqrtf(m02 * m02 + m12 * m12 + m22 * m22)
        );
    }
    
    /// Test for equality with another matrix with epsilon.
    bool Equals(const Matrix3x4& rhs, float epsilon = M_EPSILON) const
    {
        const float* leftData = Data();
        const float* rightData = rhs.Data();
        
        for (size_t i = 0; i < 12; ++i)
        {
            if (!::Equals(leftData[i], rightData[i], epsilon))
                return false;
        }
        
        return true;
    }
    
    /// Return decomposition to translation, rotation and scale.
    void Decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const
    {
        translation.x = m03;
        translation.y = m13;
        translation.z = m23;

        scale.x = sqrtf(m00 * m00 + m10 * m10 + m20 * m20);
        scale.y = sqrtf(m01 * m01 + m11 * m11 + m21 * m21);
        scale.z = sqrtf(m02 * m02 + m12 * m12 + m22 * m22);

        Vector3 invScale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
        rotation = Quaternion(ToMatrix3().Scaled(invScale));
    }

    
    /// Return inverse.
    Matrix3x4 Inverse() const
    {
        float det = m00 * m11 * m22 +
            m10 * m21 * m02 +
            m20 * m01 * m12 -
            m20 * m11 * m02 -
            m10 * m01 * m22 -
            m00 * m21 * m12;

        float invDet = 1.0f / det;
        Matrix3x4 ret;

        ret.m00 = (m11 * m22 - m21 * m12) * invDet;
        ret.m01 = -(m01 * m22 - m21 * m02) * invDet;
        ret.m02 = (m01 * m12 - m11 * m02) * invDet;
        ret.m03 = -(m03 * ret.m00 + m13 * ret.m01 + m23 * ret.m02);
        ret.m10 = -(m10 * m22 - m20 * m12) * invDet;
        ret.m11 = (m00 * m22 - m20 * m02) * invDet;
        ret.m12 = -(m00 * m12 - m10 * m02) * invDet;
        ret.m13 = -(m03 * ret.m10 + m13 * ret.m11 + m23 * ret.m12);
        ret.m20 = (m10 * m21 - m20 * m11) * invDet;
        ret.m21 = -(m00 * m21 - m20 * m01) * invDet;
        ret.m22 = (m00 * m11 - m10 * m01) * invDet;
        ret.m23 = -(m03 * ret.m20 + m13 * ret.m21 + m23 * ret.m22);

        return ret;
    }
    
    /// Return float data.
    const float* Data() const { return &m00; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero matrix.
    static const Matrix3x4 ZERO;
    /// Identity matrix.
    static const Matrix3x4 IDENTITY;
};

/// Multiply a 3x4 matrix with a scalar.
inline Matrix3x4 operator * (float lhs, const Matrix3x4& rhs) { return rhs * lhs; }

/// Multiply a 3x4 matrix with a 4x4 matrix.
inline Matrix4 operator * (const Matrix4& lhs, const Matrix3x4& rhs)
{
    return Matrix4(
        lhs.m00 * rhs.m00 + lhs.m01 * rhs.m10 + lhs.m02 * rhs.m20,
        lhs.m00 * rhs.m01 + lhs.m01 * rhs.m11 + lhs.m02 * rhs.m21,
        lhs.m00 * rhs.m02 + lhs.m01 * rhs.m12 + lhs.m02 * rhs.m22,
        lhs.m00 * rhs.m03 + lhs.m01 * rhs.m13 + lhs.m02 * rhs.m23 + lhs.m03,
        lhs.m10 * rhs.m00 + lhs.m11 * rhs.m10 + lhs.m12 * rhs.m20,
        lhs.m10 * rhs.m01 + lhs.m11 * rhs.m11 + lhs.m12 * rhs.m21,
        lhs.m10 * rhs.m02 + lhs.m11 * rhs.m12 + lhs.m12 * rhs.m22,
        lhs.m10 * rhs.m03 + lhs.m11 * rhs.m13 + lhs.m12 * rhs.m23 + lhs.m13,
        lhs.m20 * rhs.m00 + lhs.m21 * rhs.m10 + lhs.m22 * rhs.m20,
        lhs.m20 * rhs.m01 + lhs.m21 * rhs.m11 + lhs.m22 * rhs.m21,
        lhs.m20 * rhs.m02 + lhs.m21 * rhs.m12 + lhs.m22 * rhs.m22,
        lhs.m20 * rhs.m03 + lhs.m21 * rhs.m13 + lhs.m22 * rhs.m23 + lhs.m23,
        lhs.m30 * rhs.m00 + lhs.m31 * rhs.m10 + lhs.m32 * rhs.m20,
        lhs.m30 * rhs.m01 + lhs.m31 * rhs.m11 + lhs.m32 * rhs.m21,
        lhs.m30 * rhs.m02 + lhs.m31 * rhs.m12 + lhs.m32 * rhs.m22,
        lhs.m30 * rhs.m03 + lhs.m31 * rhs.m13 + lhs.m32 * rhs.m23 + lhs.m33
    );
}
