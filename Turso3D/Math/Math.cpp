// For conditions of distribution and use, see copyright notice in License.txt

#include "Color.h"
#include "Frustum.h"
#include "Matrix3.h"
#include "Polyhedron.h"
#include "Ray.h"
#include "StringHash.h"

#include <cstdio>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const Color Color::WHITE;
const Color Color::GRAY   (0.5f, 0.5f, 0.5f);
const Color Color::BLACK  (0.0f, 0.0f, 0.0f);
const Color Color::RED    (1.0f, 0.0f, 0.0f);
const Color Color::GREEN  (0.0f, 1.0f, 0.0f);
const Color Color::BLUE   (0.0f, 0.0f, 1.0f);
const Color Color::CYAN   (0.0f, 1.0f, 1.0f);
const Color Color::MAGENTA(1.0f, 0.0f, 1.0f);
const Color Color::YELLOW (1.0f, 1.0f, 0.0f);
const Color Color::TRANSPARENT(0.0f, 0.0f, 0.0f, 0.0f);

const IntRect IntRect::ZERO(0, 0, 0, 0);

const IntVector2 IntVector2::ZERO;

const Matrix3 Matrix3::ZERO(
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f);

const Matrix3 Matrix3::IDENTITY;

const Matrix3x4 Matrix3x4::ZERO(
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f);

const Matrix3x4 Matrix3x4::IDENTITY;

const Matrix4 Matrix4::ZERO(
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f);

const Matrix4 Matrix4::IDENTITY;

// Static initialization order can not be relied on, so do not use Vector3 constants
const Plane Plane::UP(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));

const Quaternion Quaternion::IDENTITY;

const Rect Rect::FULL(-1.0f, -1.0f, 1.0f, 1.0f);
const Rect Rect::POSITIVE(0.0f, 0.0f, 1.0f, 1.0f);
const Rect Rect::ZERO(0.0f, 0.0f, 0.0f, 0.0f);

const StringHash StringHash::ZERO;

const Vector2 Vector2::ZERO;
const Vector2 Vector2::LEFT(-1.0f, 0.0f);
const Vector2 Vector2::RIGHT(1.0f, 0.0f);
const Vector2 Vector2::UP(0.0f, 1.0f);
const Vector2 Vector2::DOWN(0.0f, -1.0f);
const Vector2 Vector2::ONE(1.0f, 1.0f);

const Vector3 Vector3::ZERO;
const Vector3 Vector3::LEFT(-1.0f, 0.0f, 0.0f);
const Vector3 Vector3::RIGHT(1.0f, 0.0f, 0.0f);
const Vector3 Vector3::UP(0.0f, 1.0f, 0.0f);
const Vector3 Vector3::DOWN(0.0f, -1.0f, 0.0f);
const Vector3 Vector3::FORWARD(0.0f, 0.0f, 1.0f);
const Vector3 Vector3::BACK(0.0f, 0.0f, -1.0f);
const Vector3 Vector3::ONE(1.0f, 1.0f, 1.0f);

const Vector4 Vector4::ZERO;
const Vector4 Vector4::ONE(1.0f, 1.0f, 1.0f, 1.0f);

static unsigned randomSeed = 1;

void SetRandomSeed(unsigned seed)
{
    randomSeed = seed;
}

unsigned GetRandomSeed()
{
    return randomSeed;
}

int Rand()
{
    randomSeed = randomSeed * 214013 + 2531011;
    return (randomSeed >> 16) & 32767;
}

float RandStandardNormal()
{
    float val = 0.0f;
    for (int i = 0; i < 12; i++)
        val += Rand() / 32768.0f;
    val -= 6.0f;
    
    // Now val is approximatly standard normal distributed
    return val;
}

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

void BoundingBox::Define(const Vector3* vertices, size_t count)
{
    SetIllegal();
    Merge(vertices, count);
}

void BoundingBox::Define(const Frustum& frustum)
{
    Define(frustum.vertices, NUM_FRUSTUM_VERTICES);
}

void BoundingBox::Define(const Polyhedron& poly)
{
    SetIllegal();
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
    for (size_t i = 0; i < poly.faces.Size(); ++i)
    {
        const PODVector<Vector3>& face = poly.faces[i];
        if (!face.IsEmpty())
            Merge(&face[0], face.Size());
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
        Swap(min.x, max.x);
    if (min.y > max.y)
        Swap(min.y, max.y);
    if (min.z > max.z)
        Swap(min.z, max.z);
}

void BoundingBox::Transform(const Matrix3& transform)
{
    *this = Transformed(Matrix3x4(transform));
}

void BoundingBox::Transform(const Matrix3x4& transform)
{
    *this = Transformed(transform);
}

BoundingBox BoundingBox::Transformed(const Matrix3& transform) const
{
    return Transformed(Matrix3x4(transform));
}

BoundingBox BoundingBox::Transformed(const Matrix3x4& transform) const
{
    Vector3 newCenter = transform * Center();
    Vector3 oldEdge = Size() * 0.5f;
    Vector3 newEdge = Vector3(
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
    for (unsigned i = 0; i < 8; ++i)
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

String BoundingBox::ToString() const
{
    return min.ToString() + " - " + max.ToString();
}

unsigned Color::ToUInt() const
{
    unsigned r_ = Clamp(((int)(r * 255.0f)), 0, 255);
    unsigned g_ = Clamp(((int)(g * 255.0f)), 0, 255);
    unsigned b_ = Clamp(((int)(b * 255.0f)), 0, 255);
    unsigned a_ = Clamp(((int)(a * 255.0f)), 0, 255);
    return (a_ << 24) | (b_ << 16) | (g_ << 8) | r_;
}

Vector3 Color::ToHSL() const
{
    float min, max;
    Bounds(&min, &max, true);

    float h = Hue(min, max);
    float s = SaturationHSL(min, max);
    float l = (max + min) * 0.5f;

    return Vector3(h, s, l);
}

Vector3 Color::ToHSV() const
{
    float min, max;
    Bounds(&min, &max, true);

    float h = Hue(min, max);
    float s = SaturationHSV(min, max);
    float v = max;

    return Vector3(h, s, v);
}

void Color::FromHSL(float h, float s, float l, float a_)
{
    float c;
    if (l < 0.5f)
        c = (1.0f + (2.0f * l - 1.0f)) * s;
    else
        c = (1.0f - (2.0f * l - 1.0f)) * s;

    float m = l - 0.5f * c;

    FromHCM(h, c, m);

    a = a_;
}

void Color::FromHSV(float h, float s, float v, float a_)
{
    float c = v * s;
    float m = v - c;

    FromHCM(h, c, m);

    a = a_;
}

float Color::Chroma() const
{
    float min, max;
    Bounds(&min, &max, true);

    return max - min;
}

float Color::Hue() const
{
    float min, max;
    Bounds(&min, &max, true);

    return Hue(min, max);
}

float Color::SaturationHSL() const
{
    float min, max;
    Bounds(&min, &max, true);

    return SaturationHSL(min, max);
}

float Color::SaturationHSV() const
{
    float min, max;
    Bounds(&min, &max, true);

    return SaturationHSV(min, max);
}

float Color::Lightness() const
{
    float min, max;
    Bounds(&min, &max, true);

    return (max + min) * 0.5f;
}

void Color::Bounds(float* min, float* max, bool clipped) const
{
    assert(min && max);

    if (r > g)
    {
        if (g > b) // r > g > b
        {
            *max = r;
            *min = b;
        }
        else // r > g && g <= b
        {
            *max = r > b ? r : b;
            *min = g;
        }
    }
    else
    {
        if (b > g) // r <= g < b
        {
            *max = b;
            *min = r;
        }
        else // r <= g && b <= g
        {
            *max = g;
            *min = r < b ? r : b;
        }
    }

    if (clipped)
    {
        *max = *max > 1.0f ? 1.0f : (*max < 0.0f ? 0.0f : *max);
        *min = *min > 1.0f ? 1.0f : (*min < 0.0f ? 0.0f : *min);
    }
}

float Color::MaxRGB() const
{
    if (r > g)
        return (r > b) ? r : b;
    else
        return (g > b) ? g : b;
}

float Color::MinRGB() const
{
    if (r < g)
        return (r < b) ? r : b;
    else
        return (g < b) ? g : b;
}

float Color::Range() const
{
    float min, max;
    Bounds(&min, &max);
    return max - min;
}

void Color::Clip(bool clipAlpha)
{
    r = (r > 1.0f) ? 1.0f : ((r < 0.0f) ? 0.0f : r);
    g = (g > 1.0f) ? 1.0f : ((g < 0.0f) ? 0.0f : g);
    b = (b > 1.0f) ? 1.0f : ((b < 0.0f) ? 0.0f : b);

    if (clipAlpha)
        a = (a > 1.0f) ? 1.0f : ((a < 0.0f) ? 0.0f : a);
}

void Color::Invert(bool invertAlpha)
{
    r = 1.0f - r;
    g = 1.0f - g;
    b = 1.0f - b;

    if (invertAlpha)
        a = 1.0f - a;
}

Color Color::Lerp(const Color &rhs, float t) const
{
    float invT = 1.0f - t;
    return Color(
        r * invT + rhs.r * t,
        g * invT + rhs.g * t,
        b * invT + rhs.b * t,
        a * invT + rhs.a * t
    );
}

String Color::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g", r, g, b, a);
    return String(tempBuffer);
}

float Color::Hue(float min, float max) const
{
    float chroma = max - min;

    // If chroma equals zero, hue is undefined
    if (chroma <= M_EPSILON)
        return 0.0f;

    // Calculate and return hue
    if (Turso3D::Equals(g, max))
        return (b + 2.0f*chroma - r) / (6.0f * chroma);
    else if (Turso3D::Equals(b, max))
        return (4.0f * chroma - g + r) / (6.0f * chroma);
    else
    {
        float h = (g - b) / (6.0f * chroma);
        return (h < 0.0f) ? 1.0f + h : ((h >= 1.0f) ? h - 1.0f : h);
    }

}

float Color::SaturationHSV(float min, float max) const
{
    // Avoid div-by-zero: result undefined
    if (max <= M_EPSILON)
        return 0.0f;

    // Saturation equals chroma:value ratio
    return 1.0f - (min / max);
}

float Color::SaturationHSL(float min, float max) const
{
    // Avoid div-by-zero: result undefined
    if (max <= M_EPSILON || min >= 1.0f - M_EPSILON)
        return 0.0f;

    // Chroma = max - min, lightness = (max + min) * 0.5
    float hl = (max + min);
    if (hl <= 1.0f)
        return (max - min) / hl;
    else
        return (min - max) / (hl - 2.0f);

}

void Color::FromHCM(float h, float c, float m)
{
    if (h < 0.0f || h >= 1.0f)
        h -= floorf(h);

    float hs = h * 6.0f;
    float x = c * (1.0f - Turso3D::Abs(fmodf(hs, 2.0f) - 1.0f));

    // Reconstruct r', g', b' from hue
    if (hs < 2.0f)
    {
        b = 0.0f;
        if (hs < 1.0f)
        {
            g = x;
            r = c;
        }
        else
        {
            g = c;
            r = x;
        }
    }
    else if (hs < 4.0f)
    {
        r = 0.0f;
        if (hs < 3.0f)
        {
            g = c;
            b = x;
        }
        else
        {
            g = x;
            b = c;
        }
    }
    else
    {
        g = 0.0f;
        if (hs < 5.0f)
        {
            r = x;
            b = c;
        }
        else
        {
            r = c;
            b = x;
        }
    }

    r += m;
    g += m;
    b += m;
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
    for (unsigned i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        planes[i] = rhs.planes[i];
    for (unsigned i = 0; i < NUM_FRUSTUM_VERTICES; ++i)
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
        for (unsigned i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        {
            planes[i].normal = -planes[i].normal;
            planes[i].d = -planes[i].d;
        }
    }
}

String IntRect::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%d %d %d %d", left, top, right, bottom);
    return String(tempBuffer);
}

String IntVector2::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%d %d", x, y);
    return String(tempBuffer);
}

Matrix3 Matrix3::Inverse() const
{
    float det = m00 * m11 * m22 +
        m10 * m21 * m02 +
        m20 * m01 * m12 -
        m20 * m11 * m02 -
        m10 * m01 * m22 -
        m00 * m21 * m12;
    
    float invDet = 1.0f / det;
    
    return Matrix3(
        (m11 * m22 - m21 * m12) * invDet,
        -(m01 * m22 - m21 * m02) * invDet,
        (m01 * m12 - m11 * m02) * invDet,
        -(m10 * m22 - m20 * m12) * invDet,
        (m00 * m22 - m20 * m02) * invDet,
        -(m00 * m12 - m10 * m02) * invDet,
        (m10 * m21 - m20 * m11) * invDet,
        -(m00 * m21 - m20 * m01) * invDet,
        (m00 * m11 - m10 * m01) * invDet
    );
}

String Matrix3::ToString() const
{
    char tempBuffer[MATRIX_CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g %g %g %g %g %g", m00, m01, m02, m10, m11, m12, m20, m21, m22);
    return String(tempBuffer);
}

Matrix3x4::Matrix3x4(const Vector3& translation, const Quaternion& rotation, float scale)
{
    SetRotation(rotation.RotationMatrix() * scale);
    SetTranslation(translation);
}

Matrix3x4::Matrix3x4(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
{
    SetRotation(rotation.RotationMatrix().Scaled(scale));
    SetTranslation(translation);
}

void Matrix3x4::Decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const
{
    translation.x = m03;
    translation.y = m13;
    translation.z = m23;
    
    scale.x = sqrtf(m00 * m00 + m10 * m10 + m20 * m20);
    scale.y = sqrtf(m01 * m01 + m11 * m11 + m21 * m21);
    scale.z = sqrtf(m02 * m02 + m12 * m12 + m22 * m22);
    
    Vector3 invScale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
    rotation = Quaternion(ToMatrix3().Scaled(invScale));
}

Matrix3x4 Matrix3x4::Inverse() const
{
    float det = m00 * m11 * m22 +
        m10 * m21 * m02 +
        m20 * m01 * m12 -
        m20 * m11 * m02 -
        m10 * m01 * m22 -
        m00 * m21 * m12;
    
    float invDet = 1.0f / det;
    Matrix3x4 ret;
    
    ret.m00 = (m11 * m22 - m21 * m12) * invDet;
    ret.m01 = -(m01 * m22 - m21 * m02) * invDet;
    ret.m02 = (m01 * m12 - m11 * m02) * invDet;
    ret.m03 = -(m03 * ret.m00 + m13 * ret.m01 + m23 * ret.m02);
    ret.m10 = -(m10 * m22 - m20 * m12) * invDet;
    ret.m11 = (m00 * m22 - m20 * m02) * invDet;
    ret.m12 = -(m00 * m12 - m10 * m02) * invDet;
    ret.m13 = -(m03 * ret.m10 + m13 * ret.m11 + m23 * ret.m12);
    ret.m20 = (m10 * m21 - m20 * m11) * invDet;
    ret.m21 = -(m00 * m21 - m20 * m01) * invDet;
    ret.m22 = (m00 * m11 - m10 * m01) * invDet;
    ret.m23 = -(m03 * ret.m20 + m13 * ret.m21 + m23 * ret.m22);
    
    return ret;
}

String Matrix3x4::ToString() const
{
    char tempBuffer[MATRIX_CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g %g %g %g %g %g %g %g %g", m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22,
        m23);
    return String(tempBuffer);
}

void Matrix4::Decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const
{
    translation.x = m03;
    translation.y = m13;
    translation.z = m23;
    
    scale.x = sqrtf(m00 * m00 + m10 * m10 + m20 * m20);
    scale.y = sqrtf(m01 * m01 + m11 * m11 + m21 * m21);
    scale.z = sqrtf(m02 * m02 + m12 * m12 + m22 * m22);
    
    Vector3 invScale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
    rotation = Quaternion(ToMatrix3().Scaled(invScale));
}

Matrix4 Matrix4::Inverse() const
{
    float v0 = m20 * m31 - m21 * m30;
    float v1 = m20 * m32 - m22 * m30;
    float v2 = m20 * m33 - m23 * m30;
    float v3 = m21 * m32 - m22 * m31;
    float v4 = m21 * m33 - m23 * m31;
    float v5 = m22 * m33 - m23 * m32;
    
    float i00 = (v5 * m11 - v4 * m12 + v3 * m13);
    float i10 = -(v5 * m10 - v2 * m12 + v1 * m13);
    float i20 = (v4 * m10 - v2 * m11 + v0 * m13);
    float i30 = -(v3 * m10 - v1 * m11 + v0 * m12);
    
    float invDet = 1.0f / (i00 * m00 + i10 * m01 + i20 * m02 + i30 * m03);
    
    i00 *= invDet;
    i10 *= invDet;
    i20 *= invDet;
    i30 *= invDet;
    
    float i01 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    float i11 = (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    float i21 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    float i31 = (v3 * m00 - v1 * m01 + v0 * m02) * invDet;
    
    v0 = m10 * m31 - m11 * m30;
    v1 = m10 * m32 - m12 * m30;
    v2 = m10 * m33 - m13 * m30;
    v3 = m11 * m32 - m12 * m31;
    v4 = m11 * m33 - m13 * m31;
    v5 = m12 * m33 - m13 * m32;
    
    float i02 = (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    float i12 = -(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    float i22 = (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    float i32 = -(v3 * m00 - v1 * m01 + v0 * m02) * invDet;
    
    v0 = m21 * m10 - m20 * m11;
    v1 = m22 * m10 - m20 * m12;
    v2 = m23 * m10 - m20 * m13;
    v3 = m22 * m11 - m21 * m12;
    v4 = m23 * m11 - m21 * m13;
    v5 = m23 * m12 - m22 * m13;
    
    float i03 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
    float i13 = (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
    float i23 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
    float i33 = (v3 * m00 - v1 * m01 + v0 * m02) * invDet;
    
    return Matrix4(
        i00, i01, i02, i03,
        i10, i11, i12, i13,
        i20, i21, i22, i23,
        i30, i31, i32, i33);
}

String Matrix4::ToString() const
{
    char tempBuffer[MATRIX_CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g", m00, m01, m02, m03, m10, m11, m12, m13, m20,
        m21, m22, m23, m30, m31, m32, m33);
    return String(tempBuffer);
}

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
    Vector3 forward = direction.Normalized();
    Vector3 v = forward.CrossProduct(upDirection).Normalized();
    Vector3 up = v.CrossProduct(forward);
    Vector3 right = up.CrossProduct(forward);

    Quaternion ret;
    ret.FromAxes(right, up, forward);
    
    if (!ret.IsNaN())
    {
        (*this) = ret;
        return true;
    }
    else
        return false;
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

Matrix3 Quaternion::RotationMatrix() const
{
    return Matrix3(
        1.0f - 2.0f * y * y - 2.0f * z * z,
        2.0f * x * y - 2.0f * w * z,
        2.0f * x * z + 2.0f * w * y,
        2.0f * x * y + 2.0f * w * z,
        1.0f - 2.0f * x * x - 2.0f * z * z,
        2.0f * y * z - 2.0f * w * x,
        2.0f * x * z - 2.0f * w * y,
        2.0f * y * z + 2.0f * w * x,
        1.0f - 2.0f * x * x - 2.0f * y * y
    );
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

String Quaternion::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g", w, x, y, z);
    return String(tempBuffer);
}

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
    // If illegal, no hit (infinite distance)
    if (box.min.x > box.max.x)
        return M_INFINITY;
    
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
    // If illegal, no hit (infinite distance)
    if (sphere.radius < 0.0f)
        return M_INFINITY;
    
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

float Ray::HitDistance(const Vector3& v0, const Vector3& v1, const Vector3& v2) const
{
    return HitDistance(v0, v1, v2, 0);
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
                // There is an intersection, so calculate distance & optional normal
                if (outNormal)
                    *outNormal = edge1.CrossProduct(edge2);
                
                return edge2.DotProduct(q) / det;
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

String Rect::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g", min.x, min.y, max.x, max.y);
    return String(tempBuffer);
}

void Rect::Clip(const Rect& rect)
{
    if (rect.min.x > min.x)
        min.x = rect.min.x;
    if (rect.max.x < max.x)
        max.x = rect.max.x;
    if (rect.min.y > min.y)
        min.y = rect.min.y;
    if (rect.max.y < max.y)
        max.y = rect.max.y;
    
    if (min.x > max.x)
        Swap(min.x, max.x);
    if (min.y > max.y)
        Swap(min.y, max.y);
}

void Sphere::Define(const Vector3* vertices, size_t count)
{
    SetIllegal();
    Merge(vertices, count);
}

void Sphere::Define(const BoundingBox& box)
{
    const Vector3& min = box.min;
    const Vector3& max = box.max;
    
    SetIllegal();
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
    SetIllegal();
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
        const PODVector<Vector3>& face = poly.faces[i];
        if (!face.IsEmpty())
            Merge(&face[0], face.Size());
    }
}

void Sphere::Merge(const Sphere& sphere)
{
    // If is illegal, set initial dimensions
    if (radius < 0.0f)
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

String StringHash::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%08X", value);
    return String(tempBuffer);
}

String Vector2::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g", x, y);
    return String(tempBuffer);
}

String Vector3::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g", x, y, z);
    return String(tempBuffer);
}

String Vector4::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g", x, y, z, w);
    return String(tempBuffer);
}

}
