// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "Matrix4.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

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

bool Matrix4::FromString(const String& str)
{
    return FromString(str.CString());
}

bool Matrix4::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 16)
        return false;
    
    char* ptr = (char*)str;
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

void Matrix4::Decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const
{
    translation.x = m03;
    translation.y = m13;
    translation.z = m23;
    
    scale.x = sqrtf(m00 * m00 + m10 * m10 + m20 * m20);
    scale.y = sqrtf(m01 * m01 + m11 * m11 + m21 * m21);
    scale.z = sqrtf(m02 * m02 + m12 * m12 + m22 * m22);
    
    Vector3 invScale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
    rotation = Quaternion(ToMatrix3().Scaled(invScale));
}

Matrix4 Matrix4::Inverse() const
{
    float v0 = m20 * m31 - m21 * m30;
    float v1 = m20 * m32 - m22 * m30;
    float v2 = m20 * m33 - m23 * m30;
    float v3 = m21 * m32 - m22 * m31;
    float v4 = m21 * m33 - m23 * m31;
    float v5 = m22 * m33 - m23 * m32;
    
    float i00 = (v5 * m11 - v4 * m12 + v3 * m13);
    float i10 = -(v5 * m10 - v2 * m12 + v1 * m13);
    float i20 = (v4 * m10 - v2 * m11 + v0 * m13);
    float i30 = -(v3 * m10 - v1 * m11 + v0 * m12);
    
    float invDet = 1.0f / (i00 * m00 + i10 * m01 + i20 * m02 + i30 * m03);
    
    i00 *= invDet;
    i10 *= invDet;
    i20 *= invDet;
    i30 *= invDet;
    
    float i01 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    float i11 = (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    float i21 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    float i31 = (v3 * m00 - v1 * m01 + v0 * m02) * invDet;
    
    v0 = m10 * m31 - m11 * m30;
    v1 = m10 * m32 - m12 * m30;
    v2 = m10 * m33 - m13 * m30;
    v3 = m11 * m32 - m12 * m31;
    v4 = m11 * m33 - m13 * m31;
    v5 = m12 * m33 - m13 * m32;
    
    float i02 = (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    float i12 = -(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    float i22 = (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    float i32 = -(v3 * m00 - v1 * m01 + v0 * m02) * invDet;
    
    v0 = m21 * m10 - m20 * m11;
    v1 = m22 * m10 - m20 * m12;
    v2 = m23 * m10 - m20 * m13;
    v3 = m22 * m11 - m21 * m12;
    v4 = m23 * m11 - m21 * m13;
    v5 = m23 * m12 - m22 * m13;
    
    float i03 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    float i13 = (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    float i23 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    float i33 = (v3 * m00 - v1 * m01 + v0 * m02) * invDet;
    
    return Matrix4(
        i00, i01, i02, i03,
        i10, i11, i12, i13,
        i20, i21, i22, i23,
        i30, i31, i32, i33
    );
}

String Matrix4::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g", m00, m01, m02, m03, m10, m11, m12, m13, m20,
        m21, m22, m23, m30, m31, m32, m33);
    return String(tempBuffer);
}

}
