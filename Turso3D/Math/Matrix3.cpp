// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "Matrix3.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const Matrix3 Matrix3::ZERO(
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f);

const Matrix3 Matrix3::IDENTITY;

bool Matrix3::FromString(const String& str)
{
    return FromString(str.CString());
}

bool Matrix3::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 9)
        return false;
    
    char* ptr = (char*)str;
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

String Matrix3::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g %g %g %g %g %g", m00, m01, m02, m10, m11, m12, m20, m21, m22);
    return String(tempBuffer);
}

}
