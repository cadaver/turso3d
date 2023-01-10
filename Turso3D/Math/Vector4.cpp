// For conditions of distribution and use, see copyright notice in License.txt

#include "Vector4.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const Vector4 Vector4::ZERO(0.0f, 0.0f, 0.0f, 0.0f);
const Vector4 Vector4::ONE(1.0f, 1.0f, 1.0f, 1.0f);

bool Vector4::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 4)
        return false;

    char* ptr = const_cast<char*>(string);
    x = (float)strtod(ptr, &ptr);
    y = (float)strtod(ptr, &ptr);
    z = (float)strtod(ptr, &ptr);
    w = (float)strtod(ptr, &ptr);
    
    return true;
}

std::string Vector4::ToString() const
{
    return FormatString("%g %g %g %g", x, y, z, w);
}
