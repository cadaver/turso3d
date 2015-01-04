// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Math/IntRect.h"
#include "../Math/IntVector2.h"
#include "../Math/Sphere.h"
#include "OctreeNode.h"

namespace Turso3D
{

class Texture;
struct ShadowView;

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

    /// Prepare object for rendering. Reset framenumber and calculate distance from camera. Called by Renderer.
    void OnPrepareRender(unsigned frameNumber, Camera* camera) override;
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
    /// Set shadow map face resolution in pixels.
    void SetShadowMapSize(int size);
    /// Set directional light shadow split distances. Fill unused splits with zero.
    void SetShadowSplits(const Vector4& splits);
    /// Set directional light shadow fade start depth, where 1 represents shadow max distance.
    void SetShadowFadeStart(float start);
    /// Set constant depth bias for shadows.
    void SetDepthBias(int bias);
    /// Set slope-scaled depth bias for shadows.
    void SetSlopeScaledDepthBias(float bias);

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
    /// Return shadow map face resolution in pixels.
    int ShadowMapSize() const { return shadowMapSize; }
    /// Return directional light shadow split distances.
    const Vector4& ShadowSplits() const { return shadowSplits; }
    /// Return directional light shadow fade start depth.
    float ShadowFadeStart() const { return shadowFadeStart; }
    /// Return number of directional light shadow splits.
    int NumShadowSplits() const;
    /// Return shadow split distance by index.
    float ShadowSplit(size_t index) const;
    /// Return shadow maximum distance.
    float MaxShadowDistance() const;
    /// Return constant depth bias.
    int DepthBias() const { return depthBias; }
    /// Return slope-scaled depth bias.
    float SlopeScaledDepthBias() const { return slopeScaledDepthBias; }
    /// Return total requested shadow map size, accounting for multiple faces / splits for directional and point lights.
    IntVector2 TotalShadowMapSize() const;
    /// Return number of required shadow views / cameras.
    size_t NumShadowViews() const;
    /// Return number of required shadow coordinates in the vertex shader.
    size_t NumShadowCoords() const;
    /// Return spotlight world space frustum.
    Frustum WorldFrustum() const;
    /// Return point light world space sphere.
    Sphere WorldSphere() const;

    /// Set shadow map and viewport within it. Called by Renderer.
    void SetShadowMap(Texture* shadowMap, const IntRect& shadowRect = IntRect::ZERO);
    /// Setup the shadow cameras and viewports. Called by Renderer.
    void SetupShadowViews(Camera* mainCamera);
    /// Setup the shadow matrix constants. Called by Renderer.
    void SetupShadowMatrices(Matrix4* dest, size_t& destIndex);
    /// Reset shadow views and shadow map in case light will be rendered unshadowed. Called by Renderer.
    void ResetShadowViews();
    /// Return shadow map.
    Texture* ShadowMap() const { return shadowMap; }
    /// Return actual shadow map rectangle. May be smaller than the requested total shadow map size.
    const IntRect& ShadowRect() const { return shadowRect; }
    /// Return shadow views.
    const Vector<AutoPtr<ShadowView> >& ShadowViews() const { return shadowViews; }
    /// Return shadow camera by index.
    Camera* ShadowCamera(size_t index) const;

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
    /// Shadow map resolution in pixels.
    int shadowMapSize;
    /// Directional shadow splits.
    Vector4 shadowSplits;
    /// Directional shadow fade start.
    float shadowFadeStart;
    /// Constant depth bias.
    int depthBias;
    /// Slope-scaled depth bias.
    float slopeScaledDepthBias;
    /// Current shadow map texture.
    Texture* shadowMap;
    /// Rectangle within the shadow map.
    IntRect shadowRect;
    /// Shadow views used by the light. Created on demand.
    Vector<AutoPtr<ShadowView> > shadowViews;
};

}
