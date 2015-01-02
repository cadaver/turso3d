// For conditions of distribution and use, see copyright notice in License.txt

#include "Light.h"

#include "../Debug/DebugNew.h"
#include "../Math/Ray.h"
#include "Camera.h"
#include "Octree.h"
#include "Renderer.h"

namespace Turso3D
{

static const LightType DEFAULT_LIGHTTYPE = LIGHT_POINT;
static const Color DEFAULT_COLOR = Color(1.0f, 1.0f, 1.0f, 0.5f);
static const float DEFAULT_RANGE = 10.0f;
static const float DEFAULT_SPOT_FOV = 30.0f;
static const int DEFAULT_SHADOWMAP_SIZE = 512;
static const int DEFAULT_CASCADE_SPLITS = 3;
static const float DEFAULT_CASCADE_LAMBDA = 0.8f;

static const char* lightTypeNames[] =
{
    "directional",
    "point",
    "spot",
    0
};

Light::Light() :
    lightType(DEFAULT_LIGHTTYPE),
    color(DEFAULT_COLOR),
    range(DEFAULT_RANGE),
    fov(DEFAULT_SPOT_FOV),
    lightMask(M_MAX_UNSIGNED),
    shadowMapSize(DEFAULT_SHADOWMAP_SIZE),
    numCascadeSplits(DEFAULT_CASCADE_SPLITS),
    cascadeLambda(DEFAULT_CASCADE_LAMBDA),
    shadowMap(nullptr),
    hasReceivers(false)
{
    SetFlag(NF_LIGHT, true);
}

Light::~Light()
{
}

void Light::RegisterObject()
{
    RegisterFactory<Light>();

    CopyBaseAttributes<Light, OctreeNode>();
    RegisterAttribute("lightType", &Light::LightTypeAttr, &Light::SetLightTypeAttr, (int)DEFAULT_LIGHTTYPE, lightTypeNames);
    RegisterRefAttribute("color", &Light::GetColor, &Light::SetColor, DEFAULT_COLOR);
    RegisterAttribute("range", &Light::Range, &Light::SetRange, DEFAULT_RANGE);
    RegisterAttribute("fov", &Light::Fov, &Light::SetFov, DEFAULT_SPOT_FOV);
    RegisterAttribute("lightMask", &Light::LightMask, &Light::SetLightMask, M_MAX_UNSIGNED);
    RegisterAttribute("shadowMapSize", &Light::ShadowMapSize, &Light::SetShadowMapSize, DEFAULT_SHADOWMAP_SIZE);
    RegisterAttribute("numCascadeSplits", &Light::NumCascadesplits, &Light::SetNumCascadeSplits, DEFAULT_CASCADE_SPLITS);
    RegisterAttribute("cascadeLambda", &Light::CascadeLambda, &Light::SetCascadeLambda, DEFAULT_CASCADE_LAMBDA);
}

void Light::OnRaycast(Vector<RaycastResult>& dest, const Ray& ray, float maxDistance)
{
    if (lightType == LIGHT_SPOT)
    {
        float distance = ray.HitDistance(WorldFrustum());
        if (distance <= maxDistance)
        {
            RaycastResult res;
            res.position = ray.origin + distance * ray.direction;
            res.normal = -ray.direction;
            res.distance = distance;
            res.node = this;
            res.extraData = nullptr;
            dest.Push(res);
        }
    }
    else if (lightType == LIGHT_POINT)
    {
        float distance = ray.HitDistance(WorldSphere());
        if (distance <= maxDistance)
        {
            RaycastResult res;
            res.position = ray.origin + distance * ray.direction;
            res.normal = -ray.direction;
            res.distance = distance;
            res.node = this;
            res.extraData = nullptr;
            dest.Push(res);
        }
    }
}

void Light::SetLightType(LightType type)
{
    if (type != lightType)
    {
        lightType = type;
        // Bounding box will change
        OctreeNode::OnTransformChanged();
    }
}

void Light::SetColor(const Color& color_)
{
    color = color_;
}

void Light::SetRange(float range_)
{
    range_ = Max(range_, 0.0f);
    if (range_ != range)
    {
        range = range_;
        // Bounding box will change
        OctreeNode::OnTransformChanged();
    }
}

void Light::SetFov(float fov_)
{
    fov_ = Clamp(fov_, 0.0f, 180.0f);
    if (fov_ != fov)
    {
        fov = fov_;
        // Bounding box will change
        OctreeNode::OnTransformChanged();
    }
}

void Light::SetLightMask(unsigned lightMask_)
{
    lightMask = lightMask_;
}

void Light::SetShadowMapSize(int size)
{
    if (size < 1)
        size = 1;

    shadowMapSize = NextPowerOfTwo(size);
}

void Light::SetNumCascadeSplits(int num)
{
    numCascadeSplits = Clamp(num, 1, 4);
}

void Light::SetCascadeLambda(float lambda)
{
    cascadeLambda = Clamp(lambda, 0.1f, 1.0f);
}

IntVector2 Light::TotalShadowMapSize() const
{
    if (lightType == LIGHT_DIRECTIONAL)
    {
        if (numCascadeSplits == 1)
            return IntVector2(shadowMapSize, shadowMapSize);
        else if (numCascadeSplits == 2)
            return IntVector2(shadowMapSize * 2, shadowMapSize);
        else
            return IntVector2(shadowMapSize * 2, shadowMapSize * 2);
    }
    else if (lightType == LIGHT_POINT)
        return IntVector2(shadowMapSize * 3, shadowMapSize * 2);
    else
        return IntVector2(shadowMapSize, shadowMapSize);
}

size_t Light::NumShadowViews() const
{
    if (lightType == LIGHT_DIRECTIONAL)
        return numCascadeSplits;
    else if (lightType == LIGHT_POINT)
        return 6;
    else
        return 1;
}

Frustum Light::WorldFrustum() const
{
    const Matrix3x4& transform = WorldTransform();
    Matrix3x4 frustumTransform(transform.Translation(), transform.Rotation(), 1.0f);
    Frustum ret;
    ret.Define(fov, 1.0f, 1.0f, 0.0f, range, frustumTransform);
    return ret;
}

Sphere Light::WorldSphere() const
{
    return Sphere(WorldPosition(), range);
}

void Light::SetupShadowView(size_t index, ShadowView& view, const IntRect& shadowRect)
{
    Camera* camera = view.shadowCamera;

    switch (lightType)
    {
    case LIGHT_SPOT:
        camera->SetPosition(WorldPosition());
        camera->SetRotation(WorldRotation());
        camera->SetFarClip(Range());
        camera->SetNearClip(Range() * 0.001f);
        camera->SetOrthographic(false);
        camera->SetAspectRatio(1.0f);
        view.viewport = shadowRect;
        break;
    }
}

void Light::OnWorldBoundingBoxUpdate() const
{
    switch (lightType)
    {
    case LIGHT_DIRECTIONAL:
        // Directional light always sets humongous bounding box not affected by transform
        worldBoundingBox.Define(-1000000000.0f, 1000000000.0f);
        break;
        
    case LIGHT_POINT:
        {
            const Vector3& center = WorldPosition();
            Vector3 edge(range, range, range);
            worldBoundingBox.Define(center - edge, center + edge);
        }
        break;
        
    case LIGHT_SPOT:
        worldBoundingBox.Define(WorldFrustum());
        break;
    }

    SetFlag(NF_BOUNDING_BOX_DIRTY, false);
}

void Light::SetLightTypeAttr(int type)
{
    if (type <= LIGHT_SPOT)
        SetLightType((LightType)type);
}

int Light::LightTypeAttr() const
{
    return (int)lightType;
}

}

