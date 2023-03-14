 // For conditions of distribution and use, see copyright notice in License.txt

#include "BoundingBox.h"
#include "Frustum.h"
#include "Polyhedron.h"
#include "Sphere.h"

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
    for (size_t i = 0; i < poly.faces.size(); ++i)
    {
        const std::vector<Vector3>& face = poly.faces[i];
        if (!face.empty())
            Merge(&face[0], face.size());
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

Vector3 Sphere::LocalPoint(float theta, float phi) const
{
    return Vector3(
        radius * Sin(theta) * Sin(phi),
        radius * Cos(phi),
        radius * Cos(theta) * Sin(phi)
    );
}
