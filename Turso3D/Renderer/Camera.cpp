// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Matrix3x4.h"
#include "Camera.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static const Matrix4 flipMatrix(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
    );

Camera::Camera() :
    viewMatrix(Matrix3x4::IDENTITY),
    viewMatrixDirty(false),
    orthographic(false),
    autoAspectRatio(true),
    flipVertical(false),
    nearClip(DEFAULT_NEARCLIP),
    farClip(DEFAULT_FARCLIP),
    fov(DEFAULT_FOV),
    orthoSize(DEFAULT_ORTHOSIZE),
    aspectRatio(1.0f),
    zoom(1.0f),
    lodBias(1.0f),
    viewMask(M_MAX_UNSIGNED),
    projectionOffset(Vector2::ZERO),
    reflectionPlane(Plane::UP),
    clipPlane(Plane::UP),
    useReflection(false),
    useClipping(false)
{
    reflectionMatrix = reflectionPlane.ReflectionMatrix();
}

void Camera::RegisterObject()
{
    RegisterFactory<Camera>();
    CopyBaseAttributes<Camera, SpatialNode>();

    RegisterAttribute("nearClip", &Camera::NearClip, &Camera::SetNearClip, DEFAULT_NEARCLIP);
    RegisterAttribute("farClip", &Camera::FarClip, &Camera::SetFarClip, DEFAULT_FARCLIP);
    RegisterAttribute("fov", &Camera::Fov, &Camera::SetFov, DEFAULT_FOV);
    RegisterAttribute("aspectRatio", &Camera::AspectRatio, &Camera::SetAspectRatioInternal, 1.0f);
    RegisterAttribute("autoAspectRatio", &Camera::AutoAspectRatio, &Camera::SetAutoAspectRatio, true);
    RegisterAttribute("orthographic", &Camera::IsOrthographic, &Camera::SetOrthographic, false);
    RegisterAttribute("orthoSize", &Camera::OrthoSize, &Camera::SetOrthoSize, DEFAULT_ORTHOSIZE);
    RegisterAttribute("zoom", &Camera::Zoom, &Camera::SetZoom, 1.0f);
    RegisterAttribute("lodBias", &Camera::LodBias, &Camera::SetLodBias, 1.0f);
    RegisterAttribute("viewMask", &Camera::ViewMask, &Camera::SetViewMask, M_MAX_UNSIGNED);
    RegisterRefAttribute("projectionOffset", &Camera::ProjectionOffset, &Camera::SetProjectionOffset, Vector2::ZERO);
    RegisterMixedRefAttribute("reflectionPlane", &Camera::ReflectionPlaneAttr, &Camera::SetReflectionPlaneAttr, Vector4(0.0f, 1.0f, 0.0f, 0.0f));
    RegisterMixedRefAttribute("clipPlane", &Camera::ClipPlaneAttr, &Camera::SetClipPlaneAttr, Vector4(0.0f, 1.0f, 0.0f, 0.0f));
    RegisterAttribute("useReflection", &Camera::UseReflection, &Camera::SetUseReflection, false);
    RegisterAttribute("useClipping", &Camera::UseClipping, &Camera::SetUseClipping, false);
}

void Camera::SetNearClip(float nearClip_)
{
    nearClip = Max(nearClip_, M_EPSILON);
}

void Camera::SetFarClip(float farClip_)
{
    farClip = Max(farClip_, M_EPSILON);
}

void Camera::SetFov(float fov_)
{
    fov = Clamp(fov_, 0.0f, 180.0f);
}

void Camera::SetOrthoSize(float orthoSize_)
{
    orthoSize = orthoSize_;
    aspectRatio = 1.0f;
}

void Camera::SetOrthoSize(const Vector2& orthoSize_)
{
    autoAspectRatio = false;
    orthoSize = orthoSize_.y;
    aspectRatio = orthoSize_.x / orthoSize_.y;
}

void Camera::SetAspectRatio(float aspectRatio_)
{
    autoAspectRatio = false;
    SetAspectRatioInternal(aspectRatio_);
}

void Camera::SetZoom(float zoom_)
{
    zoom = Max(zoom_, M_EPSILON);
}

void Camera::SetLodBias(float bias)
{
    lodBias = Max(bias, M_EPSILON);
}

void Camera::SetViewMask(unsigned mask)
{
    viewMask = mask;
}

void Camera::SetOrthographic(bool enable)
{
    orthographic = enable;
}

void Camera::SetAutoAspectRatio(bool enable)
{
    autoAspectRatio = enable;
}

void Camera::SetProjectionOffset(const Vector2& offset)
{
    projectionOffset = offset;
}

void Camera::SetUseReflection(bool enable)
{
    useReflection = enable;
    viewMatrixDirty = true;
}

void Camera::SetReflectionPlane(const Plane& plane)
{
    reflectionPlane = plane;
    reflectionMatrix = plane.ReflectionMatrix();
    viewMatrixDirty = true;
}

void Camera::SetUseClipping(bool enable)
{
    useClipping = enable;
}

void Camera::SetClipPlane(const Plane& plane)
{
    clipPlane = plane;
}


void Camera::SetFlipVertical(bool enable)
{
    flipVertical = enable;
}

float Camera::NearClip() const
{
    // Orthographic camera has always near clip at 0 to avoid trouble with shader depth parameters,
    // and unlike in perspective mode there should be no depth buffer precision issue
    return orthographic ? 0.0f : nearClip;
}

Frustum Camera::WorldFrustum() const
{
    Frustum ret;
    Matrix3x4 worldTransform = EffectiveWorldTransform();

    if (!orthographic)
        ret.Define(fov, aspectRatio, zoom, NearClip(), farClip, worldTransform);
    else
        ret.DefineOrtho(orthoSize, aspectRatio, zoom, NearClip(), farClip, worldTransform);

    return ret;
}

Frustum Camera::WorldSplitFrustum(float nearClip_, float farClip_) const
{
    Frustum ret;
    Matrix3x4 worldTransform = EffectiveWorldTransform();

    nearClip_ = Max(nearClip_, NearClip());
    farClip_ = Min(farClip_, farClip);
    if (farClip_ < nearClip_)
        farClip_ = nearClip_;

    if (!orthographic)
        ret.Define(fov, aspectRatio, zoom, nearClip_, farClip_, worldTransform);
    else
        ret.DefineOrtho(orthoSize, aspectRatio, zoom, nearClip_, farClip_, worldTransform);

    return ret;
}

Frustum Camera::ViewSpaceFrustum() const
{
    Frustum ret;

    if (!orthographic)
        ret.Define(fov, aspectRatio, zoom, NearClip(), farClip);
    else
        ret.DefineOrtho(orthoSize, aspectRatio, zoom, NearClip(), farClip);

    return ret;
}

Frustum Camera::ViewSpaceSplitFrustum(float nearClip_, float farClip_) const
{
    Frustum ret;

    nearClip_ = Max(nearClip_, NearClip());
    farClip_ = Min(farClip_, farClip);
    if (farClip_ < nearClip_)
        farClip_ = nearClip_;

    if (!orthographic)
        ret.Define(fov, aspectRatio, zoom, nearClip_, farClip_);
    else
        ret.DefineOrtho(orthoSize, aspectRatio, zoom, nearClip_, farClip_);

    return ret;
}

const Matrix3x4& Camera::ViewMatrix() const
{
    if (viewMatrixDirty)
    {
        viewMatrix = EffectiveWorldTransform().Inverse();
        viewMatrixDirty = false;
    }

    return viewMatrix;
}

Matrix4 Camera::ProjectionMatrix(bool apiSpecific) const
{
    Matrix4 ret(Matrix4::ZERO);

    bool openGLFormat = apiSpecific;

    // Whether to construct matrix using OpenGL or Direct3D clip space convention
    #ifndef TURSO3D_OPENGL
    openGLFormat = false;
    #endif

    if (!orthographic)
    {
        float h = (1.0f / tanf(fov * M_DEGTORAD * 0.5f)) * zoom;
        float w = h / aspectRatio;
        float q, r;

        if (openGLFormat)
        {
            q = (farClip + nearClip) / (farClip - nearClip);
            r = -2.0f * farClip * nearClip / (farClip - nearClip);
        }
        else
        {
            q = farClip / (farClip - nearClip);
            r = -q * nearClip;
        }

        ret.m00 = w;
        ret.m02 = projectionOffset.x * 2.0f;
        ret.m11 = h;
        ret.m12 = projectionOffset.y * 2.0f;
        ret.m22 = q;
        ret.m23 = r;
        ret.m32 = 1.0f;
    }
    else
    {
        // Disregard near clip, because it does not affect depth precision as with perspective projection
        float h = (1.0f / (orthoSize * 0.5f)) * zoom;
        float w = h / aspectRatio;
        float q, r;

        if (openGLFormat)
        {
            q = 2.0f / farClip;
            r = -1.0f;
        }
        else
        {
            q = 1.0f / farClip;
            r = 0.0f;
        }

        ret.m00 = w;
        ret.m03 = projectionOffset.x * 2.0f;
        ret.m11 = h;
        ret.m13 = projectionOffset.y * 2.0f;
        ret.m22 = q;
        ret.m23 = r;
        ret.m33 = 1.0f;
    }

    if (flipVertical)
        ret = flipMatrix * ret;

    return ret;
}

void Camera::FrustumSize(Vector3& near, Vector3& far) const
{
    near.z = NearClip();
    far.z = farClip;

    if (!orthographic)
    {
        float halfViewSize = tanf(fov * M_DEGTORAD * 0.5f) / zoom;
        near.y = near.z * halfViewSize;
        near.x = near.y * aspectRatio;
        far.y = far.z * halfViewSize;
        far.x = far.y * aspectRatio;
    }
    else
    {
        float halfViewSize = orthoSize * 0.5f / zoom;
        near.y = far.y = halfViewSize;
        near.x = far.x = near.y * aspectRatio;
    }

    if (flipVertical)
    {
        near.y = -near.y;
        far.y = -far.y;
    }
}

float Camera::HalfViewSize() const
{
    if (!orthographic)
        return tanf(fov * M_DEGTORAD * 0.5f) / zoom;
    else
        return orthoSize * 0.5f / zoom;
}

Ray Camera::ScreenRay(float x, float y) const
{
    Ray ret;

    // If projection is invalid, just return a ray pointing forward
    if (!IsProjectionValid())
    {
        ret.origin = WorldPosition();
        ret.direction = WorldDirection();
        return ret;
    }

    Matrix4 viewProjInverse = (ProjectionMatrix(false) * ViewMatrix()).Inverse();

    // The parameters range from 0.0 to 1.0. Expand to normalized device coordinates (-1.0 to 1.0) & flip Y axis
    x = 2.0f * x - 1.0f;
    y = 1.0f - 2.0f * y;
    Vector3 near(x, y, 0.0f);
    Vector3 far(x, y, 1.0f);

    ret.origin = viewProjInverse * near;
    ret.direction = ((viewProjInverse * far) - ret.origin).Normalized();
    return ret;
}

Vector2 Camera::WorldToScreenPoint(const Vector3& worldPos) const
{
    Vector3 eyeSpacePos = ViewMatrix() * worldPos;
    Vector2 ret;

    if (eyeSpacePos.z > 0.0f)
    {
        Vector3 screenSpacePos = ProjectionMatrix(false) * eyeSpacePos;
        ret.x = screenSpacePos.x;
        ret.y = screenSpacePos.y;
    }
    else
    {
        ret.x = (-eyeSpacePos.x > 0.0f) ? -1.0f : 1.0f;
        ret.y = (-eyeSpacePos.y > 0.0f) ? -1.0f : 1.0f;
    }

    ret.x = (ret.x * 0.5f) + 0.5f;
    ret.y = 1.0f - ((ret.y * 0.5f) + 0.5f);
    return ret;
}

Vector3 Camera::ScreenToWorldPoint(const Vector3& screenPos) const
{
    Ray ray = ScreenRay(screenPos.x, screenPos.y);
    return ray.origin + ray.direction * screenPos.z;
}

float Camera::Distance(const Vector3& worldPos) const
{
    if (!orthographic)
        return (worldPos - WorldPosition()).Length();
    else
        return Abs((ViewMatrix() * worldPos).z);
}

float Camera::DistanceSquared(const Vector3& worldPos) const
{
    if (!orthographic)
        return (worldPos - WorldPosition()).LengthSquared();
    else
    {
        float distance = (ViewMatrix() * worldPos).z;
        return distance * distance;
    }
}

float Camera::LodDistance(float distance, float scale, float bias) const
{
    float d = Max(lodBias * bias * scale * zoom, M_EPSILON);
    if (!orthographic)
        return distance / d;
    else
        return orthoSize / d;
}

Quaternion Camera::FaceCameraRotation(const Vector3& position, const Quaternion& rotation, FaceCameraMode mode)
{
    switch (mode)
    {
    default:
        return rotation;

    case FC_ROTATE_XYZ:
        return WorldRotation();

    case FC_ROTATE_Y:
        {
            Vector3 euler = rotation.EulerAngles();
            euler.y = WorldRotation().EulerAngles().y;
            return Quaternion(euler.x, euler.y, euler.z);
        }

    case FC_LOOKAT_XYZ:
        {
            Quaternion lookAt;
            lookAt.FromLookRotation(position - WorldPosition());
            return lookAt;
        }

    case FC_LOOKAT_Y:
        {
            // Make the Y-only lookat happen on an XZ plane to make sure there are no unwanted transitions
            // or singularities
            Vector3 lookAtVec(position - WorldPosition());
            lookAtVec.y = 0.0f;

            Quaternion lookAt;
            lookAt.FromLookRotation(lookAtVec);

            Vector3 euler = rotation.EulerAngles();
            euler.y = lookAt.EulerAngles().y;
            return Quaternion(euler.x, euler.y, euler.z);
        }
    }
}

Matrix3x4 Camera::EffectiveWorldTransform() const
{
    Matrix3x4 worldTransform(WorldPosition(), WorldRotation(), 1.0f);
    return useReflection ? reflectionMatrix * worldTransform : worldTransform;
}

bool Camera::IsProjectionValid() const
{
    return farClip > NearClip();
}

void Camera::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();

    viewMatrixDirty = true;
}

void Camera::SetAspectRatioInternal(float aspectRatio_)
{
    aspectRatio = aspectRatio_;
}

void Camera::SetReflectionPlaneAttr(const Vector4& value)
{
    SetReflectionPlane(Plane(value));
}

void Camera::SetClipPlaneAttr(const Vector4& value)
{
    SetClipPlane(Plane(value));
}

Vector4 Camera::ReflectionPlaneAttr() const
{
    return reflectionPlane.ToVector4();
}

Vector4 Camera::ClipPlaneAttr() const
{
    return clipPlane.ToVector4();
}

}
