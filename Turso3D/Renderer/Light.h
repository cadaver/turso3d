// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Math/IntRect.h"
#include "../Math/IntVector2.h"
#include "../Math/Sphere.h"
#include "OctreeNode.h"

class Camera;
class GeometryNode;
class LightDrawable;
class Texture;
struct ShadowView;

/// %Light types.
enum LightType
{
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT,
    LIGHT_SPOT
};

/// Shadow map render modes
enum ShadowRenderMode
{
    RENDER_DYNAMIC_LIGHT,
    RENDER_STATIC_LIGHT_STORE_STATIC,
    RENDER_STATIC_LIGHT_RESTORE_STATIC,
    RENDER_STATIC_LIGHT_CACHED
};

/// Shadow rendering view data structure.
struct ShadowView
{
    /// Default construct.
    ShadowView() :
        lastViewport(IntRect::ZERO)
    {
    }

    /// %Light drawable associated with the view.
    LightDrawable* light;
    /// Viewport within the shadow map.
    IntRect viewport;
    /// Shadow camera.
    SharedPtr<Camera> shadowCamera;
    /// Shadow camera frustum.
    Frustum shadowFrustum;
    /// Current shadow projection matrix.
    Matrix4 shadowMatrix;
    /// Shadow render mode to use.
    ShadowRenderMode renderMode;
    /// Directional light split near Z.
    float splitMinZ;
    /// Directional light split far Z.
    float splitMaxZ;
    /// Static object batch queue index in the shadowmap.
    size_t staticQueueIdx;
    /// Dynamic object batch queue index in the shadowmap.
    size_t dynamicQueueIdx;
    /// Shadow caster list index in the shadowmap.
    size_t casterListIdx;
    /// Last viewport used in shadow map render.
    IntRect lastViewport;
    /// Last shadow projection matrix.
    Matrix4 lastShadowMatrix;
    /// Last amount of geometries passed in for shadow map render.
    size_t lastNumGeometries;
};

/// %Light drawable.
class LightDrawable : public Drawable
{
    friend class Light;

public:
    /// Construct.
    LightDrawable();

    /// Recalculate the world space bounding box.
    virtual void OnWorldBoundingBoxUpdate() const override;
    /// Prepare object for rendering. Reset framenumber and calculate distance from camera. Called by Renderer in worker threads. Return false if should not render.
    bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
    /// Perform ray test on self and add possible hit to the result vector.
    void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance) override;
    /// Add debug geometry to be rendered.
    void OnRenderDebug(DebugRenderer* debug) override;

    /// Return light type.
    LightType GetLightType() const { return lightType; }
    /// Return color.
    const Color& GetColor() const { return color; }
    /// Return effective color taking distance fade into account.
    Color EffectiveColor() const;
    /// Return range.
    float Range() const { return range; }
    /// Return spotlight field of view.
    float Fov() const { return fov; }
    /// Return fade start as a function of max draw distance.
    float FadeStart() const { return fadeStart; }
    /// Return shadow map face resolution in pixels.
    int ShadowMapSize() const { return shadowMapSize; }
    /// Return directional light shadow cascade absolute end distances.
    Vector2 ShadowCascadeSplits() const;
    /// Return light shadow fade start as a function of max shadow distance.
    float ShadowFadeStart() const { return shadowFadeStart; }
    /// Return directional light cascade split distance as a function of max shadow distance.
    float ShadowCascadeSplit() const { return shadowCascadeSplit; }
    /// Return maximum distance for shadow rendering.
    float ShadowMaxDistance() const { return shadowMaxDistance; }
    /// Return maximum shadow strength.
    float ShadowMaxStrength() const { return shadowMaxStrength; }
    /// Return directional light shadow view quantization step.
    float ShadowQuantize() const { return shadowQuantize; }
    /// Return directional light shadow view minimum size.
    float ShadowMinView() const { return shadowMinView; }
    /// Return effective shadow strength from current distance.
    float ShadowStrength() const;
    /// Return constant depth bias.
    float DepthBias() const { return depthBias; }
    /// Return slope-scaled depth bias.
    float SlopeScaleBias() const { return slopeScaleBias; }
    /// Return total requested shadow map size, accounting for multiple faces / splits for directional and point lights.
    IntVector2 TotalShadowMapSize() const;
    /// Return actual shadow map face size.
    int ActualShadowMapSize() const { return lightType == LIGHT_POINT ? shadowRect.Height() / 2 : shadowRect.Height(); }
    /// Return number of required shadow views / cameras.
    size_t NumShadowViews() const;
    /// Return spotlight world space frustum.
    Frustum WorldFrustum() const;
    /// Return point light world space sphere.
    Sphere WorldSphere() const;

    /// Set shadow map and viewport within it. Called by Renderer.
    void SetShadowMap(Texture* shadowMap, const IntRect& shadowRect = IntRect::ZERO);
    /// Init the correct number of shadow views but do not setup them yet. Called by Renderer. Must be called from the same thread for all lights because new Camera nodes are allocated on first call, which uses the non-threadsafe NodeImpl allocator.
    void InitShadowViews();
    /// Setup the camera and parameters for a shadow view. Directional light shadow view should be supplied the scene bounds for focusing. Return false if the view is empty and should not render. Called by Renderer.
    bool SetupShadowView(size_t viewIndex, Camera* mainCamera, const BoundingBox* geometryBounds = nullptr);
    /// Return shadow map.
    Texture* ShadowMap() const { return shadowMap; }
    /// Return the shadow views.
    std::vector<ShadowView>& ShadowViews() { return shadowViews; }
    /// Return actual shadow map rectangle. May be smaller than the requested total shadow map size.
    const IntRect& ShadowRect() const { return shadowRect; }
    /// Return shadow map offset and depth parameters.
    const Vector4& ShadowParameters() const { return shadowParameters; }

private:
    /// %Light type.
    LightType lightType;
    /// %Light color.
    Color color;
    /// Range.
    float range;
    /// Spotlight field of view.
    float fov;
    /// Fade start as a function of max distance.
    float fadeStart;
    /// Shadow map resolution in pixels.
    int shadowMapSize;
    /// Shadow fade start as a function of max distance.
    float shadowFadeStart;
    /// Directional light shadow cascade split as a function of max distance.
    float shadowCascadeSplit;
    /// Shadow rendering max distance.
    float shadowMaxDistance;
    /// Shadow max strength when not faded.
    float shadowMaxStrength;
    /// Shadow view quantization step for directional lights.
    float shadowQuantize;
    /// Shadow view minimum size for directional lights.
    float shadowMinView;
    /// Constant depth bias for shadows.
    float depthBias;
    /// Slope-sclaed depth bias for shadows.
    float slopeScaleBias;
    /// Current shadow map texture.
    Texture* shadowMap;
    /// Rectangle within the shadow map.
    IntRect shadowRect;
    /// Shadow views.
    std::vector<ShadowView> shadowViews;
    /// Shadow mapping parameters.
    Vector4 shadowParameters;
};

/// Dynamic light scene node.
class Light : public OctreeNode
{
    OBJECT(Light);
    
public:
    /// Construct.
    Light();
    /// Destruct.
    virtual ~Light();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Set light type.
    void SetLightType(LightType type);
    /// Set color. Alpha component contains specular intensity.
    void SetColor(const Color& color);
    /// Set range.
    void SetRange(float range);
    /// Set spotlight field of view.
    void SetFov(float fov);
    /// Set fade start distance, where 1 represents max draw distance. Only has effect when max draw distance has been defined.
    void SetFadeStart(float start);
    /// Set shadow map face resolution in pixels.
    void SetShadowMapSize(int size);
    /// Set light shadow fade start distance, where 1 represents shadow max distance.
    void SetShadowFadeStart(float start);
    /// Set the directional light cascade split distance, where 1 represents shadow max distance.
    void SetShadowCascadeSplit(float split);
    /// Set maximum distance for shadow rendering.
    void SetShadowMaxDistance(float distance);
    /// Set maximum (when not faded) shadow strength (default 0 = fully dark).
    void SetShadowMaxStrength(float strength);
    /// Set directional light shadow view quantization step.
    void SetShadowQuantize(float quantize);
    /// Set directional light shadow view minimum size. This is to prevent unwanted too sharp focusing e.g. when camera is facing the ground.
    void SetShadowMinView(float minView);
    /// Set constant depth bias for shadows.
    void SetDepthBias(float bias);
    /// Set slope-scaled depth bias for shadows.
    void SetSlopeScaleBias(float bias);

    /// Return light type.
    LightType GetLightType() const { return static_cast<LightDrawable*>(drawable)->lightType; }
    /// Return color.
    const Color& GetColor() const { return static_cast<LightDrawable*>(drawable)->color; }
    /// Return range.
    float Range() const { return static_cast<LightDrawable*>(drawable)->range; }
    /// Return spotlight field of view.
    float Fov() const { return static_cast<LightDrawable*>(drawable)->fov; }
    /// Return fade start as a function of max draw distance.
    float FadeStart() const { return static_cast<LightDrawable*>(drawable)->fadeStart; }
    /// Return shadow map face resolution in pixels.
    int ShadowMapSize() const { return static_cast<LightDrawable*>(drawable)->shadowMapSize; }
    /// Return directional light shadow cascade absolute end distances.
    Vector2 ShadowCascadeSplits() const { return static_cast<LightDrawable*>(drawable)->ShadowCascadeSplits(); }
    /// Return light shadow fade start as a function of max shadow distance.
    float ShadowFadeStart() const { return static_cast<LightDrawable*>(drawable)->shadowFadeStart; }
    /// Return directional light cascade split distance as a function of max shadow distance.
    float ShadowCascadeSplit() const { return static_cast<LightDrawable*>(drawable)->shadowCascadeSplit; }
    /// Return maximum distance for shadow rendering.
    float ShadowMaxDistance() const { return static_cast<LightDrawable*>(drawable)->shadowMaxDistance; }
    /// Return maximum shadow strength.
    float ShadowMaxStrength() const { return static_cast<LightDrawable*>(drawable)->shadowMaxStrength; }
    /// Return directional light shadow view quantization step.
    float ShadowQuantize() const { return static_cast<LightDrawable*>(drawable)->shadowQuantize; }
    /// Return directional light shadow view minimum size.
    float ShadowMinView() const { return static_cast<LightDrawable*>(drawable)->shadowMinView; }
    /// Return constant depth bias.
    float DepthBias() const { return static_cast<LightDrawable*>(drawable)->depthBias; }
    /// Return slope-scaled depth bias.
    float SlopeScaleBias() const { return static_cast<LightDrawable*>(drawable)->slopeScaleBias; }
    /// Return spotlight world space frustum.
    Frustum WorldFrustum() const { return static_cast<LightDrawable*>(drawable)->WorldFrustum(); }
    /// Return point light world space sphere.
    Sphere WorldSphere() const { return static_cast<LightDrawable*>(drawable)->WorldSphere(); }

private:
    /// Set light type as int. Used in serialization.
    void SetLightTypeAttr(int lightType);
    /// Return light type as int. Used in serialization.
    int LightTypeAttr() const;
};
