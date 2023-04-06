// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Quaternion.h"
#include "Vector4.h"

#include <string>

class Matrix3x4;

/// 4x4 matrix for arbitrary linear transforms including projection.
class Matrix4
{
public:
    /// Matrix values.
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    float m30, m31, m32, m33;
    
    /// Construct undefined.
    Matrix4()
    {
    }
    
    /// Copy-construct.
    Matrix4(const Matrix4& matrix) :
        m00(matrix.m00), m01(matrix.m01), m02(matrix.m02), m03(matrix.m03),
        m10(matrix.m10), m11(matrix.m11), m12(matrix.m12), m13(matrix.m13),
        m20(matrix.m20), m21(matrix.m21), m22(matrix.m22), m23(matrix.m23),
        m30(matrix.m30), m31(matrix.m31), m32(matrix.m32), m33(matrix.m33)
    {
    }
    
    /// Copy-construct from a 3x3 matrix and set the extra elements to identity.
    Matrix4(const Matrix3& matrix) :
        m00(matrix.m00), m01(matrix.m01), m02(matrix.m02), m03(0.0f),
        m10(matrix.m10), m11(matrix.m11), m12(matrix.m12), m13(0.0f),
        m20(matrix.m20), m21(matrix.m21), m22(matrix.m22), m23(0.0f),
        m30(0.0f), m31(0.0f), m32(0.0f), m33(1.0f)
    {
    }

    /// Copy-construct from a 3x4 matrix and set the extra elements to identity.
    Matrix4(const Matrix3x4& matrix);
    
    // Construct from values.
    Matrix4(float v00, float v01, float v02, float v03,
            float v10, float v11, float v12, float v13,
            float v20, float v21, float v22, float v23,
            float v30, float v31, float v32, float v33) :
        m00(v00), m01(v01), m02(v02), m03(v03),
        m10(v10), m11(v11), m12(v12), m13(v13),
        m20(v20), m21(v21), m22(v22), m23(v23),
        m30(v30), m31(v31), m32(v32), m33(v33)
    {
    }
    
    /// Construct from a float array.
    Matrix4(const float* data) :
        m00(data[0]), m01(data[1]), m02(data[2]), m03(data[3]),
        m10(data[4]), m11(data[5]), m12(data[6]), m13(data[7]),
        m20(data[8]), m21(data[9]), m22(data[10]), m23(data[11]),
        m30(data[12]), m31(data[13]), m32(data[14]), m33(data[15])
    {
    }
    
    /// Construct by parsing a string.
    Matrix4(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    Matrix4(const char* str)
    {
        FromString(str);
    }
    
    /// Assign from another matrix.
    Matrix4& operator = (const Matrix4& rhs)
    {
        m00 = rhs.m00; m01 = rhs.m01; m02 = rhs.m02; m03 = rhs.m03;
        m10 = rhs.m10; m11 = rhs.m11; m12 = rhs.m12; m13 = rhs.m13;
        m20 = rhs.m20; m21 = rhs.m21; m22 = rhs.m22; m23 = rhs.m23;
        m30 = rhs.m30; m31 = rhs.m31; m32 = rhs.m32; m33 = rhs.m33;
        return *this;
    }
    
    /// Assign from a 3x3 matrix. Set the extra elements to identity.
    Matrix4& operator = (const Matrix3& rhs)
    {
        m00 = rhs.m00; m01 = rhs.m01; m02 = rhs.m02; m03 = 0.0f;
        m10 = rhs.m10; m11 = rhs.m11; m12 = rhs.m12; m13 = 0.0f;
        m20 = rhs.m20; m21 = rhs.m21;  m22 = rhs.m22; m23 = 0.0f;
        m30 = 0.0f; m31 = 0.0f; m32 = 0.0f; m33 = 1.0f;
        return *this;
    }
    
    /// Test for equality with another matrix without epsilon.
    bool operator == (const Matrix4& rhs) const
    {
        const float* leftData = Data();
        const float* rightData = rhs.Data();
        
        for (size_t i = 0; i < 16; ++i)
        {
            if (leftData[i] != rightData[i])
                return false;
        }
        
        return true;
    }
    
    /// Test for inequality with another matrix without epsilon.
    bool operator != (const Matrix4& rhs) const { return !(*this == rhs); }
    
    /// Multiply a Vector3 which is assumed to represent position.
    Vector3 operator * (const Vector3& rhs) const
    {
        float invW = 1.0f / (m30 * rhs.x + m31 * rhs.y + m32 * rhs.z + m33);
        
        return Vector3(
            (m00 * rhs.x + m01 * rhs.y + m02 * rhs.z + m03) * invW,
            (m10 * rhs.x + m11 * rhs.y + m12 * rhs.z + m13) * invW,
            (m20 * rhs.x + m21 * rhs.y + m22 * rhs.z + m23) * invW
        );
    }
    
    /// Multiply a Vector4.
    Vector4 operator * (const Vector4& rhs) const
    {
        return Vector4(
            m00 * rhs.x + m01 * rhs.y + m02 * rhs.z + m03 * rhs.w,
            m10 * rhs.x + m11 * rhs.y + m12 * rhs.z + m13 * rhs.w,
            m20 * rhs.x + m21 * rhs.y + m22 * rhs.z + m23 * rhs.w,
            m30 * rhs.x + m31 * rhs.y + m32 * rhs.z + m33 * rhs.w
        );
    }
    
    /// Add a matrix.
    Matrix4 operator + (const Matrix4& rhs) const
    {
        return Matrix4(
            m00 + rhs.m00, m01 + rhs.m01, m02 + rhs.m02, m03 + rhs.m03,
            m10 + rhs.m10, m11 + rhs.m11, m12 + rhs.m12, m13 + rhs.m13,
            m20 + rhs.m20, m21 + rhs.m21, m22 + rhs.m22, m23 + rhs.m23,
            m30 + rhs.m30, m31 + rhs.m31, m32 + rhs.m32, m33 + rhs.m33
        );
    }
    
    /// Subtract a matrix.
    Matrix4 operator - (const Matrix4& rhs) const
    {
        return Matrix4(
            m00 - rhs.m00, m01 - rhs.m01, m02 - rhs.m02, m03 - rhs.m03,
            m10 - rhs.m10, m11 - rhs.m11, m12 - rhs.m12, m13 - rhs.m13,
            m20 - rhs.m20, m21 - rhs.m21, m22 - rhs.m22, m23 - rhs.m23,
            m30 - rhs.m30, m31 - rhs.m31, m32 - rhs.m32, m33 - rhs.m33
        );
    }
    
    /// Multiply with a scalar.
    Matrix4 operator * (float rhs) const
    {
        return Matrix4(
            m00 * rhs, m01 * rhs, m02 * rhs, m03 * rhs,
            m10 * rhs, m11 * rhs, m12 * rhs, m13 * rhs,
            m20 * rhs, m21 * rhs, m22 * rhs, m23 * rhs,
            m30 * rhs, m31 * rhs, m32 * rhs, m33 * rhs
        );
    }
    
    /// Multiply a matrix.
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
            m30 * rhs.m00 + m31 * rhs.m10 + m32 * rhs.m20 + m33 * rhs.m30,
            m30 * rhs.m01 + m31 * rhs.m11 + m32 * rhs.m21 + m33 * rhs.m31,
            m30 * rhs.m02 + m31 * rhs.m12 + m32 * rhs.m22 + m33 * rhs.m32,
            m30 * rhs.m03 + m31 * rhs.m13 + m32 * rhs.m23 + m33 * rhs.m33
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
    
    // Set scaling elements.
    void SetScale(const Vector3& scale)
    {
        m00 = scale.x;
        m11 = scale.y;
        m22 = scale.z;
    }
    
    // Set uniform scaling elements.
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
    
    /// Return transpose.
    Matrix4 Transpose() const
    {
        return Matrix4(
            m00, m10, m20, m30,
            m01, m11, m21, m31,
            m02, m12, m22, m32,
            m03, m13, m23, m33
        );
    }
    
    /// Test for equality with another matrix with epsilon.
    bool Equals(const Matrix4& rhs, float epsilon = M_EPSILON) const
    {
        const float* leftData = Data();
        const float* rightData = rhs.Data();
        
        for (size_t i = 0; i < 16; ++i)
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
    Matrix4 Inverse() const
    {
        float v0 = m20 * m31 - m21 * m30;
        float v1 = m20 * m32 - m22 * m30;
        float v2 = m20 * m33 - m23 * m30;
        float v3 = m21 * m32 - m22 * m31;
        float v4 = m21 * m33 - m23 * m31;
        float v5 = m22 * m33 - m23 * m32;

        float i00 = (v5 * m11 - v4 * m12 + v3 * m13);
        float i10 = -(v5 * m10 - v2 * m12 + v1 * m13);
        float i20 = (v4 * m10 - v2 * m11 + v0 * m13);
        float i30 = -(v3 * m10 - v1 * m11 + v0 * m12);

        float invDet = 1.0f / (i00 * m00 + i10 * m01 + i20 * m02 + i30 * m03);

        i00 *= invDet;
        i10 *= invDet;
        i20 *= invDet;
        i30 *= invDet;

        float i01 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
        float i11 = (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
        float i21 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
        float i31 = (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

        v0 = m10 * m31 - m11 * m30;
        v1 = m10 * m32 - m12 * m30;
        v2 = m10 * m33 - m13 * m30;
        v3 = m11 * m32 - m12 * m31;
        v4 = m11 * m33 - m13 * m31;
        v5 = m12 * m33 - m13 * m32;

        float i02 = (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
        float i12 = -(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
        float i22 = (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
        float i32 = -(v3 * m00 - v1 * m01 + v0 * m02) * invDet;

        v0 = m21 * m10 - m20 * m11;
        v1 = m22 * m10 - m20 * m12;
        v2 = m23 * m10 - m20 * m13;
        v3 = m22 * m11 - m21 * m12;
        v4 = m23 * m11 - m21 * m13;
        v5 = m23 * m12 - m22 * m13;

        float i03 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
        float i13 = (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
        float i23 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
        float i33 = (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

        return Matrix4(
            i00, i01, i02, i03,
            i10, i11, i12, i13,
            i20, i21, i22, i23,
            i30, i31, i32, i33
        );
    }

    /// Return float data.
    const float* Data() const { return &m00; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero matrix.
    static const Matrix4 ZERO;
    /// Identity matrix.
    static const Matrix4 IDENTITY;
};

/// Multiply a 4x4 matrix with a scalar
inline Matrix4 operator * (float lhs, const Matrix4& rhs) { return rhs * lhs; }
