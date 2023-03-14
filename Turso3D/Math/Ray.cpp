// For conditions of distribution and use, see copyright notice in License.txt

#include "BoundingBox.h"
#include "Frustum.h"
#include "Plane.h"
#include "Ray.h"

Vector3 Ray::ClosestPoint(const Ray& ray) const
{
    // Algorithm based on http://paulbourke.net/geometry/lineline3d/
    Vector3 p13 = origin - ray.origin;
    Vector3 p43 = ray.direction;
    Vector3 p21 = direction;
    
    float d1343 = p13.DotProduct(p43);
    float d4321 = p43.DotProduct(p21);
    float d1321 = p13.DotProduct(p21);
    float d4343 = p43.DotProduct(p43);
    float d2121 = p21.DotProduct(p21);
    
    float d = d2121 * d4343 - d4321 * d4321;
    if (Abs(d) < M_EPSILON)
        return origin;
    float n = d1343 * d4321 - d1321 * d4343;
    float a = n / d;
    
    return origin + a * direction;
}

float Ray::HitDistance(const Plane& plane) const
{
    float d = plane.normal.DotProduct(direction);
    if (Abs(d) >= M_EPSILON)
    {
        float t = -(plane.normal.DotProduct(origin) + plane.d) / d;
        if (t >= 0.0f)
            return t;
        else
            return M_INFINITY;
    }
    else
        return M_INFINITY;
}

float Ray::HitDistance(const BoundingBox& box) const
{
    // Check for ray origin being inside the box
    if (box.IsInside(origin))
        return 0.0f;
    
    float dist = M_INFINITY;
    
    // Check for intersecting in the X-direction
    if (origin.x < box.min.x && direction.x > 0.0f)
    {
        float x = (box.min.x - origin.x) / direction.x;
        if (x < dist)
        {
            Vector3 point = origin + x * direction;
            if (point.y >= box.min.y && point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z)
                dist = x;
        }
    }
    if (origin.x > box.max.x && direction.x < 0.0f)
    {
        float x = (box.max.x - origin.x) / direction.x;
        if (x < dist)
        {
            Vector3 point = origin + x * direction;
            if (point.y >= box.min.y && point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z)
                dist = x;
        }
    }
    // Check for intersecting in the Y-direction
    if (origin.y < box.min.y && direction.y > 0.0f)
    {
        float x = (box.min.y - origin.y) / direction.y;
        if (x < dist)
        {
            Vector3 point = origin + x * direction;
            if (point.x >= box.min.x && point.x <= box.max.x && point.z >= box.min.z && point.z <= box.max.z)
                dist = x;
        }
    }
    if (origin.y > box.max.y && direction.y < 0.0f)
    {
        float x = (box.max.y - origin.y) / direction.y;
        if (x < dist)
        {
            Vector3 point = origin + x * direction;
            if (point.x >= box.min.x && point.x <= box.max.x && point.z >= box.min.z && point.z <= box.max.z)
                dist = x;
        }
    }
    // Check for intersecting in the Z-direction
    if (origin.z < box.min.z && direction.z > 0.0f)
    {
        float x = (box.min.z - origin.z) / direction.z;
        if (x < dist)
        {
            Vector3 point = origin + x * direction;
            if (point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y)
                dist = x;
        }
    }
    if (origin.z > box.max.z && direction.z < 0.0f)
    {
        float x = (box.max.z - origin.z) / direction.z;
        if (x < dist)
        {
            Vector3 point = origin + x * direction;
            if (point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y)
                dist = x;
        }
    }
    
    return dist;
}

float Ray::HitDistance(const Frustum& frustum, bool solidInside) const
{
    float maxOutside = 0.0f;
    float minInside = M_INFINITY;
    bool allInside = true;
    
    for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
    {
        const Plane& plane = frustum.planes[i];
        float distance = HitDistance(frustum.planes[i]);
        
        if (plane.Distance(origin) < 0.0f)
        {
            maxOutside = Max(maxOutside, distance);
            allInside = false;
        }
        else
            minInside = Min(minInside, distance);
    }
    
    if (allInside)
        return solidInside ? 0.0f : minInside;
    else if (maxOutside <= minInside)
        return maxOutside;
    else
        return M_INFINITY;
}

float Ray::HitDistance(const Sphere& sphere) const
{
    Vector3 centeredOrigin = origin - sphere.center;
    float squaredRadius = sphere.radius * sphere.radius;
    
    // Check if ray originates inside the sphere
    if (centeredOrigin.LengthSquared() <= squaredRadius)
        return 0.0f;
    
    // Calculate intersection by quadratic equation
    float a = direction.DotProduct(direction);
    float b = 2.0f * centeredOrigin.DotProduct(direction);
    float c = centeredOrigin.DotProduct(centeredOrigin) - squaredRadius;
    float d = b * b - 4.0f * a * c;
    
    // No solution
    if (d < 0.0f)
        return M_INFINITY;
    
    // Get the nearer solution
    float dSqrt = sqrtf(d);
    float dist = (-b - dSqrt) / (2.0f * a);
    if (dist >= 0.0f)
        return dist;
    else
        return (-b + dSqrt) / (2.0f * a);
}

float Ray::HitDistance(const Vector3& v0, const Vector3& v1, const Vector3& v2, Vector3* outNormal) const
{
    // Based on Fast, Minimum Storage Ray/Triangle Intersection by Möller & Trumbore
    // http://www.graphics.cornell.edu/pubs/1997/MT97.pdf
    // Calculate edge vectors
    Vector3 edge1(v1 - v0);
    Vector3 edge2(v2 - v0);
    
    // Calculate determinant & check backfacing
    Vector3 p(direction.CrossProduct(edge2));
    float det = edge1.DotProduct(p);
    if (det >= M_EPSILON)
    {
        // Calculate u & v parameters and test
        Vector3 t(origin - v0);
        float u = t.DotProduct(p);
        if (u >= 0.0f && u <= det)
        {
            Vector3 q(t.CrossProduct(edge1));
            float v = direction.DotProduct(q);
            if (v >= 0.0f && u + v <= det)
            {
                float distance = edge2.DotProduct(q) / det;
                if (distance >= 0.0f)
                {
                    // There is an intersection, so calculate distance & optional normal
                    if (outNormal)
                        *outNormal = edge1.CrossProduct(edge2);
                    
                    return distance;
                }
            }
        }
    }
    
    return M_INFINITY;
}

float Ray::HitDistance(const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount, Vector3* outNormal) const
{
    float nearest = M_INFINITY;
    const unsigned char* vertices = ((const unsigned char*)vertexData) + vertexStart * vertexSize;
    size_t index = 0;
    
    while (index + 2 < vertexCount)
    {
        const Vector3& v0 = *((const Vector3*)(&vertices[index * vertexSize]));
        const Vector3& v1 = *((const Vector3*)(&vertices[(index + 1) * vertexSize]));
        const Vector3& v2 = *((const Vector3*)(&vertices[(index + 2) * vertexSize]));
        nearest = Min(nearest, HitDistance(v0, v1, v2, outNormal));
        index += 3;
    }
    
    return nearest;
}

float Ray::HitDistance(const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize,
    size_t indexStart, size_t indexCount, Vector3* outNormal) const
{
    float nearest = M_INFINITY;
    const unsigned char* vertices = (const unsigned char*)vertexData;
    
    // 16-bit indices
    if (indexSize == sizeof(unsigned short))
    {
        const unsigned short* indices = ((const unsigned short*)indexData) + indexStart;
        const unsigned short* indicesEnd = indices + indexCount;
        
        while (indices < indicesEnd)
        {
            const Vector3& v0 = *((const Vector3*)(&vertices[indices[0] * vertexSize]));
            const Vector3& v1 = *((const Vector3*)(&vertices[indices[1] * vertexSize]));
            const Vector3& v2 = *((const Vector3*)(&vertices[indices[2] * vertexSize]));
            nearest = Min(nearest, HitDistance(v0, v1, v2, outNormal));
            indices += 3;
        }
    }
    // 32-bit indices
    else
    {
        const size_t* indices = ((const size_t*)indexData) + indexStart;
        const size_t* indicesEnd = indices + indexCount;
        
        while (indices < indicesEnd)
        {
            const Vector3& v0 = *((const Vector3*)(&vertices[indices[0] * vertexSize]));
            const Vector3& v1 = *((const Vector3*)(&vertices[indices[1] * vertexSize]));
            const Vector3& v2 = *((const Vector3*)(&vertices[indices[2] * vertexSize]));
            nearest = Min(nearest, HitDistance(v0, v1, v2, outNormal));
            indices += 3;
        }
    }
    
    return nearest;
}

bool Ray::InsideGeometry(const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount) const
{
    float currentFrontFace = M_INFINITY;
    float currentBackFace = M_INFINITY;
    const unsigned char* vertices = ((const unsigned char*)vertexData) + vertexStart * vertexSize;
    size_t index = 0;
    
    while (index + 2 < vertexCount)
    {
        const Vector3& v0 = *((const Vector3*)(&vertices[index * vertexSize]));
        const Vector3& v1 = *((const Vector3*)(&vertices[(index + 1) * vertexSize]));
        const Vector3& v2 = *((const Vector3*)(&vertices[(index + 2) * vertexSize]));
        float frontFaceDistance = HitDistance(v0, v1, v2);
        float backFaceDistance = HitDistance(v2, v1, v0);
        currentFrontFace = Min(frontFaceDistance > 0.0f ? frontFaceDistance : M_INFINITY, currentFrontFace);
        // A backwards face is just a regular one, with the vertices in the opposite order. This essentially checks backfaces by
        // checking reversed frontfaces
        currentBackFace = Min(backFaceDistance > 0.0f ? backFaceDistance : M_INFINITY, currentBackFace);
        index += 3;
    }
    
    // If the closest face is a backface, that means that the ray originates from the inside of the geometry
    // NOTE: there may be cases where both are equal, as in, no collision to either. This is prevented in the most likely case
    // (ray doesnt hit either) by this conditional
    if (currentFrontFace != M_INFINITY || currentBackFace != M_INFINITY)
        return currentBackFace < currentFrontFace;
    
    // It is still possible for two triangles to be equally distant from the triangle, however, this is extremely unlikely.
    // As such, it is safe to assume they are not
    return false;
}

bool Ray::InsideGeometry(const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize,
    size_t indexStart, size_t indexCount) const
{
    float currentFrontFace = M_INFINITY;
    float currentBackFace = M_INFINITY;
    const unsigned char* vertices = (const unsigned char*)vertexData;
    
    // 16-bit indices
    if (indexSize == sizeof(unsigned short))
    {
        const unsigned short* indices = ((const unsigned short*)indexData) + indexStart;
        const unsigned short* indicesEnd = indices + indexCount;
        
        while (indices < indicesEnd)
        {
            const Vector3& v0 = *((const Vector3*)(&vertices[indices[0] * vertexSize]));
            const Vector3& v1 = *((const Vector3*)(&vertices[indices[1] * vertexSize]));
            const Vector3& v2 = *((const Vector3*)(&vertices[indices[2] * vertexSize]));
            float frontFaceDistance = HitDistance(v0, v1, v2);
            float backFaceDistance = HitDistance(v2, v1, v0);
            currentFrontFace = Min(frontFaceDistance > 0.0f ? frontFaceDistance : M_INFINITY, currentFrontFace);
            // A backwards face is just a regular one, with the vertices in the opposite order. This essentially checks backfaces by
            // checking reversed frontfaces
            currentBackFace = Min(backFaceDistance > 0.0f ? backFaceDistance : M_INFINITY, currentBackFace);
            indices += 3;
        }
    }
    // 32-bit indices
    else
    {
        const size_t* indices = ((const size_t*)indexData) + indexStart;
        const size_t* indicesEnd = indices + indexCount;
        
        while (indices < indicesEnd)
        {
            const Vector3& v0 = *((const Vector3*)(&vertices[indices[0] * vertexSize]));
            const Vector3& v1 = *((const Vector3*)(&vertices[indices[1] * vertexSize]));
            const Vector3& v2 = *((const Vector3*)(&vertices[indices[2] * vertexSize]));
            float frontFaceDistance = HitDistance(v0, v1, v2);
            float backFaceDistance = HitDistance(v2, v1, v0);
            currentFrontFace = Min(frontFaceDistance > 0.0f ? frontFaceDistance : M_INFINITY, currentFrontFace);
            // A backwards face is just a regular one, with the vertices in the opposite order. This essentially checks backfaces by
            // checking reversed frontfaces
            currentBackFace = Min(backFaceDistance > 0.0f ? backFaceDistance : M_INFINITY, currentBackFace);
            indices += 3; 
        }
    }
    
    // If the closest face is a backface, that means that the ray originates from the inside of the geometry
    // NOTE: there may be cases where both are equal, as in, no collision to either. This is prevented in the most likely case
    // (ray doesnt hit either) by this conditional
    if (currentFrontFace != M_INFINITY || currentBackFace != M_INFINITY)
        return currentBackFace < currentFrontFace;
    
    // It is still possible for two triangles to be equally distant from the triangle, however, this is extremely unlikely.
    // As such, it is safe to assume they are not
    return false;
}

Ray Ray::Transformed(const Matrix3x4& transform) const
{
    Ray ret;
    ret.origin = transform * origin;
    ret.direction = transform * Vector4(direction, 0.0f);
    return ret;
}
