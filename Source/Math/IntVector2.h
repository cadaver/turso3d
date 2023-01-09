// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Math.h"

#include <string>

/// Two-dimensional vector with integer values.
class IntVector2
{
public:
    /// X coordinate.
    int x;
    /// Y coordinate.
    int y;
    
    /// Construct undefined.
    IntVector2()
    {
    }
    
    /// Copy-construct.
    IntVector2(const IntVector2& vector) :
        x(vector.x),
        y(vector.y)
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
    
    /// Construct by parsing a string.
    IntVector2(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    IntVector2(const char* str)
    {
        FromString(str);
    }
    
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
    
    /// Test for equality with another vector.
    bool operator == (const IntVector2& rhs) const { return x == rhs.x && y == rhs.y; }
    /// Test for inequality with another vector.
    bool operator != (const IntVector2& rhs) const { return !(*this == rhs); }
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

    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    
    /// Return integer data.
    const int* Data() const { return &x; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero vector.
    static const IntVector2 ZERO;
};

/// Multiply IntVector2 with a scalar.
inline IntVector2 operator * (int lhs, const IntVector2& rhs) { return rhs * lhs; }
