// For conditions of distribution and use, see copyright notice in License.txt

#include "Matrix3x4.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const Matrix3x4 Matrix3x4::ZERO(
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f);

const Matrix3x4 Matrix3x4::IDENTITY(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f);

bool Matrix3x4::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 12)
        return false;

    char* ptr = const_cast<char*>(string);
    m00 = (float)strtod(ptr, &ptr);
    m01 = (float)strtod(ptr, &ptr);
    m02 = (float)strtod(ptr, &ptr);
    m03 = (float)strtod(ptr, &ptr);
    m10 = (float)strtod(ptr, &ptr);
    m11 = (float)strtod(ptr, &ptr);
    m12 = (float)strtod(ptr, &ptr);
    m13 = (float)strtod(ptr, &ptr);
    m20 = (float)strtod(ptr, &ptr);
    m21 = (float)strtod(ptr, &ptr);
    m22 = (float)strtod(ptr, &ptr);
    m23 = (float)strtod(ptr, &ptr);
    
    return true;
}

std::string Matrix3x4::ToString() const
{
    return FormatString("%g %g %g %g %g %g %g %g %g %g %g %g", 
        m00, m01, m02, m03, 
        m10, m11, m12, m13, 
        m20, m21, m22, m23);
}
