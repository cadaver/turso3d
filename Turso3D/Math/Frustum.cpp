// For conditions of distribution and use, see copyright notice in License.txt

#include "Frustum.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

inline Vector3 ClipEdgeZ(const Vector3& v0, const Vector3& v1, float clipZ)
{
    return Vector3(
        v1.x + (v0.x - v1.x) * ((clipZ - v1.z) / (v0.z - v1.z)),
        v1.y + (v0.y - v1.y) * ((clipZ - v1.z) / (v0.z - v1.z)),
        clipZ
    );
}

void ProjectAndMergeEdge(Vector3 v0, Vector3 v1, Rect& rect, const Matrix4& projection)
{
    // Check if both vertices behind near plane
    if (v0.z < M_EPSILON && v1.z < M_EPSILON)
        return;
    
    // Check if need to clip one of the vertices
    if (v1.z < M_EPSILON)
        v1 = ClipEdgeZ(v1, v0, M_EPSILON);
    else if (v0.z < M_EPSILON)
        v0 = ClipEdgeZ(v0, v1, M_EPSILON);
    
    // Project, perspective divide and merge
    Vector3 tV0(projection * v0);
    Vector3 tV1(projection * v1);
    rect.Merge(Vector2(tV0.x, tV0.y));
    rect.Merge(Vector2(tV1.x, tV1.y));
}

Frustum::Frustum()
{
    UpdatePlanes();
}

Frustum::Frustum(const Frustum& frustum)
{
    *this = frustum;
}

Frustum& Frustum::operator = (const Frustum& rhs)
{
    for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        planes[i] = rhs.planes[i];
    for (size_t i = 0; i < NUM_FRUSTUM_VERTICES; ++i)
        vertices[i] = rhs.vertices[i];
    
    return *this;
}

void Frustum::Define(float fov, float aspectRatio, float zoom, float nearZ, float farZ, const Matrix3x4& transform)
{
    nearZ = Max(nearZ, 0.0f);
    farZ = Max(farZ, nearZ);
    float halfViewSize = tanf(fov * M_DEGTORAD_2) / zoom;
    Vector3 near, far;
    
    near.z = nearZ;
    near.y = near.z * halfViewSize;
    near.x = near.y * aspectRatio;
    far.z = farZ;
    far.y = far.z * halfViewSize;
    far.x = far.y * aspectRatio;
    
    Define(near, far, transform);
}

void Frustum::Define(const Vector3& near, const Vector3& far, const Matrix3x4& transform)
{
    vertices[0] = transform * near;
    vertices[1] = transform * Vector3(near.x, -near.y, near.z);
    vertices[2] = transform * Vector3(-near.x, -near.y, near.z);
    vertices[3] = transform * Vector3(-near.x, near.y, near.z);
    vertices[4] = transform * far;
    vertices[5] = transform * Vector3(far.x, -far.y, far.z);
    vertices[6] = transform * Vector3(-far.x, -far.y, far.z);
    vertices[7] = transform * Vector3(-far.x, far.y, far.z);
    
    UpdatePlanes();
}

void Frustum::Define(const BoundingBox& box, const Matrix3x4& transform)
{
    vertices[0] = transform * Vector3(box.max.x, box.max.y, box.min.z);
    vertices[1] = transform * Vector3(box.max.x, box.min.y, box.min.z);
    vertices[2] = transform * Vector3(box.min.x, box.min.y, box.min.z);
    vertices[3] = transform * Vector3(box.min.x, box.max.y, box.min.z);
    vertices[4] = transform * Vector3(box.max.x, box.max.y, box.max.z);
    vertices[5] = transform * Vector3(box.max.x, box.min.y, box.max.z);
    vertices[6] = transform * Vector3(box.min.x, box.min.y, box.max.z);
    vertices[7] = transform * Vector3(box.min.x, box.max.y, box.max.z);
    
    UpdatePlanes();
}

void Frustum::DefineOrtho(float orthoSize, float aspectRatio, float zoom, float nearZ, float farZ, const Matrix3x4& transform)
{
    nearZ = Max(nearZ, 0.0f);
    farZ = Max(farZ, nearZ);
    float halfViewSize = orthoSize * 0.5f / zoom;
    Vector3 near, far;
    
    near.z = nearZ;
    far.z = farZ;
    far.y = near.y = halfViewSize;
    far.x = near.x = near.y * aspectRatio;
    
    Define(near, far, transform);
}

void Frustum::Transform(const Matrix3& transform)
{
    for (size_t i = 0; i < NUM_FRUSTUM_VERTICES; ++i)
        vertices[i] = transform * vertices[i];
    
    UpdatePlanes();
}

void Frustum::Transform(const Matrix3x4& transform)
{
    for (size_t i = 0; i < NUM_FRUSTUM_VERTICES; ++i)
        vertices[i] = transform * vertices[i];
    
    UpdatePlanes();
}

Frustum Frustum::Transformed(const Matrix3& transform) const
{
    Frustum transformed;
    for (size_t i = 0; i < NUM_FRUSTUM_VERTICES; ++i)
        transformed.vertices[i] = transform * vertices[i];
    
    transformed.UpdatePlanes();
    return transformed;
}

Frustum Frustum::Transformed(const Matrix3x4& transform) const
{
    Frustum transformed;
    for (size_t i = 0; i < NUM_FRUSTUM_VERTICES; ++i)
        transformed.vertices[i] = transform * vertices[i];
    
    transformed.UpdatePlanes();
    return transformed;
}

Rect Frustum::Projected(const Matrix4& projection) const
{
    Rect rect;
    
    ProjectAndMergeEdge(vertices[0], vertices[4], rect, projection);
    ProjectAndMergeEdge(vertices[1], vertices[5], rect, projection);
    ProjectAndMergeEdge(vertices[2], vertices[6], rect, projection);
    ProjectAndMergeEdge(vertices[3], vertices[7], rect, projection);
    ProjectAndMergeEdge(vertices[4], vertices[5], rect, projection);
    ProjectAndMergeEdge(vertices[5], vertices[6], rect, projection);
    ProjectAndMergeEdge(vertices[6], vertices[7], rect, projection);
    ProjectAndMergeEdge(vertices[7], vertices[4], rect, projection);
    
    return rect;
}

void Frustum::UpdatePlanes()
{
    planes[PLANE_NEAR].Define(vertices[2], vertices[1], vertices[0]);
    planes[PLANE_LEFT].Define(vertices[3], vertices[7], vertices[6]);
    planes[PLANE_RIGHT].Define(vertices[1], vertices[5], vertices[4]);
    planes[PLANE_UP].Define(vertices[0], vertices[4], vertices[7]);
    planes[PLANE_DOWN].Define(vertices[6], vertices[5], vertices[1]);
    planes[PLANE_FAR].Define(vertices[5], vertices[6], vertices[7]);

    // Check if we ended up with inverted planes (reflected transform) and flip in that case
    if (planes[PLANE_NEAR].Distance(vertices[5]) < 0.0f)
    {
        for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            planes[i].normal = -planes[i].normal;
            planes[i].d = -planes[i].d;
        }
    }
}

}
