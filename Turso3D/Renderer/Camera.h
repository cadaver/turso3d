// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Math/Plane.h"
#include "../Math/Ray.h"
#include "../Scene/SpatialNode.h"

/// Billboard camera facing modes.
enum FaceCameraMode
{
    FC_NONE = 0,
    FC_ROTATE_XYZ,
    FC_ROTATE_Y,
    FC_LOOKAT_XYZ,
    FC_LOOKAT_Y
};

/// %Camera scene node.
class Camera : public SpatialNode
{
    OBJECT(Camera);

    /// Construct.
    Camera();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Set near clip distance.
    void SetNearClip(float distance) { nearClip = Max(distance, M_EPSILON); }
    /// Set far clip distance.
    void SetFarClip(float distance) { farClip = Max(distance, M_EPSILON); }
    /// Set vertical field of view in degrees.
    void SetFov(float degrees) { fov = Clamp(degrees, 0.0f, 180.0f); }
    /// Set orthographic mode view uniform size.
    void SetOrthoSize(float size) { orthoSize = size; aspectRatio = 1.0f; }
    /// Set orthographic mode view non-uniform size.
    void SetOrthoSize(const Vector2& size) { orthoSize = size.y; aspectRatio = size.x / size.y; }
    /// Set aspect ratio.
    void SetAspectRatio(float ratio) { aspectRatio = Max(ratio, M_EPSILON); }
    /// Set zoom level, where 1 is no zooming.
    void SetZoom(float level) { zoom = Max(level, M_EPSILON); }
    /// Set LOD bias. Values higher than 1 uses higher quality LOD (acts if distance is smaller.)
    void SetLodBias(float bias) { lodBias = Max(bias, M_EPSILON); }
    /// Set view layer mask. Will be checked against scene objects' layers to see what to render.
    void SetViewMask(unsigned mask) { viewMask = mask; }
    /// Set orthographic projection mode.
    void SetOrthographic(bool enable) { orthographic = enable; }
    /// Set reflection mode.
    void SetUseReflection(bool enable) { useReflection = enable; viewMatrixDirty = true; }
    /// Set reflection plane in world space for reflection mode.
    void SetReflectionPlane(const Plane& plane) { reflectionPlane = plane; reflectionMatrix = plane.ReflectionMatrix(); viewMatrixDirty = true; }
    /// Set whether to use a custom clip plane.
    void SetUseClipping(bool enable) { useClipping = enable; }
    /// Set custom clipping plane in world space.
    void SetClipPlane(const Plane& plane) { clipPlane = plane; }
    /// Set vertical flipping mode.
    void SetFlipVertical(bool enable) { flipVertical = enable; }

    /// Return far clip distance.
    float FarClip() const { return farClip; }
    /// Return near clip distance.
    float NearClip() const;
    /// Return vertical field of view in degrees.
    float Fov() const { return fov; }
    /// Return orthographic mode size.
    float OrthoSize() const { return orthoSize; }
    /// Return aspect ratio.
    float AspectRatio() const { return aspectRatio; }
    /// Return zoom.
    float Zoom() const { return zoom; }
    /// Return LOD bias.
    float LodBias() const { return lodBias; }
    /// Return view layer mask.
    unsigned ViewMask() const { return viewMask; }
    /// Return whether is orthographic.
    bool IsOrthographic() const { return orthographic; }
    /// Return whether is using reflection.
    bool UseReflection() const { return useReflection; }
    /// Return the reflection plane.
    const Plane& ReflectionPlane() const { return reflectionPlane; }
    /// Return whether is using a custom clipping plane.
    bool UseClipping() const { return useClipping; }
    /// Return the custom clipping plane.
    const Plane& ClipPlane() const { return clipPlane; }
    /// Return vertical flipping mode.
    bool FlipVertical() const { return flipVertical; }
    /// Return whether to reverse culling; affected by vertical flipping and reflection.
    bool UseReverseCulling() const { return flipVertical ^ useReflection; }
    /// Return frustum in world space.
    Frustum WorldFrustum() const;
    /// Return world space frustum split by custom near and far clip distances.
    Frustum WorldSplitFrustum(float nearClip, float farClip) const;
    /// Return frustum in view space.
    Frustum ViewSpaceFrustum() const;
    /// Return split frustum in view space.
    Frustum ViewSpaceSplitFrustum(float nearClip, float farClip) const;
    /// Return view matrix.
    const Matrix3x4& ViewMatrix() const { if (viewMatrixDirty) { viewMatrix = EffectiveWorldTransform().Inverse(); viewMatrixDirty = false; } return viewMatrix; }
    /// Return either API-specific or API-independent (D3D convention) projection matrix.
    Matrix4 ProjectionMatrix(bool apiSpecific = true) const;
    /// Return frustum near and far sizes.
    void FrustumSize(Vector3& near, Vector3& far) const;
    /// Return half view size.
    float HalfViewSize() const;
    /// Return ray corresponding to normalized screen coordinates (0.0 - 1.0).
    Ray ScreenRay(float x, float y) const;
    // Convert a world space point to normalized screen coordinates (0.0 - 1.0).
    Vector2 WorldToScreenPoint(const Vector3& worldPos) const;
    // Convert normalized screen coordinates (0.0 - 1.0) and depth to a world space point.
    Vector3 ScreenToWorldPoint(const Vector3& screenPos) const;
    /// Return distance to position.
    float Distance(const Vector3& worldPos) const { return (worldPos - WorldPosition()).Length(); }
    /// Return a scene node's LOD scaled distance.
    float LodDistance(float distance, float nodeScale, float bias) const { float d = Max(lodBias * zoom * bias * nodeScale, M_EPSILON); return !orthographic ? distance / d : orthoSize / d; }
    /// Return a world rotation for facing a camera on certain axes based on the existing world rotation.
    Quaternion FaceCameraRotation(const Vector3& position, const Quaternion& rotation, FaceCameraMode mode);
    /// Return effective world transform for matrix and frustum calculations including reflection but excluding node scaling.
    Matrix3x4 EffectiveWorldTransform() const;
    /// Return if projection parameters are valid for rendering and raycasting.
    bool IsProjectionValid() const;

protected:
    /// Handle the transform matrix changing.
    void OnTransformChanged() override;

private:
    /// Set reflection plane as vector. Used in serialization.
    void SetReflectionPlaneAttr(const Vector4& value);
    /// Return reflection plane as vector. Used in serialization.
    Vector4 ReflectionPlaneAttr() const;
    /// Set clipping plane attribute as vector. Used in serialization.
    void SetClipPlaneAttr(const Vector4& value);
    /// Return clipping plane attribute as vector. Used in serialization.
    Vector4 ClipPlaneAttr() const;

    /// Cached view matrix.
    mutable Matrix3x4 viewMatrix;
    /// View matrix dirty flag.
    mutable bool viewMatrixDirty;
    /// Orthographic mode flag.
    bool orthographic;
    /// Flip vertical flag.
    bool flipVertical;
    /// Near clip distance.
    float nearClip;
    /// Far clip distance.
    float farClip;
    /// Field of view.
    float fov;
    /// Orthographic view size.
    float orthoSize;
    /// Aspect ratio.
    float aspectRatio;
    /// Zoom.
    float zoom;
    /// LOD bias.
    float lodBias;
    /// View layer mask.
    unsigned viewMask;
    /// Reflection plane.
    Plane reflectionPlane;
    /// Clipping plane.
    Plane clipPlane;
    /// Reflection matrix calculated from the plane.
    Matrix3x4 reflectionMatrix;
    /// Reflection mode enabled flag.
    bool useReflection;
    /// Use custom clip plane flag.
    bool useClipping;
};
