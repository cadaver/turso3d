// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Math.h"

#include <string>

/// Two-dimensional vector.
class Vector2
{
public:
    /// X coordinate.
    float x;
    /// Y coordinate.
    float y;
    
    /// Construct undefined.
    Vector2()
    {
    }
    
    /// Copy-construct.
    Vector2(const Vector2& vector) :
        x(vector.x),
        y(vector.y)
    {
    }
    
    /// Construct from coordinates.
    Vector2(float x_, float y_) :
        x(x_),
        y(y_)
    {
    }
    
    /// Construct from a float array.
    Vector2(const float* data) :
        x(data[0]),
        y(data[1])
    {
    }
    
    /// Construct by parsing a string.
    Vector2(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    Vector2(const char* str)
    {
        FromString(str);
    }
    
    /// Assign from another vector.
    Vector2& operator = (const Vector2& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }
    
    /// Add-assign a vector.
    Vector2& operator += (const Vector2& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    
    /// Subtract-assign a vector.
    Vector2& operator -= (const Vector2& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    
    /// Multiply-assign a scalar.
    Vector2& operator *= (float rhs)
    {
        x *= rhs;
        y *= rhs;
        return *this;
    }
    
    /// Multiply-assign a vector.
    Vector2& operator *= (const Vector2& rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }
    
    /// Divide-assign a scalar.
    Vector2& operator /= (float rhs)
    {
        float invRhs = 1.0f / rhs;
        x *= invRhs;
        y *= invRhs;
        return *this;
    }
    
    /// Divide-assign a vector.
    Vector2& operator /= (const Vector2& rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
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
        }
    }
    
    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    
    /// Test for equality with another vector without epsilon.
    bool operator == (const Vector2& rhs) const { return x == rhs.x && y == rhs.y; }
    /// Test for inequality with another vector without epsilon.
    bool operator != (const Vector2& rhs) const { return !(*this == rhs); }
    /// Add a vector.
    Vector2 operator + (const Vector2& rhs) const { return Vector2(x + rhs.x, y + rhs.y); }
    /// Return negation.
    Vector2 operator - () const { return Vector2(-x, -y); }
    /// Subtract a vector.
    Vector2 operator - (const Vector2& rhs) const { return Vector2(x - rhs.x, y - rhs.y); }
    /// Multiply with a scalar.
    Vector2 operator * (float rhs) const { return Vector2(x * rhs, y * rhs); }
    /// Multiply with a vector.
    Vector2 operator * (const Vector2& rhs) const { return Vector2(x * rhs.x, y * rhs.y); }
    /// Divide by a scalar.
    Vector2 operator / (float rhs) const { return Vector2(x / rhs, y / rhs); }
    /// Divide by a vector.
    Vector2 operator / (const Vector2& rhs) const { return Vector2(x / rhs.x, y / rhs.y); }
    
    /// Return length.
    float Length() const { return sqrtf(x * x + y * y); }
    /// Return squared length.
    float LengthSquared() const { return x * x + y * y; }
    /// Calculate dot product.
    float DotProduct(const Vector2& rhs) const { return x * rhs.x + y * rhs.y; }
    /// Calculate absolute dot product.
    float AbsDotProduct(const Vector2& rhs) const { return ::Abs(x * rhs.x) + ::Abs(y * rhs.y); }
    /// Return absolute vector.
    Vector2 Abs() const { return Vector2(::Abs(x), ::Abs(y)); }
    /// Linear interpolation with another vector.
    Vector2 Lerp(const Vector2& rhs, float t) const { return *this * (1.0f - t) + rhs * t; }
    /// Test for equality with another vector with epsilon.
    bool Equals(const Vector2& rhs, float epsilon = M_EPSILON) const { return ::Equals(x, rhs.x, epsilon) && ::Equals(y, rhs.y, epsilon); }
    /// Return whether is NaN.
    bool IsNaN() const { return ::IsNaN(x) || ::IsNaN(y); }
    
    /// Return normalized to unit length.
    Vector2 Normalized() const
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
    static const Vector2 ZERO;
    /// (-1,0) vector.
    static const Vector2 LEFT;
    /// (1,0) vector.
    static const Vector2 RIGHT;
    /// (0,1) vector.
    static const Vector2 UP;
    /// (0,-1) vector.
    static const Vector2 DOWN;
    /// (1,1) vector.
    static const Vector2 ONE;
};

/// Multiply Vector2 with a scalar
inline Vector2 operator * (float lhs, const Vector2& rhs) { return rhs * lhs; }
