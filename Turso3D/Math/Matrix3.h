// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Vector3.h"

#include <string>

/// 3x3 matrix for rotation and scaling.
class Matrix3
{
public:
    /// Matrix values.
    float m00, m01, m02;
    float m10, m11, m12;
    float m20, m21, m22;

    /// Construct undefined.
    Matrix3()
    {
    }
    
    /// Copy-construct.
    Matrix3(const Matrix3& matrix) :
        m00(matrix.m00), m01(matrix.m01), m02(matrix.m02),
        m10(matrix.m10), m11(matrix.m11), m12(matrix.m12),
        m20(matrix.m20), m21(matrix.m21), m22(matrix.m22)
    {
    }
    
    /// Construct from values.
    Matrix3(float v00, float v01, float v02,
            float v10, float v11, float v12,
            float v20, float v21, float v22) :
        m00(v00), m01(v01), m02(v02),
        m10(v10), m11(v11), m12(v12),
        m20(v20), m21(v21), m22(v22)
    {
    }
    
    /// Construct from a float array.
    Matrix3(const float* data) :
        m00(data[0]), m01(data[1]), m02(data[2]),
        m10(data[3]), m11(data[4]), m12(data[5]),
        m20(data[6]), m21(data[7]), m22(data[8])
    {
    }
    
    /// Construct by parsing a string.
    Matrix3(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    Matrix3(const char* str)
    {
        FromString(str);
    }
    
    /// Assign from another matrix.
    Matrix3& operator = (const Matrix3& rhs)
    {
        m00 = rhs.m00; m01 = rhs.m01; m02 = rhs.m02;
        m10 = rhs.m10; m11 = rhs.m11; m12 = rhs.m12;
        m20 = rhs.m20; m21 = rhs.m21; m22 = rhs.m22;
        return *this;
    }
    
    /// Test for equality with another matrix without epsilon.
    bool operator == (const Matrix3& rhs) const
    {
        const float* leftData = Data();
        const float* rightData = rhs.Data();
        
        for (size_t i = 0; i < 9; ++i)
        {
            if (leftData[i] != rightData[i])
                return false;
        }
        
        return true;
    }
    
    /// Test for inequality with another matrix without epsilon.
    bool operator != (const Matrix3& rhs) const { return !(*this == rhs); }
    
    /// Multiply a Vector3.
    Vector3 operator * (const Vector3& rhs) const
    {
        return Vector3(
            m00 * rhs.x + m01 * rhs.y + m02 * rhs.z,
            m10 * rhs.x + m11 * rhs.y + m12 * rhs.z,
            m20 * rhs.x + m21 * rhs.y + m22 * rhs.z
        );
    }
    
    /// Add a matrix.
    Matrix3 operator + (const Matrix3& rhs) const
    {
        return Matrix3(
            m00 + rhs.m00, m01 + rhs.m01, m02 + rhs.m02,
            m10 + rhs.m10, m11 + rhs.m11, m12 + rhs.m12,
            m20 + rhs.m20, m21 + rhs.m21, m22 + rhs.m22
        );
    }
    
    /// Subtract a matrix.
    Matrix3 operator - (const Matrix3& rhs) const
    {
        return Matrix3(
            m00 - rhs.m00, m01 - rhs.m01, m02 - rhs.m02,
            m10 - rhs.m10, m11 - rhs.m11, m12 - rhs.m12,
            m20 - rhs.m20, m21 - rhs.m21, m22 - rhs.m22
        );
    }
    
    /// Multiply with a scalar.
    Matrix3 operator * (float rhs) const
    {
        return Matrix3(
            m00 * rhs, m01 * rhs, m02 * rhs,
            m10 * rhs, m11 * rhs, m12 * rhs,
            m20 * rhs, m21 * rhs, m22 * rhs
        );
    }
    
    /// Multiply a matrix.
    Matrix3 operator * (const Matrix3& rhs) const
    {
        return Matrix3(
            m00 * rhs.m00 + m01 * rhs.m10 + m02 * rhs.m20,
            m00 * rhs.m01 + m01 * rhs.m11 + m02 * rhs.m21,
            m00 * rhs.m02 + m01 * rhs.m12 + m02 * rhs.m22,
            m10 * rhs.m00 + m11 * rhs.m10 + m12 * rhs.m20,
            m10 * rhs.m01 + m11 * rhs.m11 + m12 * rhs.m21,
            m10 * rhs.m02 + m11 * rhs.m12 + m12 * rhs.m22,
            m20 * rhs.m00 + m21 * rhs.m10 + m22 * rhs.m20,
            m20 * rhs.m01 + m21 * rhs.m11 + m22 * rhs.m21,
            m20 * rhs.m02 + m21 * rhs.m12 + m22 * rhs.m22
        );
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
    Matrix3 Transpose() const
    {
        return Matrix3(
            m00, m10, m20,
            m01, m11, m21,
            m02, m12, m22
        );
    }
    
    /// Return scaled by a vector.
    Matrix3 Scaled(const Vector3& scale) const
    {
        return Matrix3(
            m00 * scale.x, m01 * scale.y, m02 * scale.z,
            m10 * scale.x, m11 * scale.y, m12 * scale.z,
            m20 * scale.x, m21 * scale.y, m22 * scale.z
        );
    }
    
    /// Test for equality with another matrix with epsilon.
    bool Equals(const Matrix3& rhs, float epsilon = M_EPSILON) const
    {
        const float* leftData = Data();
        const float* rightData = rhs.Data();
        
        for (size_t i = 0; i < 9; ++i)
        {
            if (!::Equals(leftData[i], rightData[i], epsilon))
                return false;
        }
        
        return true;
    }
    
    /// Return inverse.
    Matrix3 Inverse() const
    {
        float det = m00 * m11 * m22 +
            m10 * m21 * m02 +
            m20 * m01 * m12 -
            m20 * m11 * m02 -
            m10 * m01 * m22 -
            m00 * m21 * m12;

        float invDet = 1.0f / det;

        return Matrix3(
            (m11 * m22 - m21 * m12) * invDet,
            -(m01 * m22 - m21 * m02) * invDet,
            (m01 * m12 - m11 * m02) * invDet,
            -(m10 * m22 - m20 * m12) * invDet,
            (m00 * m22 - m20 * m02) * invDet,
            -(m00 * m12 - m10 * m02) * invDet,
            (m10 * m21 - m20 * m11) * invDet,
            -(m00 * m21 - m20 * m01) * invDet,
            (m00 * m11 - m10 * m01) * invDet
        );
    }
    
    /// Return float data.
    const float* Data() const { return &m00; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero matrix.
    static const Matrix3 ZERO;
    /// Identity matrix.
    static const Matrix3 IDENTITY;
};

/// Multiply a 3x3 matrix with a scalar.
inline Matrix3 operator * (float lhs, const Matrix3& rhs) { return rhs * lhs; }
