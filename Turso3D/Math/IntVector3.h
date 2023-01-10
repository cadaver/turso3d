// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Math.h"

#include <string>

/// Three-dimensional vector with integer values.
class IntVector3
{
public:
    /// X coordinate.
    int x;
    /// Y coordinate.
    int y;
    /// Z coordinate.
    int z;

    /// Construct undefined.
    IntVector3()
    {
    }
    
    /// Copy-construct.
    IntVector3(const IntVector3& vector) :
        x(vector.x),
        y(vector.y),
        z(vector.z)
    {
    }
    
    /// Construct from coordinates.
    IntVector3(int x_, int y_, int z_) :
        x(x_),
        y(y_),
        z(z_)
    {
    }
    
    /// Construct from an int array.
    IntVector3(const int* data) :
        x(data[0]),
        y(data[1]),
        z(data[2])
    {
    }
    
    /// Construct by parsing a string.
    IntVector3(const std::string& str)
    {
        FromString(str);
    }
    
    /// Construct by parsing a C string.
    IntVector3(const char* str)
    {
        FromString(str);
    }
    
    /// Add-assign a vector.
    IntVector3& operator += (const IntVector3& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
    
    /// Subtract-assign a vector.
    IntVector3& operator -= (const IntVector3& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }
    
    /// Multiply-assign a scalar.
    IntVector3& operator *= (int rhs)
    {
        x *= rhs;
        y *= rhs;
        z *= rhs;
        return *this;
    }
    
    /// Divide-assign a scalar.
    IntVector3& operator /= (int rhs)
    {
        x /= rhs;
        y /= rhs;
        z /= rhs;
        return *this;
    }
    
    /// Test for equality with another vector.
    bool operator == (const IntVector3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
    /// Test for inequality with another vector.
    bool operator != (const IntVector3& rhs) const { return !(*this == rhs); }
    /// Add a vector.
    IntVector3 operator + (const IntVector3& rhs) const { return IntVector3(x + rhs.x, y + rhs.y, z + rhs.z); }
    /// Return negation.
    IntVector3 operator - () const { return IntVector3(-x, -y, -z); }
    /// Subtract a vector.
    IntVector3 operator - (const IntVector3& rhs) const { return IntVector3(x - rhs.x, y - rhs.y, z - rhs.z); }
    /// Multiply with a scalar.
    IntVector3 operator * (int rhs) const { return IntVector3(x * rhs, y * rhs, z * rhs); }
    /// Divide by a scalar.
    IntVector3 operator / (int rhs) const { return IntVector3(x / rhs, y / rhs, z / rhs); }

    /// Parse from a string. Return true on success.
    bool FromString(const std::string& string);
    /// Parse from a C string. Return true on success.
    bool FromString(const char* str);
    
    /// Return integer data.
    const int* Data() const { return &x; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero vector.
    static const IntVector3 ZERO;
};

/// Multiply IntVector2 with a scalar.
inline IntVector3 operator * (int lhs, const IntVector3& rhs) { return rhs * lhs; }
