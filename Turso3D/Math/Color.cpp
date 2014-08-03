// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "Color.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const Color Color::WHITE;
const Color Color::GRAY   (0.5f, 0.5f, 0.5f);
const Color Color::BLACK  (0.0f, 0.0f, 0.0f);
const Color Color::RED    (1.0f, 0.0f, 0.0f);
const Color Color::GREEN  (0.0f, 1.0f, 0.0f);
const Color Color::BLUE   (0.0f, 0.0f, 1.0f);
const Color Color::CYAN   (0.0f, 1.0f, 1.0f);
const Color Color::MAGENTA(1.0f, 0.0f, 1.0f);
const Color Color::YELLOW (1.0f, 1.0f, 0.0f);
const Color Color::TRANSPARENT(0.0f, 0.0f, 0.0f, 0.0f);

unsigned Color::ToUInt() const
{
    unsigned r_ = Clamp(((int)(r * 255.0f)), 0, 255);
    unsigned g_ = Clamp(((int)(g * 255.0f)), 0, 255);
    unsigned b_ = Clamp(((int)(b * 255.0f)), 0, 255);
    unsigned a_ = Clamp(((int)(a * 255.0f)), 0, 255);
    return (a_ << 24) | (b_ << 16) | (g_ << 8) | r_;
}

Vector3 Color::ToHSL() const
{
    float min, max;
    Bounds(&min, &max, true);

    float h = Hue(min, max);
    float s = SaturationHSL(min, max);
    float l = (max + min) * 0.5f;

    return Vector3(h, s, l);
}

Vector3 Color::ToHSV() const
{
    float min, max;
    Bounds(&min, &max, true);

    float h = Hue(min, max);
    float s = SaturationHSV(min, max);
    float v = max;

    return Vector3(h, s, v);
}

void Color::FromHSL(float h, float s, float l, float a_)
{
    float c;
    if (l < 0.5f)
        c = (1.0f + (2.0f * l - 1.0f)) * s;
    else
        c = (1.0f - (2.0f * l - 1.0f)) * s;

    float m = l - 0.5f * c;

    FromHCM(h, c, m);

    a = a_;
}

void Color::FromHSV(float h, float s, float v, float a_)
{
    float c = v * s;
    float m = v - c;

    FromHCM(h, c, m);

    a = a_;
}

bool Color::FromString(const String& str)
{
    return FromString(str.CString());
}

bool Color::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 3)
        return false;
    
    char* ptr = (char*)str;
    r = (float)strtod(ptr, &ptr);
    g = (float)strtod(ptr, &ptr);
    b = (float)strtod(ptr, &ptr);
    if (elements > 3)
        a = (float)strtod(ptr, &ptr);
    
    return true;
}

float Color::Chroma() const
{
    float min, max;
    Bounds(&min, &max, true);

    return max - min;
}

float Color::Hue() const
{
    float min, max;
    Bounds(&min, &max, true);

    return Hue(min, max);
}

float Color::SaturationHSL() const
{
    float min, max;
    Bounds(&min, &max, true);

    return SaturationHSL(min, max);
}

float Color::SaturationHSV() const
{
    float min, max;
    Bounds(&min, &max, true);

    return SaturationHSV(min, max);
}

float Color::Lightness() const
{
    float min, max;
    Bounds(&min, &max, true);

    return (max + min) * 0.5f;
}

void Color::Bounds(float* min, float* max, bool clipped) const
{
    assert(min && max);

    if (r > g)
    {
        if (g > b) // r > g > b
        {
            *max = r;
            *min = b;
        }
        else // r > g && g <= b
        {
            *max = r > b ? r : b;
            *min = g;
        }
    }
    else
    {
        if (b > g) // r <= g < b
        {
            *max = b;
            *min = r;
        }
        else // r <= g && b <= g
        {
            *max = g;
            *min = r < b ? r : b;
        }
    }

    if (clipped)
    {
        *max = *max > 1.0f ? 1.0f : (*max < 0.0f ? 0.0f : *max);
        *min = *min > 1.0f ? 1.0f : (*min < 0.0f ? 0.0f : *min);
    }
}

float Color::MaxRGB() const
{
    if (r > g)
        return (r > b) ? r : b;
    else
        return (g > b) ? g : b;
}

float Color::MinRGB() const
{
    if (r < g)
        return (r < b) ? r : b;
    else
        return (g < b) ? g : b;
}

float Color::Range() const
{
    float min, max;
    Bounds(&min, &max);
    return max - min;
}

void Color::Clip(bool clipAlpha)
{
    r = (r > 1.0f) ? 1.0f : ((r < 0.0f) ? 0.0f : r);
    g = (g > 1.0f) ? 1.0f : ((g < 0.0f) ? 0.0f : g);
    b = (b > 1.0f) ? 1.0f : ((b < 0.0f) ? 0.0f : b);

    if (clipAlpha)
        a = (a > 1.0f) ? 1.0f : ((a < 0.0f) ? 0.0f : a);
}

void Color::Invert(bool invertAlpha)
{
    r = 1.0f - r;
    g = 1.0f - g;
    b = 1.0f - b;

    if (invertAlpha)
        a = 1.0f - a;
}

Color Color::Lerp(const Color &rhs, float t) const
{
    float invT = 1.0f - t;
    return Color(
        r * invT + rhs.r * t,
        g * invT + rhs.g * t,
        b * invT + rhs.b * t,
        a * invT + rhs.a * t
    );
}

String Color::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g", r, g, b, a);
    return String(tempBuffer);
}

float Color::Hue(float min, float max) const
{
    float chroma = max - min;

    // If chroma equals zero, hue is undefined
    if (chroma <= M_EPSILON)
        return 0.0f;

    // Calculate and return hue
    if (Turso3D::Equals(g, max))
        return (b + 2.0f*chroma - r) / (6.0f * chroma);
    else if (Turso3D::Equals(b, max))
        return (4.0f * chroma - g + r) / (6.0f * chroma);
    else
    {
        float h = (g - b) / (6.0f * chroma);
        return (h < 0.0f) ? 1.0f + h : ((h >= 1.0f) ? h - 1.0f : h);
    }

}

float Color::SaturationHSV(float min, float max) const
{
    // Avoid div-by-zero: result undefined
    if (max <= M_EPSILON)
        return 0.0f;

    // Saturation equals chroma:value ratio
    return 1.0f - (min / max);
}

float Color::SaturationHSL(float min, float max) const
{
    // Avoid div-by-zero: result undefined
    if (max <= M_EPSILON || min >= 1.0f - M_EPSILON)
        return 0.0f;

    // Chroma = max - min, lightness = (max + min) * 0.5
    float hl = (max + min);
    if (hl <= 1.0f)
        return (max - min) / hl;
    else
        return (min - max) / (hl - 2.0f);

}

void Color::FromHCM(float h, float c, float m)
{
    if (h < 0.0f || h >= 1.0f)
        h -= floorf(h);

    float hs = h * 6.0f;
    float x = c * (1.0f - Turso3D::Abs(fmodf(hs, 2.0f) - 1.0f));

    // Reconstruct r', g', b' from hue
    if (hs < 2.0f)
    {
        b = 0.0f;
        if (hs < 1.0f)
        {
            g = x;
            r = c;
        }
        else
        {
            g = c;
            r = x;
        }
    }
    else if (hs < 4.0f)
    {
        r = 0.0f;
        if (hs < 3.0f)
        {
            g = c;
            b = x;
        }
        else
        {
            g = x;
            b = c;
        }
    }
    else
    {
        g = 0.0f;
        if (hs < 5.0f)
        {
            r = x;
            b = c;
        }
        else
        {
            r = c;
            b = x;
        }
    }

    r += m;
    g += m;
    b += m;
}

}
