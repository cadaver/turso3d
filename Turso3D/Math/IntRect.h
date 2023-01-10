// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "IntVector2.h"

#include <string>

/// Two-dimensional bounding rectangle with integer values.
class IntRect
{
public:
    /// Left coordinate.
    int left;
    /// Top coordinate.
    int top;
    /// Right coordinate.
    int right;
    /// Bottom coordinate.
    int bottom;
    
    /// Construct undefined.
    IntRect()
    {
    }
    
    /// Copy-construct.
    IntRect(const IntRect& rect) :
        left(rect.left),
        top(rect.top),
        right(rect.right),
        bottom(rect.bottom)
    {
    }
    
    /// Construct from coordinates.
    IntRect(int left_, int top_, int right_, int bottom_) :
        left(left_),
        top(top_),
        right(right_),
        bottom(bottom_)
    {
    }
    
    /// Construct from an int array.
    IntRect(const int* data) :
        left(data[0]),
        top(data[1]),
        right(data[2]),
        bottom(data[3])
    {
    }

    /// Construct by parsing a string.
    IntRect(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    IntRect(const char* str)
    {
        FromString(str);
    }
    
    /// Test for equality with another rect.
    bool operator == (const IntRect& rhs) const { return left == rhs.left && top == rhs.top && right == rhs.right && bottom == rhs.bottom; }
    /// Test for inequality with another rect.
    bool operator != (const IntRect& rhs) const { return !(*this == rhs); }
    
    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    
    /// Return size.
    IntVector2 Size() const { return IntVector2(Width(), Height()); }
    /// Return width.
    int Width() const { return right - left; }
    /// Return height.
    int Height() const { return bottom - top; }
    
    /// Test whether a point is inside.
    Intersection IsInside(const IntVector2& point) const
    {
        if (point.x < left || point.y < top || point.x >= right || point.y >= bottom)
            return OUTSIDE;
        else
            return INSIDE;
    }

    /// Test whether another rect is inside or intersects.
    Intersection IsInside(const IntRect& rect) const
    {
        if (rect.right <= left || rect.left >= right || rect.bottom <= top || rect.top >= bottom)
            return OUTSIDE;
        else if (rect.left >= left && rect.right <= right && rect.top >= top && rect.bottom <= bottom)
            return INSIDE;
        else
            return INTERSECTS;
    }
    
    /// Return integer data.
    const int* Data() const { return &left; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero-sized rect.
    static const IntRect ZERO;
};
