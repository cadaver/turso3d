// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Vector2.h"

/// Three-dimensional vector.
class Vector3
{
public:
    /// X coordinate.
    float x;
    /// Y coordinate.
    float y;
    /// Z coordinate.
    float z;
    
    /// Construct undefined.
    Vector3()
    {
    }
    
    /// Copy-construct.
    Vector3(const Vector3& vector) :
        x(vector.x),
        y(vector.y),
        z(vector.z)
    {
    }
    
    /// Construct from a two-dimensional vector and the Z coordinate.
    Vector3(const Vector2& vector, float z_) :
        x(vector.x),
        y(vector.y),
        z(z_)
    {
    }

    /// Construct from a two-dimensional vector, with Z coordinate left zero.
    Vector3(const Vector2& vector) :
        x(vector.x),
        y(vector.y),
        z(0.0f)
    {
    }
    
    /// Construct from coordinates.
    Vector3(float x_, float y_, float z_) :
        x(x_),
        y(y_),
        z(z_)
    {
    }

    /// Construct from two-dimensional coordinates, with Z coordinate left zero.
    Vector3(float x_, float y_) :
        x(x_),
        y(y_),
        z(0.0f)
    {
    }
    
    /// Construct from a float array.
    Vector3(const float* data) :
        x(data[0]),
        y(data[1]),
        z(data[2])
    {
    }
    
    /// Construct by parsing a string.
    Vector3(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    Vector3(const char* str)
    {
        FromString(str);
    }
    
    /// Assign from another vector.
    Vector3& operator = (const Vector3& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        return *this;
    }
    
    /// Test for equality with another vector without epsilon.
    bool operator == (const Vector3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
    /// Test for inequality with another vector without epsilon.
    bool operator != (const Vector3& rhs) const { return !(*this == rhs); }
    /// Add a vector.
    Vector3 operator + (const Vector3& rhs) const { return Vector3(x + rhs.x, y + rhs.y, z + rhs.z); }
    /// Return negation.
    Vector3 operator - () const { return Vector3(-x, -y, -z); }
    /// Subtract a vector.
    Vector3 operator - (const Vector3& rhs) const { return Vector3(x - rhs.x, y - rhs.y, z - rhs.z); }
    /// Multiply with a scalar.
    Vector3 operator * (float rhs) const { return Vector3(x * rhs, y * rhs, z * rhs); }
    /// Multiply with a vector.
    Vector3 operator * (const Vector3& rhs) const { return Vector3(x * rhs.x, y * rhs.y, z * rhs.z); }
    /// Divide by a scalar.
    Vector3 operator / (float rhs) const { return Vector3(x / rhs, y / rhs, z / rhs); }
    /// Divide by a vector.
    Vector3 operator / (const Vector3& rhs) const { return Vector3(x / rhs.x, y / rhs.y, z / rhs.z); }
    
    /// Add-assign a vector.
    Vector3& operator += (const Vector3& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
    
    /// Subtract-assign a vector.
    Vector3& operator -= (const Vector3& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }
    
    /// Multiply-assign a scalar.
    Vector3& operator *= (float rhs)
    {
        x *= rhs;
        y *= rhs;
        z *= rhs;
        return *this;
    }
    
    /// Multiply-assign a vector.
    Vector3& operator *= (const Vector3& rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        return *this;
    }
    
    /// Divide-assign a scalar.
    Vector3& operator /= (float rhs)
    {
        float invRhs = 1.0f / rhs;
        x *= invRhs;
        y *= invRhs;
        z *= invRhs;
        return *this;
    }
    
    /// Divide-assign a vector.
    Vector3& operator /= (const Vector3& rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        return *this;
    }
    
    /// Normalize to unit length.
    void Normalize()
    {
        float lenSquared = LengthSquared();
        if (!::Equals(lenSquared, 1.0f) && lenSquared > 0.0f)
        {
            float invLen = 1.0f / sqrtf(lenSquared);
            x *= invLen;
            y *= invLen;
            z *= invLen;
        }
    }
    
    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    
    /// Return length.
    float Length() const { return sqrtf(x * x + y * y + z * z); }
    /// Return squared length.
    float LengthSquared() const { return x * x + y * y + z * z; }
    /// Calculate dot product.
    float DotProduct(const Vector3& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }
    /// Calculate absolute dot product.
    float AbsDotProduct(const Vector3& rhs) const { return ::Abs(x * rhs.x) + ::Abs(y * rhs.y) + ::Abs(z * rhs.z); }
    
    /// Calculate cross product.
    Vector3 CrossProduct(const Vector3& rhs) const
    {
        return Vector3(
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        );
    }
    
    /// Return absolute vector.
    Vector3 Abs() const { return Vector3(::Abs(x), ::Abs(y), ::Abs(z)); }
    /// Linear interpolation with another vector.
    Vector3 Lerp(const Vector3& rhs, float t) const { return *this * (1.0f - t) + rhs * t; }
    /// Test for equality with another vector with epsilon.
    bool Equals(const Vector3& rhs, float epsilon = M_EPSILON) const { return ::Equals(x, rhs.x, epsilon) && ::Equals(y, rhs.y, epsilon) && ::Equals(z, rhs.z, epsilon); }
    /// Return the angle between this vector and another vector in degrees.
    float Angle(const Vector3& rhs) const { return ::Acos(DotProduct(rhs) / (Length() * rhs.Length() ) ); }
    /// Return whether is NaN.
    bool IsNaN() const { return ::IsNaN(x) || ::IsNaN(y) || ::IsNaN(z); }
    
    /// Return normalized to unit length.
    Vector3 Normalized() const
    {
        float lenSquared = LengthSquared();
        if (!::Equals(lenSquared, 1.0f) && lenSquared > 0.0f)
        {
            float invLen = 1.0f / sqrtf(lenSquared);
            return *this * invLen;
        }
        else
            return *this;
    }
    
    /// Return float data.
    const float* Data() const { return &x; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero vector.
    static const Vector3 ZERO;
    /// (-1,0,0) vector.
    static const Vector3 LEFT;
    /// (1,0,0) vector.
    static const Vector3 RIGHT;
    /// (0,1,0) vector.
    static const Vector3 UP;
    /// (0,-1,0) vector.
    static const Vector3 DOWN;
    /// (0,0,1) vector.
    static const Vector3 FORWARD;
    /// (0,0,-1) vector.
    static const Vector3 BACK;
    /// (1,1,1) vector.
    static const Vector3 ONE;
};

/// Multiply Vector3 with a scalar.
inline Vector3 operator * (float lhs, const Vector3& rhs) { return rhs * lhs; }
