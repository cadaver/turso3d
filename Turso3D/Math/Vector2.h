// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Math.h"
#include "../Base/String.h"

namespace Turso3D
{

/// Two-dimensional vector.
class TURSO3D_API Vector2
{
public:
    /// X coordinate.
    float x;
    /// Y coordinate.
    float y;
    
    /// Construct a zero vector.
    Vector2() :
        x(0.0f),
        y(0.0f)
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
        if (!Turso3D::Equals(lenSquared, 1.0f) && lenSquared > 0.0f)
        {
            float invLen = 1.0f / sqrtf(lenSquared);
            x *= invLen;
            y *= invLen;
        }
    }
    
    /// Test for equality with another vector without epsilon.
    bool operator == (const Vector2& rhs) const { return x == rhs.x && y == rhs.y; }
    /// Test for inequality with another vector without epsilon.
    bool operator != (const Vector2& rhs) const { return x != rhs.x || y != rhs.y; }
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
    float AbsDotProduct(const Vector2& rhs) const { return Turso3D::Abs(x * rhs.x) + Turso3D::Abs(y * rhs.y); }
    /// Return absolute vector.
    Vector2 Abs() const { return Vector2(Turso3D::Abs(x), Turso3D::Abs(y)); }
    /// Linear interpolation with another vector.
    Vector2 Lerp(const Vector2& rhs, float t) const { return *this * (1.0f - t) + rhs * t; }
    /// Test for equality with another vector with epsilon.
    bool Equals(const Vector2& rhs) const { return Turso3D::Equals(x, rhs.x) && Turso3D::Equals(y, rhs.y); }
    /// Return whether is NaN.
    bool IsNaN() const { return Turso3D::IsNaN(x) || Turso3D::IsNaN(y); }
    
    /// Return normalized to unit length.
    Vector2 Normalized() const
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
    
    /// Return float data.
    const float* Data() const { return &x; }
    /// Return as string.
    String ToString() const;
    
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

/// Two-dimensional vector with integer values.
class TURSO3D_API IntVector2
{
public:
    /// X coordinate.
    int x;
    /// Y coordinate.
    int y;
    
    /// Construct a zero vector.
    IntVector2() :
        x(0),
        y(0)
    {
    }
    
    /// Construct from coordinates.
    IntVector2(int x_, int y_) :
        x(x_),
        y(y_)
    {
    }
    
    /// Construct from an int array.
    IntVector2(const int* data) :
        x(data[0]),
        y(data[1])
    {
    }
    
    /// Copy-construct.
    IntVector2(const IntVector2& rhs) :
        x(rhs.x),
        y(rhs.y)
    {
    }
    
    /// Test for equality with another vector.
    bool operator == (const IntVector2& rhs) const { return x == rhs.x && y == rhs.y; }
    /// Test for inequality with another vector.
    bool operator != (const IntVector2& rhs) const { return x != rhs.x || y != rhs.y; }
    /// Add a vector.
    IntVector2 operator + (const IntVector2& rhs) const { return IntVector2(x + rhs.x, y + rhs.y); }
    /// Return negation.
    IntVector2 operator - () const { return IntVector2(-x, -y); }
    /// Subtract a vector.
    IntVector2 operator - (const IntVector2& rhs) const { return IntVector2(x - rhs.x, y - rhs.y); }
    /// Multiply with a scalar.
    IntVector2 operator * (int rhs) const { return IntVector2(x * rhs, y * rhs); }
    /// Divide by a scalar.
    IntVector2 operator / (int rhs) const { return IntVector2(x / rhs, y / rhs); }
    
    /// Add-assign a vector.
    IntVector2& operator += (const IntVector2& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    
    /// Subtract-assign a vector.
    IntVector2& operator -= (const IntVector2& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    
    /// Multiply-assign a scalar.
    IntVector2& operator *= (int rhs)
    {
        x *= rhs;
        y *= rhs;
        return *this;
    }
    
    /// Divide-assign a scalar.
    IntVector2& operator /= (int rhs)
    {
        x /= rhs;
        y /= rhs;
        return *this;
    }
    
    /// Return integer data.
    const int* Data() const { return &x; }
    /// Return as string.
    String ToString() const;
    
    /// Zero vector.
    static const IntVector2 ZERO;
};

/// Multiply IntVector2 with a scalar.
inline IntVector2 operator * (int lhs, const IntVector2& rhs) { return rhs * lhs; }

}
