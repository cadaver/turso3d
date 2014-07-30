// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Quaternion.h"
#include "Vector4.h"

namespace Turso3D
{

/// 4x4 matrix for arbitrary linear transforms including projection.
class TURSO3D_API Matrix4
{
public:
    /// Matrix values.
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    float m30, m31, m32, m33;
    
    /// Construct an identity matrix.
    Matrix4() :
        m00(1.0f),
        m01(0.0f),
        m02(0.0f),
        m03(0.0f),
        m10(0.0f),
        m11(1.0f),
        m12(0.0f),
        m13(0.0f),
        m20(0.0f),
        m21(0.0f),
        m22(1.0f),
        m23(0.0f),
        m30(0.0f),
        m31(0.0f),
        m32(0.0f),
        m33(1.0f)
    {
    }
    
    /// Copy-construct.
    Matrix4(const Matrix4& matrix) :
        m00(matrix.m00),
        m01(matrix.m01),
        m02(matrix.m02),
        m03(matrix.m03),
        m10(matrix.m10),
        m11(matrix.m11),
        m12(matrix.m12),
        m13(matrix.m13),
        m20(matrix.m20),
        m21(matrix.m21),
        m22(matrix.m22),
        m23(matrix.m23),
        m30(matrix.m30),
        m31(matrix.m31),
        m32(matrix.m32),
        m33(matrix.m33)
    {
    }
    
    /// Copy-cnstruct from a 3x3 matrix and set the extra elements to identity.
    Matrix4(const Matrix3& matrix) :
        m00(matrix.m00),
        m01(matrix.m01),
        m02(matrix.m02),
        m03(0.0f),
        m10(matrix.m10),
        m11(matrix.m11),
        m12(matrix.m12),
        m13(0.0f),
        m20(matrix.m20),
        m21(matrix.m21),
        m22(matrix.m22),
        m23(0.0f),
        m30(0.0f),
        m31(0.0f),
        m32(0.0f),
        m33(1.0f)
    {
    }
    
    // Construct from values.
    Matrix4(float v00, float v01, float v02, float v03,
            float v10, float v11, float v12, float v13,
            float v20, float v21, float v22, float v23,
            float v30, float v31, float v32, float v33) :
        m00(v00),
        m01(v01),
        m02(v02),
        m03(v03),
        m10(v10),
        m11(v11),
        m12(v12),
        m13(v13),
        m20(v20),
        m21(v21),
        m22(v22),
        m23(v23),
        m30(v30),
        m31(v31),
        m32(v32),
        m33(v33)
    {
    }
    
    /// Construct from a float array.
    Matrix4(const float* data) :
        m00(data[0]),
        m01(data[1]),
        m02(data[2]),
        m03(data[3]),
        m10(data[4]),
        m11(data[5]),
        m12(data[6]),
        m13(data[7]),
        m20(data[8]),
        m21(data[9]),
        m22(data[10]),
        m23(data[11]),
        m30(data[12]),
        m31(data[13]),
        m32(data[14]),
        m33(data[15])
    {
    }
    
    /// Assign from another matrix.
    Matrix4& operator = (const Matrix4& rhs)
    {
        m00 = rhs.m00;
        m01 = rhs.m01;
        m02 = rhs.m02;
        m03 = rhs.m03;
        m10 = rhs.m10;
        m11 = rhs.m11;
        m12 = rhs.m12;
        m13 = rhs.m13;
        m20 = rhs.m20;
        m21 = rhs.m21;
        m22 = rhs.m22;
        m23 = rhs.m23;
        m30 = rhs.m30;
        m31 = rhs.m31;
        m32 = rhs.m32;
        m33 = rhs.m33;
        return *this;
    }
    
    /// Assign from a 3x3 matrix. Set the extra elements to identity.
    Matrix4& operator = (const Matrix3& rhs)
    {
        m00 = rhs.m00;
        m01 = rhs.m01;
        m02 = rhs.m02;
        m03 = 0.0f;
        m10 = rhs.m10;
        m11 = rhs.m11;
        m12 = rhs.m12;
        m13 = 0.0f;
        m20 = rhs.m20;
        m21 = rhs.m21;
        m22 = rhs.m22;
        m23 = 0.0f;
        m30 = 0.0f;
        m31 = 0.0f;
        m32 = 0.0f;
        m33 = 1.0f;
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
            m00 + rhs.m00,
            m01 + rhs.m01,
            m02 + rhs.m02,
            m03 + rhs.m03,
            m10 + rhs.m10,
            m11 + rhs.m11,
            m12 + rhs.m12,
            m13 + rhs.m13,
            m20 + rhs.m20,
            m21 + rhs.m21,
            m22 + rhs.m22,
            m23 + rhs.m23,
            m30 + rhs.m30,
            m31 + rhs.m31,
            m32 + rhs.m32,
            m33 + rhs.m33
        );
    }
    
    /// Subtract a matrix.
    Matrix4 operator - (const Matrix4& rhs) const
    {
        return Matrix4(
            m00 - rhs.m00,
            m01 - rhs.m01,
            m02 - rhs.m02,
            m03 - rhs.m03,
            m10 - rhs.m10,
            m11 - rhs.m11,
            m12 - rhs.m12,
            m13 - rhs.m13,
            m20 - rhs.m20,
            m21 - rhs.m21,
            m22 - rhs.m22,
            m23 - rhs.m23,
            m30 - rhs.m30,
            m31 - rhs.m31,
            m32 - rhs.m32,
            m33 - rhs.m33
        );
    }
    
    /// Multiply with a scalar.
    Matrix4 operator * (float rhs) const
    {
        return Matrix4(
            m00 * rhs,
            m01 * rhs,
            m02 * rhs,
            m03 * rhs,
            m10 * rhs,
            m11 * rhs,
            m12 * rhs,
            m13 * rhs,
            m20 * rhs,
            m21 * rhs,
            m22 * rhs,
            m23 * rhs,
            m30 * rhs,
            m31 * rhs,
            m32 * rhs,
            m33 * rhs
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
        m00 = rotation.m00;
        m01 = rotation.m01;
        m02 = rotation.m02;
        m10 = rotation.m10;
        m11 = rotation.m11;
        m12 = rotation.m12;
        m20 = rotation.m20;
        m21 = rotation.m21;
        m22 = rotation.m22;
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
    
    /// Return the combined rotation and scaling matrix.
    Matrix3 ToMatrix3() const
    {
        return Matrix3(
            m00,
            m01,
            m02,
            m10,
            m11,
            m12,
            m20,
            m21,
            m22
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
    
    /// Return the scaling part
    Vector3 Scale() const
    {
        return Vector3(
            sqrtf(m00 * m00 + m10 * m10 + m20 * m20),
            sqrtf(m01 * m01 + m11 * m11 + m21 * m21),
            sqrtf(m02 * m02 + m12 * m12 + m22 * m22)
        );
    }
    
    /// Return transpose
    Matrix4 Transpose() const
    {
        return Matrix4(
            m00,
            m10,
            m20,
            m30,
            m01,
            m11,
            m21,
            m31,
            m02,
            m12,
            m22,
            m32,
            m03,
            m13,
            m23,
            m33
        );
    }
    
    /// Test for equality with another matrix with epsilon.
    bool Equals(const Matrix4& rhs) const
    {
        const float* leftData = Data();
        const float* rightData = rhs.Data();
        
        for (size_t i = 0; i < 16; ++i)
        {
            if (!Turso3D::Equals(leftData[i], rightData[i]))
                return false;
        }
        
        return true;
    }
    
    /// Return decomposition to translation, rotation and scale.
    void Decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const;
    /// Return inverse.
    Matrix4 Inverse() const;
    
    /// Return float data
    const float* Data() const { return &m00; }
    /// Return as string.
    String ToString() const;
    
    /// Bulk transpose matrices.
    static void BulkTranspose(float* dest, const float* src, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            dest[0] = src[0];
            dest[1] = src[4];
            dest[2] = src[8];
            dest[3] = src[12];
            dest[4] = src[1];
            dest[5] = src[5];
            dest[6] = src[9];
            dest[7] = src[13];
            dest[8] = src[2];
            dest[9] = src[6];
            dest[10] = src[10];
            dest[11] = src[14];
            dest[12] = src[3];
            dest[13] = src[7];
            dest[14] = src[11];
            dest[15] = src[15];
            
            dest += 16;
            src += 16;
        }
    }
    
    /// Zero matrix.
    static const Matrix4 ZERO;
    /// Identity matrix.
    static const Matrix4 IDENTITY;
};

/// Multiply a 4x4 matrix with a scalar
inline Matrix4 operator * (float lhs, const Matrix4& rhs) { return rhs * lhs; }

}
