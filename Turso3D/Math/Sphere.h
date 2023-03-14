// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "BoundingBox.h"

class Polyhedron;
class Frustum;

/// %Sphere in three-dimensional space.
class Sphere
{
public:
    /// Sphere center.
    Vector3 center;
    /// Sphere radius.
    float radius;
    
    /// Construct as undefined (negative radius.)
    Sphere() :
        center(Vector3::ZERO),
        radius(-M_INFINITY)
    {
    }
    
    /// Copy-construct.
    Sphere(const Sphere& sphere) :
        center(sphere.center),
        radius(sphere.radius)
    {
    }
    
    /// Construct from center and radius.
    Sphere(const Vector3& center_, float radius_) :
        center(center_),
        radius(radius_)
    {
    }
    
    /// Construct from an array of vertices.
    Sphere(const Vector3* vertices, size_t count)
    {
        Define(vertices, count);
    }
    
    /// Construct from a bounding box.
    Sphere(const BoundingBox& box)
    {
        Define(box);
    }
    
    /// Construct from a frustum.
    Sphere(const Frustum& frustum)
    {
        Define(frustum);
    }
    
    /// Construct from a polyhedron.
    Sphere(const Polyhedron& poly)
    {
        Define(poly);
    }
    
    /// Assign from another sphere.
    Sphere& operator = (const Sphere& rhs)
    {
        center = rhs.center;
        radius = rhs.radius;
        return *this;
    }
    
    /// Test for equality with another sphere without epsilon.
    bool operator == (const Sphere& rhs) const { return center == rhs.center && radius == rhs.radius; }
    /// Test for inequality with another sphere without epsilon.
    bool operator != (const Sphere& rhs) const { return !(*this == rhs); }
    
    /// Define from another sphere.
    void Define(const Sphere& sphere)
    {
        center = sphere.center;
        radius = sphere.radius;
    }
    
    /// Define from center and radius.
    void Define(const Vector3& center_, float radius_)
    {
        center = center_;
        radius = radius_;
    }
    
    /// Define from an array of vertices.
    void Define(const Vector3* vertices, size_t count);
    /// Define from a bounding box.
    void Define(const BoundingBox& box);
    /// Define from a frustum.
    void Define(const Frustum& frustum);
    /// Define from a polyhedron.
    void Define(const Polyhedron& poly);
    
    /// Merge a point.
    void Merge(const Vector3& point)
    {
        // If undefined, set initial dimensions
        if (!IsDefined())
        {
            center = point;
            radius = 0.0f;
            return;
        }
        
        Vector3 offset = point - center;
        float dist = offset.Length();
        
        if (dist > radius)
        {
            float half = (dist - radius) * 0.5f;
            radius += half;
            center += (half / dist) * offset;
        }
    }
    
    /// Set as undefined to allow the next merge to set initial size.
    void Undefine()
    {
        radius = -M_INFINITY;
    }
    
    /// Merge an array of vertices.
    void Merge(const Vector3* vertices, size_t count);
    /// Merge a bounding box.
    void Merge(const BoundingBox& box);
    /// Merge a frustum.
    void Merge(const Frustum& frustum);
    /// Merge a polyhedron.
    void Merge(const Polyhedron& poly);
    /// Merge a sphere.
    void Merge(const Sphere& sphere);
    
    /// Return whether has non-negative radius.
    bool IsDefined() const { return radius >= 0.0f; }

    /// Return local-space point on the sphere.
    Vector3 LocalPoint(float theta, float phi) const;
    /// Return world-space point on the sphere.
    Vector3 Point(float theta, float phi) const { return center + LocalPoint(theta, phi); }

    /// Test if a point is inside.
    Intersection IsInside(const Vector3& point) const
    {
        float distSquared = (point - center).LengthSquared();

        return distSquared >= radius * radius ? OUTSIDE : INSIDE;
    }
    
    /// Test if another sphere is inside, outside or intersects.
    Intersection IsInside(const Sphere& sphere) const
    {
        float dist = (sphere.center - center).Length();
        if (dist >= sphere.radius + radius)
            return OUTSIDE;
        else if (dist + sphere.radius < radius)
            return INSIDE;
        else
            return INTERSECTS;
    }
    
    /// Test if another sphere is (partially) inside or outside.
    Intersection IsInsideFast(const Sphere& sphere) const
    {
        float distSquared = (sphere.center - center).LengthSquared();
        float combined = sphere.radius + radius;
        
        return distSquared >= combined * combined ? OUTSIDE : INSIDE;
    }
    
    /// Test if a bounding box is inside, outside or intersects.
    Intersection IsInside(const BoundingBox& box) const
    {
        float radiusSquared = radius * radius;
        
        Vector3 closest(
            Clamp(center.x, box.min.x, box.max.x),
            Clamp(center.y, box.min.y, box.max.y),
            Clamp(center.z, box.min.z, box.max.z)
        );

        if ((closest - center).LengthSquared() >= radiusSquared)
            return OUTSIDE;

        Vector3 min = (box.min - center).Abs();
        Vector3 max = (box.max - center).Abs();

        Vector3 furthest(
            Max(min.x, max.x),
            Max(min.y, max.y),
            Max(min.z, max.z)
        );

        return furthest.LengthSquared() >= radiusSquared ? INTERSECTS : INSIDE;
    }

    /// Test if a bounding box is (partially) inside or outside.
    Intersection IsInsideFast(const BoundingBox& box) const
    {
        float radiusSquared = radius * radius;

        Vector3 closest(
            Clamp(center.x, box.min.x, box.max.x),
            Clamp(center.y, box.min.y, box.max.y),
            Clamp(center.z, box.min.z, box.max.z)
        );

        return ((closest - center).LengthSquared() >= radiusSquared) ? OUTSIDE : INSIDE;
    }

    /// Return distance of a point to the surface, or 0 if inside.
    float Distance(const Vector3& point) const { return Max((point - center).Length() - radius, 0.0f); }
};
