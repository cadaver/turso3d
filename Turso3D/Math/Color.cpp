// For conditions of distribution and use, see copyright notice in License.txt

#include "Color.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const Color Color::WHITE(1.0f, 1.0f, 1.0f);
const Color Color::GRAY(0.5f, 0.5f, 0.5f);
const Color Color::BLACK(0.0f, 0.0f, 0.0f);
const Color Color::RED(1.0f, 0.0f, 0.0f);
const Color Color::GREEN(0.0f, 1.0f, 0.0f);
const Color Color::BLUE(0.0f, 0.0f, 1.0f);
const Color Color::CYAN(0.0f, 1.0f, 1.0f);
const Color Color::MAGENTA(1.0f, 0.0f, 1.0f);
const Color Color::YELLOW(1.0f, 1.0f, 0.0f);
const Color Color::TRANSPARENT(0.0f, 0.0f, 0.0f, 0.0f);

unsigned Color::ToUInt() const
{
    unsigned r_ = Clamp(((int)(r * 255.0f)), 0, 255);
    unsigned g_ = Clamp(((int)(g * 255.0f)), 0, 255);
    unsigned b_ = Clamp(((int)(b * 255.0f)), 0, 255);
    unsigned a_ = Clamp(((int)(a * 255.0f)), 0, 255);
    return (a_ << 24) | (b_ << 16) | (g_ << 8) | r_;
}

bool Color::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 3)
        return false;
    char* ptr = const_cast<char*>(string);
    r = (float)strtod(ptr, &ptr);
    g = (float)strtod(ptr, &ptr);
    b = (float)strtod(ptr, &ptr);
    if (elements > 3)
        a = (float)strtod(ptr, &ptr);
    else
        a = 1.0f;
    
    return true;
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

std::string Color::ToString() const
{
    return FormatString("%g %g %g %g", r, g, b, a);
}

Color Color::FromHSV(float h, float s, float v, float a)
{
    while (h < 0.0f)
		h += 360.0f;
    while (h >= 360.0f)
        h -= 360.0f;

    s = Clamp(s, 0.0f, 1.0f);
	v = Clamp(v, 0.0f, 1.0f);

    if (s == 0.0f)
        return Color(v, v, v, a);

	float sector = h / 60.0f;
	int i = (int)sector;

	float f = sector - i;

	float p = v * (1.0f - s);
	float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (i)
    {
    case 0:
        return Color(v, t, p, a);
    case 1:
        return Color(q, v, p, a);
    case 2:
        return Color(p, v, t, a);
    case 3:
        return Color(p, q, v, a);
    case 4:
        return Color(t, p, v, a);
    default:
        return Color(v, p, q, a);
    }
}

Vector3 Color::ToHSV() const
{
    return Vector3();
}

Color Color::BlendPremultiplied(const Color& rhs) const
{
    return Color();
}

Color Color::GammaToLinear() const
{
    return Color();
}

Color Color::LinearToGamma() const
{
    return Color();
}
