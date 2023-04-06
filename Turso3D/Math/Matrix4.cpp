// For conditions of distribution and use, see copyright notice in License.txt

#include "Matrix3x4.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const Matrix4 Matrix4::ZERO(
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f);

const Matrix4 Matrix4::IDENTITY(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

Matrix4::Matrix4(const Matrix3x4& matrix) :
    m00(matrix.m00), m01(matrix.m01), m02(matrix.m02), m03(matrix.m03),
    m10(matrix.m10), m11(matrix.m11), m12(matrix.m12), m13(matrix.m13),
    m20(matrix.m20), m21(matrix.m21), m22(matrix.m22), m23(matrix.m23),
    m30(0.0f), m31(0.0f), m32(0.0f), m33(1.0f)
{
}

bool Matrix4::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 16)
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
    m30 = (float)strtod(ptr, &ptr);
    m31 = (float)strtod(ptr, &ptr);
    m32 = (float)strtod(ptr, &ptr);
    m33 = (float)strtod(ptr, &ptr);
    
    return true;
}

std::string Matrix4::ToString() const
{
    return FormatString("%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g", 
        m00, m01, m02, m03,
        m10, m11, m12, m13,
        m20, m21, m22, m23,
        m30, m31, m32, m33);
}
