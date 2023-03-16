// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <cstdlib>
#include <cmath>
#include <limits>

#undef M_PI
static const float M_PI = 3.14159265358979323846264338327950288f;
static const float M_HALF_PI = M_PI * 0.5f;
static const int M_MIN_INT = 0x80000000;
static const int M_MAX_INT = 0x7fffffff;
static const unsigned M_MIN_UNSIGNED = 0x00000000;
static const unsigned M_MAX_UNSIGNED = 0xffffffff;

static const float M_EPSILON = 0.000001f;
static const float M_MAX_FLOAT = 3.402823466e+38f;
static const float M_INFINITY = (float)HUGE_VAL;
static const float M_DEGTORAD = (float)M_PI / 180.0f;
static const float M_DEGTORAD_2 = (float)M_PI / 360.0f; // M_DEGTORAD / 2.f
static const float M_RADTODEG = 1.0f / M_DEGTORAD;

/// Intersection test result.
enum Intersection
{
    OUTSIDE = 0,
    INTERSECTS,
    INSIDE
};

/// Check whether two floating point values are equal within accuracy.
inline bool Equals(float lhs, float rhs, float epsilon = M_EPSILON) { return lhs + epsilon >= rhs && lhs - epsilon <= rhs; }
/// Check whether a floating point value is NaN.
inline bool IsNaN(float value) { return value != value; }
/// Linear interpolation between two float values.
inline float Lerp(float lhs, float rhs, float t) { return lhs * (1.0f - t) + rhs * t; }
/// Return the smaller of two floats.
inline float Min(float lhs, float rhs) { return lhs < rhs ? lhs : rhs; }
/// Return the larger of two floats.
inline float Max(float lhs, float rhs) { return lhs > rhs ? lhs : rhs; }
/// Return absolute value of a float.
inline float Abs(float value) { return value >= 0.0f ? value : -value; }
/// Return the sign of a float (-1, 0 or 1.)
inline float Sign(float value) { return value > 0.0f ? 1.0f : (value < 0.0f ? -1.0f : 0.0f); }

/// Clamp a float to a range.
inline float Clamp(float value, float min, float max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

/// Smoothly damp between values.
inline float SmoothStep(float lhs, float rhs, float t)
{
    t = Clamp((t - lhs) / (rhs - lhs), 0.0f, 1.0f); // Saturate t
    return t * t * (3.0f - 2.0f * t);
}

/// Return sine of an angle in degrees.
inline float Sin(float angle) { return sinf(angle * M_DEGTORAD); }
/// Return cosine of an angle in degrees.
inline float Cos(float angle) { return cosf(angle * M_DEGTORAD); }
/// Return tangent of an angle in degrees.
inline float Tan(float angle) { return tanf(angle * M_DEGTORAD); }
/// Return arc sine in degrees.
inline float Asin(float x) { return M_RADTODEG * asinf(Clamp(x, -1.0f, 1.0f)); }
/// Return arc cosine in degrees.
inline float Acos(float x) { return M_RADTODEG * acosf(Clamp(x, -1.0f, 1.0f)); }
/// Return arc tangent in degrees.
inline float Atan(float x) { return M_RADTODEG * atanf(x); }
/// Return arc tangent of y/x in degrees.
inline float Atan2(float y, float x) { return M_RADTODEG * atan2f(y, x); }

/// Return the smaller of two integer values.
inline int Min(int lhs, int rhs) { return lhs < rhs ? lhs : rhs; }
/// Return the larger of two integer values.
inline int Max(int lhs, int rhs) { return lhs > rhs ? lhs : rhs; }
/// Return the smaller of two integer values.
inline size_t Min(size_t lhs, size_t rhs) { return lhs < rhs ? lhs : rhs; }
/// Return the larger of two integer values.
inline size_t Max(size_t lhs, size_t rhs) { return lhs > rhs ? lhs : rhs; }

/// Return absolute value of an integer.
inline int Abs(int value) { return value >= 0 ? value : -value; }

/// Clamp an integer to a range.
inline int Clamp(int value, int min, int max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

/// Check whether an unsigned integer is a power of two.
inline bool IsPowerOfTwo(unsigned value)
{
    if (!value)
        return true;
    while (!(value & 1))
        value >>= 1;
    return value == 1;
}

/// Round up to next power of two.
inline unsigned NextPowerOfTwo(unsigned value)
{
    unsigned ret = 1;
    while (ret < value && ret < 0x80000000)
        ret <<= 1;
    return ret;
}
