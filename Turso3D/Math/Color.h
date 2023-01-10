// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Vector4.h"

#include <string>

// Defined by Windows headers
#undef TRANSPARENT

/// RGBA color.
class Color
{
public:
    /// Red value.
    float r;
    /// Green value.
    float g;
    /// Blue value.
    float b;
    /// Alpha value.
    float a;

    /// Construct undefined.
    Color()
    {
    }
    
    /// Copy-construct.
    Color(const Color& color) :
        r(color.r),
        g(color.g),
        b(color.b),
        a(color.a)
    {
    }
    
    /// Construct from another color and modify the alpha.
    Color(const Color& color, float a_) :
        r(color.r),
        g(color.g),
        b(color.b),
        a(a_)
    {
    }
    
    /// Construct from RGB values and set alpha fully opaque.
    Color(float r_, float g_, float b_) :
        r(r_),
        g(g_),
        b(b_),
        a(1.0f)
    {
    }
    
    /// Construct from RGBA values.
    Color(float r_, float g_, float b_, float a_) :
        r(r_),
        g(g_),
        b(b_),
        a(a_)
    {
    }

    /// Construct from a float array.
    Color(const float* data) :
        r(data[0]),
        g(data[1]),
        b(data[2]),
        a(data[3])
    {
    }
    
    /// Construct by parsing a string.
    Color(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    Color(const char* str)
    {
        FromString(str);
    }
    
    /// Add-assign a color.
    Color& operator += (const Color& rhs)
    {
        r += rhs.r;
        g += rhs.g;
        b += rhs.b;
        a += rhs.a;
        return *this;
    }
    
    /// Test for equality with another color without epsilon.
    bool operator == (const Color& rhs) const { return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a; }
    /// Test for inequality with another color without epsilon.
    bool operator != (const Color& rhs) const { return !(*this == rhs); }
    /// Multiply with a scalar.
    Color operator * (float rhs) const { return Color(r * rhs, g * rhs, b * rhs, a * rhs); }
    /// Add a color.
    Color operator + (const Color& rhs) const { return Color(r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a); }
    /// Substract a color.
    Color operator - (const Color& rhs) const { return Color(r - rhs.r, g - rhs.g, b - rhs.b, a - rhs.a); }
    
    /// Return float data.
    const float* Data() const { return &r; }

    /// Return color packed to a 32-bit integer, with R component in the lowest 8 bits. Components are clamped to [0, 1] range.
    unsigned ToUInt() const;
    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);

    /// Return RGB as a three-dimensional vector.
    Vector3 ToVector3() const { return Vector3(r, g, b); }
    /// Return RGBA as a four-dimensional vector.
    Vector4 ToVector4() const { return Vector4(r, g, b, a); }

    /// Return sum of RGB components.
    float SumRGB() const { return r + g + b; }
    /// Return average value of the RGB channels.
    float Average() const { return (r + g + b) / 3.0f; }

    /// Return linear interpolation of this color with another color.
    Color Lerp(const Color& rhs, float t) const;
    /// Return color with absolute components.
    Color Abs() const { return Color(::Abs(r), ::Abs(g), ::Abs(b), ::Abs(a)); }
    /// Test for equality with another color with epsilon.
    bool Equals(const Color& rhs, float epsilon = M_EPSILON) const { return ::Equals(r, rhs.r, epsilon) && ::Equals(g, rhs.g, epsilon) && ::Equals(b, rhs.b, epsilon) && ::Equals(a, rhs.a, epsilon); }
    
    /// Return as string.
    std::string ToString() const;
    
    /// Opaque white color.
    static const Color WHITE;
    /// Opaque gray color.
    static const Color GRAY;
    /// Opaque black color.
    static const Color BLACK;
    /// Opaque red color.
    static const Color RED;
    /// Opaque green color.
    static const Color GREEN;
    /// Opaque blue color.
    static const Color BLUE;
    /// Opaque cyan color.
    static const Color CYAN;
    /// Opaque magenta color.
    static const Color MAGENTA;
    /// Opaque yellow color.
    static const Color YELLOW;
    /// Transparent color (black with no alpha).
    static const Color TRANSPARENT;
};

/// Multiply Color with a scalar.
inline Color operator * (float lhs, const Color& rhs) { return rhs * lhs; }
