// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/FrameBuffer.h"
#include "../Graphics/Texture.h"
#include "../IO/Log.h"
#include "../Math/Ray.h"
#include "Camera.h"
#include "Light.h"
#include "Octree.h"
#include "Renderer.h"

#include <tracy/Tracy.hpp>

static const LightType DEFAULT_LIGHTTYPE = LIGHT_POINT;
static const Color DEFAULT_COLOR = Color(1.0f, 1.0f, 1.0f, 0.5f);
static const float DEFAULT_RANGE = 10.0f;
static const float DEFAULT_SPOT_FOV = 30.0f;
static const int DEFAULT_SHADOWMAP_SIZE = 512;
static const float DEFAULT_FADE_START = 0.9f;
static const float DEFAULT_SHADOW_MAX_DISTANCE = 250.0f;
static const float DEFAULT_SHADOW_MAX_STRENGTH = 0.0f;
static const float DEFAULT_SHADOW_QUANTIZE = 0.5f;
static const float DEFAULT_DEPTH_BIAS = 2.0f;
static const float DEFAULT_SLOPESCALE_BIAS = 1.5f;

static const Quaternion pointLightFaceRotations[] = {
    Quaternion(0.0f, 90.0f, 0.0f),
    Quaternion(0.0f, -90.0f, 0.0f),
    Quaternion(-90.0f, 0.0f, 0.0f),
    Quaternion(90.0f, 0.0f, 0.0f),
    Quaternion(0.0f, 0.0f, 0.0f),
    Quaternion(0.0f, 180.0f, 0.0f)
};

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
    fadeStart(DEFAULT_FADE_START),
    shadowMapSize(DEFAULT_SHADOWMAP_SIZE),
    shadowFadeStart(DEFAULT_FADE_START),
    shadowMaxDistance(DEFAULT_SHADOW_MAX_DISTANCE),
    shadowMaxStrength(DEFAULT_SHADOW_MAX_STRENGTH),
    shadowQuantize(DEFAULT_SHADOW_QUANTIZE),
    depthBias(DEFAULT_DEPTH_BIAS),
    slopeScaleBias(DEFAULT_SLOPESCALE_BIAS),
    shadowMap(nullptr)
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
    RegisterDerivedType<Light, OctreeNode>();
    RegisterAttribute("lightType", &Light::LightTypeAttr, &Light::SetLightTypeAttr, (int)DEFAULT_LIGHTTYPE, lightTypeNames);
    RegisterRefAttribute("color", &Light::GetColor, &Light::SetColor, DEFAULT_COLOR);
    RegisterAttribute("range", &Light::Range, &Light::SetRange, DEFAULT_RANGE);
    RegisterAttribute("fov", &Light::Fov, &Light::SetFov, DEFAULT_SPOT_FOV);
    RegisterAttribute("fadeStart", &Light::FadeStart, &Light::SetFadeStart, DEFAULT_FADE_START);
    RegisterAttribute("shadowMapSize", &Light::ShadowMapSize, &Light::SetShadowMapSize, DEFAULT_SHADOWMAP_SIZE);
    RegisterAttribute("shadowFadeStart", &Light::ShadowFadeStart, &Light::SetShadowFadeStart, DEFAULT_FADE_START);
    RegisterAttribute("shadowMaxDistance", &Light::ShadowMaxDistance, &Light::SetShadowMaxDistance, DEFAULT_SHADOW_MAX_DISTANCE);
    RegisterAttribute("shadowMaxStrength", &Light::ShadowMaxStrength, &Light::SetShadowMaxStrength, DEFAULT_SHADOW_MAX_STRENGTH);
    RegisterAttribute("shadowQuantize", &Light::ShadowQuantize, &Light::SetShadowQuantize, DEFAULT_SHADOW_QUANTIZE);
    RegisterAttribute("depthBias", &Light::DepthBias, &Light::SetDepthBias, DEFAULT_DEPTH_BIAS);
    RegisterAttribute("slopeScaleBias", &Light::SlopeScaleBias, &Light::SetSlopeScaleBias, DEFAULT_SLOPESCALE_BIAS);
}

bool Light::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    switch (lightType)
    {
    case LIGHT_DIRECTIONAL:
        distance = 0.0f;
        break;

    case LIGHT_SPOT:
        distance = camera->Distance(WorldPosition() + 0.5f * range * WorldDirection());
        break;

    case LIGHT_POINT:
        distance = camera->Distance(WorldPosition());
        break;
    }

    if (maxDistance > 0.0f && distance > maxDistance)
        return false;

    // If there was a discontinuity in rendering the light, assume cached shadow map content lost
    if (!WasInView(frameNumber))
        SetShadowMap(nullptr);

    lastFrameNumber = frameNumber;
    return true;
}

void Light::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
{
    if (lightType == LIGHT_SPOT)
    {
        float hitDistance = ray.HitDistance(WorldFrustum());
        if (hitDistance <= maxDistance_)
        {
            RaycastResult res;
            res.position = ray.origin + hitDistance * ray.direction;
            res.normal = -ray.direction;
            res.distance = hitDistance;
            res.node = this;
            res.subObject = 0;
            dest.push_back(res);
        }
    }
    else if (lightType == LIGHT_POINT)
    {
        float hitDistance = ray.HitDistance(WorldSphere());
        if (hitDistance <= maxDistance_)
        {
            RaycastResult res;
            res.position = ray.origin + hitDistance * ray.direction;
            res.normal = -ray.direction;
            res.distance = hitDistance;
            res.node = this;
            res.subObject = 0;
            dest.push_back(res);
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

void Light::SetFadeStart(float start)
{
    fadeStart = Clamp(start, 0.0f, 1.0f - M_EPSILON);
}

void Light::SetShadowMapSize(int size)
{
    if (size < 1)
        size = 1;

    shadowMapSize = NextPowerOfTwo(size);
}

void Light::SetShadowFadeStart(float start)
{
    shadowFadeStart = Clamp(start, 0.0f, 1.0f - M_EPSILON);
}

void Light::SetShadowMaxDistance(float distance_)
{
    shadowMaxDistance = Max(distance_, 0.0f);
}

void Light::SetShadowMaxStrength(float strength)
{
    shadowMaxStrength = Clamp(strength, 0.0f, 1.f);
}

void Light::SetShadowQuantize(float quantize)
{
    shadowQuantize = Max(quantize, M_EPSILON);
}

void Light::SetDepthBias(float bias)
{
    depthBias = Max(bias, 0.0f);
}

void Light::SetSlopeScaleBias(float bias)
{
    slopeScaleBias = Max(bias, 0.0f);
}

IntVector2 Light::TotalShadowMapSize() const
{
    if (lightType == LIGHT_DIRECTIONAL)
        return IntVector2(shadowMapSize * 2, shadowMapSize);
    else if (lightType == LIGHT_POINT)
        return IntVector2(shadowMapSize * 3, shadowMapSize * 2);
    else
        return IntVector2(shadowMapSize, shadowMapSize);
}

Color Light::EffectiveColor() const
{
    if (maxDistance > 0.0f)
    {
        float scaledDistance = distance / maxDistance;
        if (scaledDistance >= shadowFadeStart)
            return color.Lerp(Color::BLACK, (scaledDistance - fadeStart) / (1.0f - fadeStart));
    }

    return color;
}

float Light::ShadowStrength() const
{
    if (!CastShadows())
        return 1.0f;

    if (lightType != LIGHT_DIRECTIONAL && shadowMaxDistance > 0.0f)
    {
        float scaledDistance = distance / shadowMaxDistance;
        if (scaledDistance >= shadowFadeStart)
            return Lerp(shadowMaxStrength, 1.0f, (scaledDistance - shadowFadeStart) / (1.0f - shadowFadeStart));
    }

    return shadowMaxStrength;
}

float Light::ShadowSplit(size_t index) const
{
    if (index == 0)
        return 0.25f * shadowMaxDistance;
    else
        return shadowMaxDistance;
}

Vector2 Light::ShadowSplits() const
{
    return Vector2(ShadowSplit(0), ShadowSplit(1));
}

size_t Light::NumShadowViews() const
{
    if (!CastShadows())
        return 0;
    else if (lightType == LIGHT_DIRECTIONAL)
        return 2;
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

void Light::SetShadowMap(Texture* shadowMap_, const IntRect& shadowRect_)
{
    if (!shadowMap)
        shadowViews.clear();

    shadowMap = shadowMap_;
    shadowRect = shadowRect_;
}

void Light::InitShadowViews()
{
    shadowViews.resize(NumShadowViews());

    for (size_t i = 0; i < shadowViews.size(); ++i)
    {
        ShadowView& view = shadowViews[i];
        if (!view.shadowCamera)
        {
            view.shadowCamera = new Camera();
            // OpenGL render-to-texture needs vertical flip
            view.shadowCamera->SetFlipVertical(true);
        }
    }

    // Calculate shadow mapping constants common to all lights
    shadowParameters = Vector4(0.5f / (float)shadowMap->Width(), 0.5f / (float)shadowMap->Height(), ShadowStrength(), 0.0f);
}

void Light::SetupShadowView(size_t viewIndex, Camera* mainCamera)
{
    int numVerticalSplits = lightType == LIGHT_POINT ? 2 : 1;
    int actualShadowMapSize = shadowRect.Height() / numVerticalSplits;
    float pointShadowZoom = (float)(actualShadowMapSize - 4) / (float)actualShadowMapSize;

    ShadowView& view = shadowViews[viewIndex];
    view.light = this;

    Camera* shadowCamera = view.shadowCamera;

    switch (lightType)
    {
    case LIGHT_DIRECTIONAL:
    {
        IntVector2 topLeft(shadowRect.left, shadowRect.top);
        if (viewIndex & 1)
            topLeft.x += actualShadowMapSize;
        view.viewport = IntRect(topLeft.x, topLeft.y, topLeft.x + actualShadowMapSize, topLeft.y + actualShadowMapSize);

        view.splitStart = Max(mainCamera->NearClip(), (viewIndex == 0) ? 0.0f : ShadowSplit(viewIndex - 1));
        view.splitEnd = Min(mainCamera->FarClip(), ShadowSplit(viewIndex));
        float extrusionDistance = mainCamera->FarClip();

        // Calculate initial position & rotation
        shadowCamera->SetTransform(mainCamera->WorldPosition() - extrusionDistance * WorldDirection(), WorldRotation());

        // Calculate main camera shadowed frustum in light's view space
        Frustum splitFrustum = mainCamera->WorldSplitFrustum(view.splitStart, view.splitEnd);
        Frustum lightViewFrustum = splitFrustum.Transformed(shadowCamera->ViewMatrix());

        // Fit the frustum inside a bounding box
        BoundingBox shadowBox;
        shadowBox.Define(lightViewFrustum);

        // If shadow camera is far away from the frustum, can bring it closer for better depth precision
        /// \todo The minimum distance is somewhat arbitrary
        float minDistance = mainCamera->FarClip() * 0.25f;
        if (shadowBox.min.z > minDistance)
        {
            float move = shadowBox.min.z - minDistance;
            shadowCamera->Translate(Vector3(0.0f, 0.f, move));
            shadowBox.min.z -= move,
                shadowBox.max.z -= move;
        }

        shadowCamera->SetOrthographic(true);
        shadowCamera->SetFarClip(shadowBox.max.z);

        Vector3 center = shadowBox.Center();
        Vector3 size = shadowBox.Size();

        size.x = ceilf(sqrtf(size.x / shadowQuantize));
        size.y = ceilf(sqrtf(size.y / shadowQuantize));
        size.x = size.x * size.x * shadowQuantize;
        size.y = size.y * size.y * shadowQuantize;

        shadowCamera->SetOrthoSize(Vector2(size.x, size.y));
        shadowCamera->SetZoom(1.0f);

        // Center shadow camera to the view space bounding box
        Vector3 pos(shadowCamera->WorldPosition());
        Quaternion rot(shadowCamera->WorldRotation());
        Vector3 adjust(center.x, center.y, 0.0f);
        shadowCamera->Translate(rot * adjust, TS_WORLD);

        // Snap to whole texels
        {
            Vector3 viewPos(rot.Inverse() * shadowCamera->WorldPosition());
            float invSize = 4.0f / actualShadowMapSize;
            Vector2 texelSize(size.x * invSize, size.y * invSize);
            Vector3 snap(-fmodf(viewPos.x, texelSize.x), -fmodf(viewPos.y, texelSize.y), 0.0f);
            shadowCamera->Translate(rot * snap, TS_WORLD);
        }

        view.shadowFrustum = shadowCamera->WorldFrustum();
    }
    break;

    case LIGHT_POINT:
    {
        IntVector2 topLeft(shadowRect.left, shadowRect.top);
        if (viewIndex & 1)
            topLeft.y += actualShadowMapSize;
        topLeft.x += ((unsigned)viewIndex >> 1) * actualShadowMapSize;
        view.viewport = IntRect(topLeft.x, topLeft.y, topLeft.x + actualShadowMapSize, topLeft.y + actualShadowMapSize);

        shadowCamera->SetTransform(WorldPosition(), pointLightFaceRotations[viewIndex]);
        shadowCamera->SetFov(90.0f);
        shadowCamera->SetZoom(pointShadowZoom);
        shadowCamera->SetFarClip(Range());
        shadowCamera->SetNearClip(Range() * 0.01f);
        shadowCamera->SetOrthographic(false);
        shadowCamera->SetAspectRatio(1.0f);
        view.shadowFrustum = shadowCamera->WorldFrustum();
    }
    break;

    case LIGHT_SPOT:
        view.viewport = shadowRect;
        shadowCamera->SetTransform(WorldPosition(), WorldRotation());
        shadowCamera->SetFov(fov);
        shadowCamera->SetZoom(1.0f);
        shadowCamera->SetFarClip(Range());
        shadowCamera->SetNearClip(Range() * 0.01f);
        shadowCamera->SetOrthographic(false);
        shadowCamera->SetAspectRatio(1.0f);
        view.shadowFrustum = shadowCamera->WorldFrustum();
        break;
    }

    // Setup shadow matrices now as camera positions have been finalized
    if (lightType != LIGHT_POINT)
    {
        float width = (float)shadowMap->Width();
        float height = (float)shadowMap->Height();
        Vector3 viewOffset((float)view.viewport.left / width, (float)view.viewport.top / height, 0.0f);
        Vector3 viewScale(0.5f * (float)view.viewport.Width() / width, 0.5f * (float)view.viewport.Height() / height, 1.0f);

        viewOffset.x += viewScale.x;
        viewOffset.y += viewScale.y;

        // OpenGL has different depth range
        viewOffset.z = 0.5f;
        viewScale.z = 0.5f;

        Matrix4 texAdjust(Matrix4::IDENTITY);
        texAdjust.SetTranslation(viewOffset);
        texAdjust.SetScale(viewScale);

        view.shadowMatrix = texAdjust * shadowCamera->ProjectionMatrix() * shadowCamera->ViewMatrix();
    }
    else
    {
        if (!viewIndex)
        {
            Vector3 worldPosition = WorldPosition();
            Vector2 textureSize((float)shadowMap->Width(), (float)shadowMap->Height());
            float nearClip = Range() * 0.01f;
            float farClip = Range();
            float q = farClip / (farClip - nearClip);
            float r = -q * nearClip;

            Matrix4& shadowMatrix = view.shadowMatrix;
            shadowMatrix.m00 = actualShadowMapSize / textureSize.x;
            shadowMatrix.m01 = actualShadowMapSize / textureSize.y;
            shadowMatrix.m02 = (float)shadowRect.left / textureSize.x;
            shadowMatrix.m03 = (float)shadowRect.top / textureSize.y;
            shadowMatrix.m10 = pointShadowZoom;
            shadowMatrix.m11 = q;
            shadowMatrix.m12 = r;
            // Put position here for invalidating dynamic light shadowmaps when the light moves
            shadowMatrix.m20 = worldPosition.x;
            shadowMatrix.m21 = worldPosition.y;
            shadowMatrix.m22 = worldPosition.z;
        }
        else
        {
            // Shadow matrix for the rest of the cubemap sides is copied from the first
            view.shadowMatrix = shadowViews[0].shadowMatrix;
        }
    }
}

void Light::OnWorldBoundingBoxUpdate() const
{
    switch (lightType)
    {
    case LIGHT_DIRECTIONAL:
        // Directional light always sets humongous bounding box not affected by transform
        worldBoundingBox.Define(-M_MAX_FLOAT, M_MAX_FLOAT);
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
