// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "BoundingBox.h"
#include "Matrix3x4.h"
#include "Plane.h"
#include "Sphere.h"

/// Frustum planes.
enum FrustumPlane
{
    PLANE_NEAR = 0,
    PLANE_LEFT,
    PLANE_RIGHT,
    PLANE_UP,
    PLANE_DOWN,
    PLANE_FAR,
};

static const size_t NUM_FRUSTUM_PLANES = 6;
static const size_t NUM_FRUSTUM_VERTICES = 8;
static const size_t NUM_SAT_AXES = 3 + 5 + 3 * 6;

/// Helper data for speeding up SAT tests of bounding boxes against a frustum. This needs to be calculated once for a given frustum.
struct SATData
{
    /// Calculate from a frustum.
    void Calculate(const Frustum& frustum);

    /// Bounding box normal axes, frustum normal axes and edge cross product axes.
    Vector3 axes[NUM_SAT_AXES];
    /// 1D coordinates of the frustum projected to each axis.
    std::pair<float, float> frustumProj[NUM_SAT_AXES];
};

/// Convex constructed of 6 planes.
class Frustum
{
public:
    /// Frustum planes.
    Plane planes[NUM_FRUSTUM_PLANES];
    /// Frustum vertices.
    Vector3 vertices[NUM_FRUSTUM_VERTICES];
    
    /// Construct a degenerate frustum with all points at origin.
    Frustum();
    /// Copy-construct.
    Frustum(const Frustum& frustum);
    
    /// Assign from another frustum.
    Frustum& operator = (const Frustum& rhs);
    
    /// Define with projection parameters and a transform matrix.
    void Define(float fov, float aspectRatio, float zoom, float nearZ, float farZ, const Matrix3x4& transform = Matrix3x4::IDENTITY);
    /// Define with near and far dimension vectors and a transform matrix.
    void Define(const Vector3& near, const Vector3& far, const Matrix3x4& transform = Matrix3x4::IDENTITY);
    /// Define with a bounding box and a transform matrix.
    void Define(const BoundingBox& box, const Matrix3x4& transform = Matrix3x4::IDENTITY);
    /// Define with orthographic projection parameters and a transform matrix.
    void DefineOrtho(float orthoSize, float aspectRatio, float zoom, float nearZ, float farZ, const Matrix3x4& transform = Matrix3x4::IDENTITY);
    /// Transform by a 3x3 matrix.
    void Transform(const Matrix3& transform);
    /// Transform by a 3x4 matrix.
    void Transform(const Matrix3x4& transform);
    
    /// Test if a point is inside or outside.
    Intersection IsInside(const Vector3& point) const
    {
        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            if (planes[i].Distance(point) < 0.0f)
                return OUTSIDE;
        }
        
        return INSIDE;
    }
    
    /// Test if a sphere is inside, outside or intersects.
    Intersection IsInside(const Sphere& sphere) const
    {
        bool allInside = true;

        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            float dist = planes[i].Distance(sphere.center);
            if (dist < -sphere.radius)
                return OUTSIDE;
            else if (dist < sphere.radius)
                allInside = false;
        }
        
        return allInside ? INSIDE : INTERSECTS;
    }
    
    /// Test if a sphere if (partially) inside or outside.
    Intersection IsInsideFast(const Sphere& sphere) const
    {
        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            if (planes[i].Distance(sphere.center) < -sphere.radius)
                return OUTSIDE;
        }
        
        return INSIDE;
    }
    
    /// Test if a bounding box is inside, outside or intersects.
    Intersection IsInside(const BoundingBox& box) const
    {
        Vector3 center = box.Center();
        Vector3 edge = center - box.min;
        bool allInside = true;
        
        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            const Plane& plane = planes[i];
            float dist = plane.normal.DotProduct(center) + plane.d;
            float absDist = plane.absNormal.DotProduct(edge);
            
            if (dist < -absDist)
                return OUTSIDE;
            else if (dist < absDist)
                allInside = false;
        }
        
        return allInside ? INSIDE : INTERSECTS;
    }

    /// Test if a bounding box is inside, outside or intersects. Updates a bitmask for speeding up further tests of hierarchies. Returns updated plane mask: 0xff if outside, 0x00 if completely inside, otherwise intersecting.
    unsigned char IsInsideMasked(const BoundingBox& box, unsigned char planeMask = 0x3f) const
    {
        Vector3 center = box.Center();
        Vector3 edge = center - box.min;

        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            unsigned char bit = 1 << i;

            if (planeMask & bit)
            {
                const Plane& plane = planes[i];
                float dist = plane.normal.DotProduct(center) + plane.d;
                float absDist = plane.absNormal.DotProduct(edge);

                if (dist < -absDist)
                    return 0xff;
                else if (dist >= absDist)
                    planeMask &= ~bit;
            }
        }

        return planeMask;
    }

    /// Test if a bounding box is inside, using a mask to skip unnecessary planes.
    Intersection IsInsideMaskedFast(const BoundingBox& box, unsigned char planeMask = 0x3f) const
    {
        Vector3 center = box.Center();
        Vector3 edge = center - box.min;

        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            unsigned char bit = 1 << i;

            if (planeMask & bit)
            {
                const Plane& plane = planes[i];
                float dist = plane.normal.DotProduct(center) + plane.d;
                float absDist = plane.absNormal.DotProduct(edge);
    
                if (dist < -absDist)
                    return OUTSIDE;
            }
        }

        return INSIDE;
    }
    
    /// Test if a bounding box is (partially) inside or outside.
    Intersection IsInsideFast(const BoundingBox& box) const
    {
        Vector3 center = box.Center();
        Vector3 edge = center - box.min;
        
        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            const Plane& plane = planes[i];
            float dist = plane.normal.DotProduct(center) + plane.d;
            float absDist = plane.absNormal.DotProduct(edge);
            
            if (dist < -absDist)
                return OUTSIDE;
        }
        
        return INSIDE;
    }

    /// Test if a bounding box is (partially) inside or outside using SAT. Is slower but correct. The SAT helper data needs to be calculated beforehand to speed up.
    Intersection IsInsideSAT(const BoundingBox& box, const SATData& data) const
    {
        for (size_t i = 0; i < NUM_SAT_AXES; ++i)
        {
            std::pair<float, float> boxProj = box.Projected(data.axes[i]);
            if (data.frustumProj[i].second < boxProj.first || boxProj.second < data.frustumProj[i].first)
                return OUTSIDE;
        }

        return INSIDE;
    }
    
    /// Return distance of a point to the frustum, or 0 if inside.
    float Distance(const Vector3& point) const
    {
        float distance = 0.0f;

        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
            distance = Max(-planes[i].Distance(point), distance);
        
        return distance;
    }
    
    /// Return transformed by a 3x3 matrix.
    Frustum Transformed(const Matrix3& transform) const;
    /// Return transformed by a 3x4 matrix.
    Frustum Transformed(const Matrix3x4& transform) const;
    /// Return projected by a 4x4 projection matrix.
    Rect Projected(const Matrix4& transform) const;
    /// Return projected by an axis to 1D coordinates.
    std::pair<float, float> Projected(const Vector3& axis) const;
    
    /// Update the planes. Called internally.
    void UpdatePlanes();
};
