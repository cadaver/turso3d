// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Rect.h"
#include "Vector3.h"

#include <string>

class Polyhedron;
class Frustum;
class Matrix3;
class Matrix4;
class Matrix3x4;
class Sphere;

/// Three-dimensional axis-aligned bounding box.
class BoundingBox
{
public:
    /// Minimum vector.
    Vector3 min;
    /// Maximum vector.
    Vector3 max;
    
    /// Construct as undefined (negative size.)
    BoundingBox() :
        min(Vector3(M_INFINITY, M_INFINITY, M_INFINITY)),
        max(Vector3(-M_INFINITY, -M_INFINITY, -M_INFINITY))
    {
    }
    
    /// Copy-construct.
    BoundingBox(const BoundingBox& box) :
        min(box.min),
        max(box.max)
    {
    }
    
    /// Construct from a rect, with the Z dimension left zero.
    BoundingBox(const Rect& rect) :
        min(Vector3(rect.min)),
        max(Vector3(rect.max))
    {
    }
    
    /// Construct from minimum and maximum vectors.
    BoundingBox(const Vector3& min_, const Vector3& max_) :
        min(min_),
        max(max_)
    {
    }
    
    /// Construct from minimum and maximum floats (all dimensions same.)
    BoundingBox(float min_, float max_) :
        min(Vector3(min_, min_, min_)),
        max(Vector3(max_, max_, max_))
    {
    }
    
    /// Construct from an array of vertices.
    BoundingBox(const Vector3* vertices, size_t count)
    {
        Define(vertices, count);
    }
    
    /// Construct from a frustum.
    BoundingBox(const Frustum& frustum)
    {
        Define(frustum);
    }
    
    /// Construct from a polyhedron.
    BoundingBox(const Polyhedron& poly)
    {
        Define(poly);
    }
    
    /// Construct from a sphere.
    BoundingBox(const Sphere& sphere)
    {
        Define(sphere);
    }
    
    /// Construct by parsing a string.
    BoundingBox(const std::string& str)
    {
        FromString(str.c_str());
    }
    
    /// Construct by parsing a C string.
    BoundingBox(const char* str)
    {
        FromString(str);
    }
    
    /// Assign from another bounding box.
    BoundingBox& operator = (const BoundingBox& rhs)
    {
        min = rhs.min;
        max = rhs.max;
        return *this;
    }
    
    /// Assign from a Rect, with the Z dimension left zero.
    BoundingBox& operator = (const Rect& rhs)
    {
        min = Vector3(rhs.min);
        max = Vector3(rhs.max);
        return *this;
    }
    
    /// Test for equality with another bounding box without epsilon.
    bool operator == (const BoundingBox& rhs) const { return min == rhs.min && max == rhs.max; }
    /// Test for inequality with another bounding box without epsilon.
    bool operator != (const BoundingBox& rhs) const { return !(*this == rhs); }
    
    /// Define from another bounding box.
    void Define(const BoundingBox& box)
    {
        min = box.min;
        max = box.max;
    }
    
    /// Define from a Rect.
    void Define(const Rect& rect)
    {
        min = Vector3(rect.min);
        max = Vector3(rect.max);
    }
    
    /// Define from minimum and maximum vectors.
    void Define(const Vector3& min_, const Vector3& max_)
    {
        min = min_;
        max = max_;
    }
    
    /// Define from minimum and maximum floats (all dimensions same.)
    void Define(float min_, float max_)
    {
        min = Vector3(min_, min_, min_);
        max = Vector3(max_, max_, max_);
    }
    
    /// Define from a point.
    void Define(const Vector3& point)
    {
        min = max = point;
    }
    
    /// Merge a point.
    void Merge(const Vector3& point)
    {
        // If undefined, set initial dimensions
        if (!IsDefined())
        {
            min = max = point;
            return;
        }
        
        if (point.x < min.x)
            min.x = point.x;
        if (point.y < min.y)
            min.y = point.y;
        if (point.z < min.z)
            min.z = point.z;
        if (point.x > max.x)
            max.x = point.x;
        if (point.y > max.y)
            max.y = point.y;
        if (point.z > max.z)
            max.z = point.z;
    }
    
    /// Merge another bounding box.
    void Merge(const BoundingBox& box)
    {
        if (min.x > max.x)
        {
            min = box.min;
            max = box.max;
            return;
        }
    
        if (box.min.x < min.x)
            min.x = box.min.x;
        if (box.min.y < min.y)
            min.y = box.min.y;
        if (box.min.z < min.z)
            min.z = box.min.z;
        if (box.max.x > max.x)
            max.x = box.max.x;
        if (box.max.y > max.y)
            max.y = box.max.y;
        if (box.max.z > max.z)
            max.z = box.max.z;
    }
    
    /// Set as undefined (negative size) to allow the next merge to set initial size.
    void Undefine()
    {
        min = Vector3(M_INFINITY, M_INFINITY, M_INFINITY);
        max = -min;
    }
    
    /// Define from an array of vertices.
    void Define(const Vector3* vertices, size_t count);
    /// Define from a frustum.
    void Define(const Frustum& frustum);
    /// Define from a polyhedron.
    void Define(const Polyhedron& poly);
    /// Define from a sphere.
    void Define(const Sphere& sphere);
    /// Merge an array of vertices.
    void Merge(const Vector3* vertices, size_t count);
    /// Merge a frustum.
    void Merge(const Frustum& frustum);
    /// Merge a polyhedron.
    void Merge(const Polyhedron& poly);
    /// Merge a sphere.
    void Merge(const Sphere& sphere);
    /// Clip with another bounding box.
    void Clip(const BoundingBox& box);
    /// Transform with a 3x3 matrix.
    void Transform(const Matrix3& transform);
    /// Transform with a 3x4 matrix.
    void Transform(const Matrix3x4& transform);
   /// Parse from a string. Return true on success.
    bool FromString(const std::string& str) { return FromString(str.c_str()); }
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    
    /// Return whether has non-negative size.
    bool IsDefined() const { return (min.x <= max.x); }
    /// Return center.
    Vector3 Center() const { return (max + min) * 0.5f; }
    /// Return size.
    Vector3 Size() const { return max - min; }
    /// Return half-size.
    Vector3 HalfSize() const { return (max - min) * 0.5f; }
    /// Test for equality with another bounding box with epsilon.
    bool Equals(const BoundingBox& box) const { return min.Equals(box.min) && max.Equals(box.max); }
    
    /// Test if a point is inside.
    Intersection IsInside(const Vector3& point) const
    {
        if (point.x < min.x || point.x > max.x || point.y < min.y || point.y > max.y ||
            point.z < min.z || point.z > max.z)
            return OUTSIDE;
        else
            return INSIDE;
    }
    
    /// Test if another bounding box is inside, outside or intersects.
    Intersection IsInside(const BoundingBox& box) const
    {
        if (box.max.x < min.x || box.min.x > max.x || box.max.y < min.y || box.min.y > max.y ||
            box.max.z < min.z || box.min.z > max.z)
            return OUTSIDE;
        else if (box.min.x < min.x || box.max.x > max.x || box.min.y < min.y || box.max.y > max.y ||
            box.min.z < min.z || box.max.z > max.z)
            return INTERSECTS;
        else
            return INSIDE;
    }
    
    /// Test if another bounding box is (partially) inside or outside.
    Intersection IsInsideFast(const BoundingBox& box) const
    {
        if (box.max.x < min.x || box.min.x > max.x || box.max.y < min.y || box.min.y > max.y ||
            box.max.z < min.z || box.min.z > max.z)
            return OUTSIDE;
        else
            return INSIDE;
    }
    
    /// Test if a sphere is inside, outside or intersects.
    Intersection IsInside(const Sphere& sphere) const;
    /// Test if a sphere is (partially) inside or outside.
    Intersection IsInsideFast(const Sphere& sphere) const;

    /// Return closest distance of a point to the faces of the box, or 0 if inside.
    float Distance(const Vector3 & point) const
    {
        Vector3 closest(
            Max(Min(point.x, max.x), min.x),
            Max(Min(point.y, max.y), min.y),
            Max(Min(point.z, max.z), min.z)
        );
        
        return sqrtf((point.x - closest.x) * (point.x - closest.x) + (point.y - closest.y) * (point.y - closest.y) + (point.z - closest.z) * (point.z - closest.z));
    }
    
    /// Return transformed by a 3x3 matrix.
    BoundingBox Transformed(const Matrix3 & transform) const;
    /// Return transformed by a 3x4 matrix.
    BoundingBox Transformed(const Matrix3x4 & transform) const;
    /// Return projected by a 4x4 projection matrix.
    Rect Projected(const Matrix4 & projection) const;

    /// Return projected by an axis to 1D coordinates.
    std::pair<float, float> Projected(const Vector3& axis) const
    {
        Vector3 center = Center();
        Vector3 edge = max - center;

        float centerProj = axis.DotProduct(center);
        float edgeProj = Abs(axis.Abs().DotProduct(edge));

        return std::make_pair(centerProj - edgeProj, centerProj + edgeProj);
    }
    
    /// Return as string.
    std::string ToString() const;
};
