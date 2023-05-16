// For conditions of distribution and use, see copyright notice in License.txt

#include "BoundingBox.h"
#include "Frustum.h"
#include "Polyhedron.h"
#include "../IO/StringUtils.h"

#include <utility>
#include <cstdlib>

void BoundingBox::Define(const Vector3* vertices, size_t count)
{
    Undefine();
    Merge(vertices, count);
}

void BoundingBox::Define(const Frustum& frustum)
{
    Define(frustum.vertices, NUM_FRUSTUM_VERTICES);
}

void BoundingBox::Define(const Polyhedron& poly)
{
    Undefine();
    Merge(poly);
}

void BoundingBox::Define(const Sphere& sphere)
{
    const Vector3& center = sphere.center;
    float radius = sphere.radius;
    
    min = center + Vector3(-radius, -radius, -radius);
    max = center + Vector3(radius, radius, radius);
}

void BoundingBox::Merge(const Vector3* vertices, size_t count)
{
    while (count--)
        Merge(*vertices++);
}

void BoundingBox::Merge(const Frustum& frustum)
{
    Merge(frustum.vertices, NUM_FRUSTUM_VERTICES);
}

void BoundingBox::Merge(const Polyhedron& poly)
{
    for (size_t i = 0; i < poly.faces.size(); ++i)
    {
        const std::vector<Vector3>& face = poly.faces[i];
        if (!face.empty())
            Merge(&face[0], face.size());
    }
}

void BoundingBox::Merge(const Sphere& sphere)
{
    const Vector3& center = sphere.center;
    float radius = sphere.radius;
    
    Merge(center + Vector3(radius, radius, radius));
    Merge(center + Vector3(-radius, -radius, -radius));
}

void BoundingBox::Clip(const BoundingBox& box)
{
    if (box.min.x > min.x)
        min.x = box.min.x;
    if (box.max.x < max.x)
        max.x = box.max.x;
    if (box.min.y > min.y)
        min.y = box.min.y;
    if (box.max.y < max.y)
        max.y = box.max.y;
    if (box.min.z > min.z)
        min.z = box.min.z;
    if (box.max.z < max.z)
        max.z = box.max.z;
    
    if (min.x > max.x)
        std::swap(min.x, max.x);
    if (min.y > max.y)
        std::swap(min.y, max.y);
    if (min.z > max.z)
        std::swap(min.z, max.z);
}

void BoundingBox::Transform(const Matrix3& transform)
{
    *this = Transformed(Matrix3x4(transform));
}

void BoundingBox::Transform(const Matrix3x4& transform)
{
    *this = Transformed(transform);
}

bool BoundingBox::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 6)
        return false;

    char* ptr = const_cast<char*>(string);
    min.x = (float)strtod(ptr, &ptr);
    min.y = (float)strtod(ptr, &ptr);
    min.z = (float)strtod(ptr, &ptr);
    max.x = (float)strtod(ptr, &ptr);
    max.y = (float)strtod(ptr, &ptr);
    max.z = (float)strtod(ptr, &ptr);
    
    return true;
}

BoundingBox BoundingBox::Transformed(const Matrix3& transform) const
{
    return Transformed(Matrix3x4(transform));
}

BoundingBox BoundingBox::Transformed(const Matrix3x4& transform) const
{
    Vector3 oldCenter = Center();
    Vector3 oldEdge = max - oldCenter;
    Vector3 newCenter = transform * oldCenter;
    Vector3 newEdge(
        Abs(transform.m00) * oldEdge.x + Abs(transform.m01) * oldEdge.y + Abs(transform.m02) * oldEdge.z,
        Abs(transform.m10) * oldEdge.x + Abs(transform.m11) * oldEdge.y + Abs(transform.m12) * oldEdge.z,
        Abs(transform.m20) * oldEdge.x + Abs(transform.m21) * oldEdge.y + Abs(transform.m22) * oldEdge.z
    );
    
    return BoundingBox(newCenter - newEdge, newCenter + newEdge);
}

Rect BoundingBox::Projected(const Matrix4& projection) const
{
    Vector3 projMin = min;
    Vector3 projMax = max;
    if (projMin.z < M_EPSILON)
        projMin.z = M_EPSILON;
    if (projMax.z < M_EPSILON)
        projMax.z = M_EPSILON;
    
    Vector3 vertices[8];
    vertices[0] = projMin;
    vertices[1] = Vector3(projMax.x, projMin.y, projMin.z);
    vertices[2] = Vector3(projMin.x, projMax.y, projMin.z);
    vertices[3] = Vector3(projMax.x, projMax.y, projMin.z);
    vertices[4] = Vector3(projMin.x, projMin.y, projMax.z);
    vertices[5] = Vector3(projMax.x, projMin.y, projMax.z);
    vertices[6] = Vector3(projMin.x, projMax.y, projMax.z);
    vertices[7] = projMax;
    
    Rect rect;
    for (size_t i = 0; i < 8; ++i)
    {
        Vector3 projected = projection * vertices[i];
        rect.Merge(Vector2(projected.x, projected.y));
    }
    
    return rect;
}

Intersection BoundingBox::IsInside(const Sphere& sphere) const
{
    float distSquared = 0;
    float temp;
    const Vector3& center = sphere.center;
    
    if (center.x < min.x)
    {
        temp = center.x - min.x;
        distSquared += temp * temp;
    }
    else if (center.x > max.x)
    {
        temp = center.x - max.x;
        distSquared += temp * temp;
    }
    if (center.y < min.y)
    {
        temp = center.y - min.y;
        distSquared += temp * temp;
    }
    else if (center.y > max.y)
    {
        temp = center.y - max.y;
        distSquared += temp * temp;
    }
    if (center.z < min.z)
    {
        temp = center.z - min.z;
        distSquared += temp * temp;
    }
    else if (center.z > max.z)
    {
        temp = center.z - max.z;
        distSquared += temp * temp;
    }
    
    float radius = sphere.radius;
    if (distSquared >= radius * radius)
        return OUTSIDE;
    else if (center.x - radius < min.x || center.x + radius > max.x || center.y - radius < min.y ||
        center.y + radius > max.y || center.z - radius < min.z || center.z + radius > max.z)
        return INTERSECTS;
    else
        return INSIDE;
}

Intersection BoundingBox::IsInsideFast(const Sphere& sphere) const
{
    float distSquared = 0;
    float temp;
    const Vector3& center = sphere.center;
    
    if (center.x < min.x)
    {
        temp = center.x - min.x;
        distSquared += temp * temp;
    }
    else if (center.x > max.x)
    {
        temp = center.x - max.x;
        distSquared += temp * temp;
    }
    if (center.y < min.y)
    {
        temp = center.y - min.y;
        distSquared += temp * temp;
    }
    else if (center.y > max.y)
    {
        temp = center.y - max.y;
        distSquared += temp * temp;
    }
    if (center.z < min.z)
    {
        temp = center.z - min.z;
        distSquared += temp * temp;
    }
    else if (center.z > max.z)
    {
        temp = center.z - max.z;
        distSquared += temp * temp;
    }
    
    float radius = sphere.radius;
    if (distSquared >= radius * radius)
        return OUTSIDE;
    else
        return INSIDE;
}


std::string BoundingBox::ToString() const
{
    return min.ToString() + " " + max.ToString();
}
