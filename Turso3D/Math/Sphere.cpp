 // For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "BoundingBox.h"
#include "Frustum.h"
#include "Polyhedron.h"
#include "Sphere.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

void Sphere::Define(const Vector3* vertices, size_t count)
{
    Undefine();
    Merge(vertices, count);
}

void Sphere::Define(const BoundingBox& box)
{
    const Vector3& min = box.min;
    const Vector3& max = box.max;
    
    Undefine();
    Merge(min);
    Merge(Vector3(max.x, min.y, min.z));
    Merge(Vector3(min.x, max.y, min.z));
    Merge(Vector3(max.x, max.y, min.z));
    Merge(Vector3(min.x, min.y, max.z));
    Merge(Vector3(max.x, min.y, max.z));
    Merge(Vector3(min.x, max.y, max.z));
    Merge(max);
}

void Sphere::Define(const Frustum& frustum)
{
    Define(frustum.vertices, NUM_FRUSTUM_VERTICES);
}

void Sphere::Define(const Polyhedron& poly)
{
    Undefine();
    Merge(poly);
}

void Sphere::Merge(const Vector3* vertices, size_t count)
{
    while (count--)
        Merge(*vertices++);
}

void Sphere::Merge(const BoundingBox& box)
{
    const Vector3& min = box.min;
    const Vector3& max = box.max;
    
    Merge(min);
    Merge(Vector3(max.x, min.y, min.z));
    Merge(Vector3(min.x, max.y, min.z));
    Merge(Vector3(max.x, max.y, min.z));
    Merge(Vector3(min.x, min.y, max.z));
    Merge(Vector3(max.x, min.y, max.z));
    Merge(Vector3(min.x, max.y, max.z));
    Merge(max);
}

void Sphere::Merge(const Frustum& frustum)
{
    const Vector3* vertices = frustum.vertices;
    Merge(vertices, NUM_FRUSTUM_VERTICES);
}

void Sphere::Merge(const Polyhedron& poly)
{
    for (size_t i = 0; i < poly.faces.Size(); ++i)
    {
        const Vector<Vector3>& face = poly.faces[i];
        if (!face.IsEmpty())
            Merge(&face[0], face.Size());
    }
}

void Sphere::Merge(const Sphere& sphere)
{
    // If undefined, set initial dimensions
    if (!IsDefined())
    {
        center = sphere.center;
        radius = sphere.radius;
        return;
    }
    
    Vector3 offset = sphere.center - center;
    float dist = offset.Length();
    
    // If sphere fits inside, do nothing
    if (dist + sphere.radius < radius)
        return;
    
    // If we fit inside the other sphere, become it
    if (dist + radius < sphere.radius)
    {
        center = sphere.center;
        radius = sphere.radius;
    }
    else
    {
        Vector3 normalizedOffset = offset / dist;
        
        Vector3 min = center - radius * normalizedOffset;
        Vector3 max = sphere.center + sphere.radius * normalizedOffset;
        center = (min + max) * 0.5f;
        radius = (max - center).Length();
    }
}

Intersection Sphere::IsInside(const BoundingBox& box) const
{
    float radiusSquared = radius * radius;
    float distSquared = 0;
    float temp;
    Vector3 min = box.min;
    Vector3 max = box.max;
    
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
    
    if (distSquared >= radiusSquared)
        return OUTSIDE;
    
    min -= center;
    max -= center;
    
    Vector3 tempVec = min; // - - -
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    tempVec.x = max.x; // + - -
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    tempVec.y = max.y; // + + -
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    tempVec.x = min.x; // - + -
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    tempVec.z = max.z; // - + +
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    tempVec.y = min.y; // - - +
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    tempVec.x = max.x; // + - +
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    tempVec.y = max.y; // + + +
    if (tempVec.LengthSquared() >= radiusSquared)
        return INTERSECTS;
    
    return INSIDE;
}

Intersection Sphere::IsInsideFast(const BoundingBox& box) const
{
    float radiusSquared = radius * radius;
    float distSquared = 0;
    float temp;
    Vector3 min = box.min;
    Vector3 max = box.max;
    
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
    
    if (distSquared >= radiusSquared)
        return OUTSIDE;
    else
        return INSIDE;
}

}
