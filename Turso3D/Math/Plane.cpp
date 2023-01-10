// For conditions of distribution and use, see copyright notice in License.txt

#include "Plane.h"

// Static initialization order can not be relied on, so do not use Vector3 constants
const Plane Plane::UP(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));

void Plane::Transform(const Matrix3& transform)
{
    Define(Matrix4(transform).Inverse().Transpose() * ToVector4());
}

void Plane::Transform(const Matrix3x4& transform)
{
    Define(transform.ToMatrix4().Inverse().Transpose() * ToVector4());
}

void Plane::Transform(const Matrix4& transform)
{
    Define(transform.Inverse().Transpose() * ToVector4());
}

Matrix3x4 Plane::ReflectionMatrix() const
{
    return Matrix3x4(
        -2.0f * normal.x * normal.x + 1.0f,
        -2.0f * normal.x * normal.y,
        -2.0f * normal.x * normal.z,
        -2.0f * normal.x * d,
        -2.0f * normal.y * normal.x ,
        -2.0f * normal.y * normal.y + 1.0f,
        -2.0f * normal.y * normal.z,
        -2.0f * normal.y * d,
        -2.0f * normal.z * normal.x,
        -2.0f * normal.z * normal.y,
        -2.0f * normal.z * normal.z + 1.0f,
        -2.0f * normal.z * d
    );
}

Plane Plane::Transformed(const Matrix3& transform) const
{
    return Plane(Matrix4(transform).Inverse().Transpose() * ToVector4());
}

Plane Plane::Transformed(const Matrix3x4& transform) const
{
    return Plane(transform.ToMatrix4().Inverse().Transpose() * ToVector4());
}

Plane Plane::Transformed(const Matrix4& transform) const
{
    return Plane(transform.Inverse().Transpose() * ToVector4());
}
