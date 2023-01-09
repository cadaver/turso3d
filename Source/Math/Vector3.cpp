// For conditions of distribution and use, see copyright notice in License.txt

#include "Vector3.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const Vector3 Vector3::ZERO(0.0f, 0.0f, 0.0f);
const Vector3 Vector3::LEFT(-1.0f, 0.0f, 0.0f);
const Vector3 Vector3::RIGHT(1.0f, 0.0f, 0.0f);
const Vector3 Vector3::UP(0.0f, 1.0f, 0.0f);
const Vector3 Vector3::DOWN(0.0f, -1.0f, 0.0f);
const Vector3 Vector3::FORWARD(0.0f, 0.0f, 1.0f);
const Vector3 Vector3::BACK(0.0f, 0.0f, -1.0f);
const Vector3 Vector3::ONE(1.0f, 1.0f, 1.0f);

bool Vector3::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 3)
        return false;

    char* ptr = const_cast<char*>(string);
    x = (float)strtod(ptr, &ptr);
    y = (float)strtod(ptr, &ptr);
    z = (float)strtod(ptr, &ptr);
    
    return true;
}

std::string Vector3::ToString() const
{
    return FormatString("%g %g %g", x, y, z);
}
