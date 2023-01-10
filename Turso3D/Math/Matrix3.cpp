// For conditions of distribution and use, see copyright notice in License.txt

#include "Matrix3.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const Matrix3 Matrix3::ZERO(
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f);

const Matrix3 Matrix3::IDENTITY(
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f);

bool Matrix3::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 9)
        return false;

    char* ptr = const_cast<char*>(string);
    m00 = (float)strtod(ptr, &ptr);
    m01 = (float)strtod(ptr, &ptr);
    m02 = (float)strtod(ptr, &ptr);
    m10 = (float)strtod(ptr, &ptr);
    m11 = (float)strtod(ptr, &ptr);
    m12 = (float)strtod(ptr, &ptr);
    m20 = (float)strtod(ptr, &ptr);
    m21 = (float)strtod(ptr, &ptr);
    m22 = (float)strtod(ptr, &ptr);
    
    return true;
}

Matrix3 Matrix3::Inverse() const
{
    float det = m00 * m11 * m22 +
        m10 * m21 * m02 +
        m20 * m01 * m12 -
        m20 * m11 * m02 -
        m10 * m01 * m22 -
        m00 * m21 * m12;
    
    float invDet = 1.0f / det;
    
    return Matrix3(
        (m11 * m22 - m21 * m12) * invDet,
        -(m01 * m22 - m21 * m02) * invDet,
        (m01 * m12 - m11 * m02) * invDet,
        -(m10 * m22 - m20 * m12) * invDet,
        (m00 * m22 - m20 * m02) * invDet,
        -(m00 * m12 - m10 * m02) * invDet,
        (m10 * m21 - m20 * m11) * invDet,
        -(m00 * m21 - m20 * m01) * invDet,
        (m00 * m11 - m10 * m01) * invDet
    );
}

std::string Matrix3::ToString() const
{
    return FormatString("%g %g %g %g %g %g %g %g %g", 
        m00, m01, m02,
        m10, m11, m12,
        m20, m21, m22);
}
