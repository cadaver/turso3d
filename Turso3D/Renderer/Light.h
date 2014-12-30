// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Math/Sphere.h"
#include "OctreeNode.h"

namespace Turso3D
{

/// %Light types.
enum LightType
{
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT,
    LIGHT_SPOT
};

/// Dynamic light scene node.
class TURSO3D_API Light : public OctreeNode
{
    OBJECT(Light);
    
public:
    /// Construct.
    Light();
    /// Destruct.
    virtual ~Light();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Perform ray test on self and add possible hit to the result vector.
    void OnRaycast(Vector<RaycastResult>& dest, const Ray& ray, float maxDistance) override;

    /// Set light type.
    void SetLightType(LightType type);
    /// Set color. Alpha component contains specular intensity.
    void SetColor(const Color& color);
    /// Set range.
    void SetRange(float range);
    /// Set spotlight field of view.
    void SetFov(float fov);
    /// Set light layer mask. Will be checked against scene objects' layers to see what objects to illuminate.
    void SetLightMask(unsigned mask);

    /// Return light type.
    LightType GetLightType() const { return lightType; }
    /// Return color.
    const Color& GetColor() const { return color; }
    /// Return range.
    float Range() const { return range; }
    /// Return spotlight field of view.
    float Fov() const { return fov; }
    /// Return light layer mask.
    unsigned LightMask() const { return lightMask; }
    /// Return spotlight world space frustum.
    Frustum WorldFrustum() const;
    /// Return point light world space sphere.
    Sphere WorldSphere() const;

protected:
    /// Recalculate the world space bounding box.
    virtual void OnWorldBoundingBoxUpdate() const override;

private:
    /// Set light type as int. Used in serialization.
    void SetLightTypeAttr(int lightType);
    /// Return light type as int. Used in serialization.
    int LightTypeAttr() const;

    /// Light type.
    LightType lightType;
    /// Light color.
    Color color;
    /// Range.
    float range;
    /// Spotlight field of view.
    float fov;
    /// Light layer mask.
    unsigned lightMask;
};

}
