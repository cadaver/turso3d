// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "IntVector3.h"

#include <string>

/// Three-dimensional bounding box with integer values.
class IntBox
{
public:
    /// Left coordinate.
    int left;
    /// Top coordinate.
    int top;
    /// Near coordinate.
    int near;
    /// Right coordinate.
    int right;
    /// Bottom coordinate.
    int bottom;
    /// Far coordinate.
    int far;
    
    /// Construct undefined.
    IntBox()
    {
    }
    
    /// Copy-construct.
    IntBox(const IntBox& box) :
        left(box.left),
        top(box.top),
        near(box.near),
        right(box.right),
        bottom(box.bottom),
        far(box.far)
    {
    }
    
    /// Construct from coordinates.
    IntBox(int left_, int top_, int near_, int right_, int bottom_, int far_) :
        left(left_),
        top(top_),
        near(near_),
        right(right_),
        bottom(bottom_),
        far(far_)
    {
    }
    
    /// Construct from an int array.
    IntBox(const int* data) :
        left(data[0]),
        top(data[1]),
        near(data[2]),
        right(data[3]),
        bottom(data[4]),
        far(data[5])
    {
    }

    /// Construct by parsing a string.
    IntBox(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    IntBox(const char* str)
    {
        FromString(str);
    }
    
    /// Test for equality with another box.
    bool operator == (const IntBox& rhs) const { return left == rhs.left && top == rhs.top && right == rhs.right && bottom == rhs.bottom && near == rhs.near && far == rhs.far; }
    /// Test for inequality with another box.
    bool operator != (const IntBox& rhs) const { return !(*this == rhs); }
    
    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    
    /// Return size.
    IntVector3 Size() const { return IntVector3(Width(), Height(), Depth()); }
    /// Return width.
    int Width() const { return right - left; }
    /// Return height.
    int Height() const { return bottom - top; }
    /// Return depth.
    int Depth() const { return far - near; }
    
    /// Test whether a point is inside.
    Intersection IsInside(const IntVector3& point) const
    {
        if (point.x < left || point.y < top || point.z < near || point.x >= right || point.y >= bottom || point.z >= far)
            return OUTSIDE;
        else
            return INSIDE;
    }

    /// Test whether another box is inside or intersects.
    Intersection IsInside(const IntBox& box) const
    {
        if (box.right <= left || box.left >= right || box.bottom <= top || box.top >= bottom || box.far <= near || box.near >= far)
            return OUTSIDE;
        else if (box.left >= left && box.right <= right && box.top >= top && box.bottom <= bottom && box.near >= near && box.far <= far)
            return INSIDE;
        else
            return INTERSECTS;
    }
    
    /// Return integer data.
    const int* Data() const { return &left; }
    /// Return as string.
    std::string ToString() const;
    
    /// Zero-sized box.
    static const IntBox ZERO;
};
