// For conditions of distribution and use, see copyright notice in License.txt

#include "Quaternion.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const Quaternion Quaternion::IDENTITY(1.0f, 0.0f, 0.0f, 0.0f);

void Quaternion::FromAngleAxis(float angle, const Vector3& axis)
{
    Vector3 normAxis = axis.Normalized();
    angle *= M_DEGTORAD_2;
    float sinAngle = sinf(angle);
    float cosAngle = cosf(angle);
    
    w = cosAngle;
    x = normAxis.x * sinAngle;
    y = normAxis.y * sinAngle;
    z = normAxis.z * sinAngle;
}

void Quaternion::FromEulerAngles(float x_, float y_, float z_)
{
    // Order of rotations: Z first, then X, then Y (mimics typical FPS camera with gimbal lock at top/bottom)
    x_ *= M_DEGTORAD_2;
    y_ *= M_DEGTORAD_2;
    z_ *= M_DEGTORAD_2;
    float sinX = sinf(x_);
    float cosX = cosf(x_);
    float sinY = sinf(y_);
    float cosY = cosf(y_);
    float sinZ = sinf(z_);
    float cosZ = cosf(z_);
    
    w = cosY * cosX * cosZ + sinY * sinX * sinZ;
    x = cosY * sinX * cosZ + sinY * cosX * sinZ;
    y = sinY * cosX * cosZ - cosY * sinX * sinZ;
    z = cosY * cosX * sinZ - sinY * sinX * cosZ;
}

void Quaternion::FromRotationTo(const Vector3& start, const Vector3& end)
{
    Vector3 normStart = start.Normalized();
    Vector3 normEnd = end.Normalized();
    float d = normStart.DotProduct(normEnd);
    
    if (d > -1.0f + M_EPSILON)
    {
        Vector3 c = normStart.CrossProduct(normEnd);
        float s = sqrtf((1.0f + d) * 2.0f);
        float invS = 1.0f / s;
        
        x = c.x * invS;
        y = c.y * invS;
        z = c.z * invS;
        w = 0.5f * s;
    }
    else
    {
        Vector3 axis = Vector3::RIGHT.CrossProduct(normStart);
        if (axis.Length() < M_EPSILON)
            axis = Vector3::UP.CrossProduct(normStart);
        
        FromAngleAxis(180.f, axis);
    }
}

void Quaternion::FromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
{
    Matrix3 matrix(
        xAxis.x, yAxis.x, zAxis.x,
        xAxis.y, yAxis.y, zAxis.y,
        xAxis.z, yAxis.z, zAxis.z
    );
    
    FromRotationMatrix(matrix);
}

void Quaternion::FromRotationMatrix(const Matrix3& matrix)
{
    float t = matrix.m00 + matrix.m11 + matrix.m22;
    
    if (t > 0.0f)
    {
        float invS = 0.5f / sqrtf(1.0f + t);
        
        x = (matrix.m21 - matrix.m12) * invS;
        y = (matrix.m02 - matrix.m20) * invS;
        z = (matrix.m10 - matrix.m01) * invS;
        w = 0.25f / invS;
    }
    else
    {
        if (matrix.m00 > matrix.m11 && matrix.m00 > matrix.m22)
        {
            float invS = 0.5f / sqrtf(1.0f + matrix.m00 - matrix.m11 - matrix.m22);
            
            x = 0.25f / invS;
            y = (matrix.m01 + matrix.m10) * invS;
            z = (matrix.m20 + matrix.m02) * invS;
            w = (matrix.m21 - matrix.m12) * invS;
        }
        else if (matrix.m11 > matrix.m22)
        {
            float invS = 0.5f / sqrtf(1.0f + matrix.m11 - matrix.m00 - matrix.m22);
            
            x = (matrix.m01 + matrix.m10) * invS;
            y = 0.25f / invS;
            z = (matrix.m12 + matrix.m21) * invS;
            w = (matrix.m02 - matrix.m20) * invS;
        }
        else
        {
            float invS = 0.5f / sqrtf(1.0f + matrix.m22 - matrix.m00 - matrix.m11);
            
            x = (matrix.m02 + matrix.m20) * invS;
            y = (matrix.m12 + matrix.m21) * invS;
            z = 0.25f / invS;
            w = (matrix.m10 - matrix.m01) * invS;
        }
    }
}

bool Quaternion::FromLookRotation(const Vector3& direction, const Vector3& upDirection)
{
    Quaternion ret;
    Vector3 forward = direction.Normalized();

    Vector3 v = forward.CrossProduct(upDirection);
    // If direction & upDirection are parallel and crossproduct becomes zero, use FromRotationTo() fallback
    if (v.LengthSquared() >= M_EPSILON)
    {
        v.Normalize();
        Vector3 up = v.CrossProduct(forward);
        Vector3 right = up.CrossProduct(forward);
        ret.FromAxes(right, up, forward);
    }
    else
        ret.FromRotationTo(Vector3::FORWARD, forward);

    if (!ret.IsNaN())
    {
        (*this) = ret;
        return true;
    }
    else
        return false;
}

bool Quaternion::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 3)
        return false;

    char* ptr = const_cast<char*>(string);
    if (elements >= 4)
    {
        w = (float)strtod(ptr, &ptr);
        x = (float)strtod(ptr, &ptr);
        y = (float)strtod(ptr, &ptr);
        z = (float)strtod(ptr, &ptr);
    }
    else
    {
        float x_, y_, z_;
        x_ = (float)strtod(ptr, &ptr);
        y_ = (float)strtod(ptr, &ptr);
        z_ = (float)strtod(ptr, &ptr);
        FromEulerAngles(x_, y_, z_);
    }

    return true;
}

Vector3 Quaternion::EulerAngles() const
{
    // Derivation from http://www.geometrictools.com/Documentation/EulerAngles.pdf
    // Order of rotations: Z first, then X, then Y
    float check = 2.0f * (-y * z + w * x);
    
    if (check < -0.995f)
    {
        return Vector3(
            -90.0f,
            0.0f,
            -atan2f(2.0f * (x * z - w * y), 1.0f - 2.0f * (y * y + z * z)) * M_RADTODEG
        );
    }
    else if (check > 0.995f)
    {
        return Vector3(
            90.0f,
            0.0f,
            atan2f(2.0f * (x * z - w * y), 1.0f - 2.0f * (y * y + z * z)) * M_RADTODEG
        );
    }
    else
    {
        return Vector3(
            asinf(check) * M_RADTODEG,
            atan2f(2.0f * (x * z + w * y), 1.0f - 2.0f * (x * x + y * y)) * M_RADTODEG,
            atan2f(2.0f * (x * y + w * z), 1.0f - 2.0f * (x * x + z * z)) * M_RADTODEG
        );
    }
}

float Quaternion::YawAngle() const
{
    return EulerAngles().y;
}

float Quaternion::PitchAngle() const
{
    return EulerAngles().x;
}

float Quaternion::RollAngle() const
{
    return EulerAngles().z;
}

Quaternion Quaternion::Slerp(Quaternion rhs, float t) const
{
    float cosAngle = DotProduct(rhs);
    // Enable shortest path rotation
    if (cosAngle < 0.0f)
    {
        cosAngle = -cosAngle;
        rhs = -rhs;
    }
    
    float angle = acosf(cosAngle);
    float sinAngle = sinf(angle);
    float t1, t2;
    
    if (sinAngle > 0.001f)
    {
        float invSinAngle = 1.0f / sinAngle;
        t1 = sinf((1.0f - t) * angle) * invSinAngle;
        t2 = sinf(t * angle) * invSinAngle;
    }
    else
    {
        t1 = 1.0f - t;
        t2 = t;
    }
    
    return *this * t1 + rhs * t2;
}

Quaternion Quaternion::Nlerp(Quaternion rhs, float t, bool shortestPath) const
{
    Quaternion result;
    float fCos = DotProduct(rhs);
    if (fCos < 0.0f && shortestPath)
        result = (*this) + (((-rhs) - (*this)) * t);
    else
        result = (*this) + ((rhs - (*this)) * t);
    result.Normalize();
    return result;
}

std::string Quaternion::ToString() const
{
    return FormatString("%g %g %g %g", w, x, y, z);
}
