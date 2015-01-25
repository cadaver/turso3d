// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Shader.h"
#include "../Graphics/ShaderVariation.h"
#include "../Graphics/Texture.h"
#include "../Graphics/VertexBuffer.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "Light.h"
#include "Material.h"
#include "Model.h"
#include "Octree.h"
#include "Renderer.h"
#include "StaticModel.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static const unsigned LVS_GEOMETRY = (0x1 | 0x2);
static const unsigned LVS_NUMSHADOWCOORDS = (0x4 | 0x8 | 0x10);

static const unsigned LPS_AMBIENT = 0x1;
static const unsigned LPS_NUMSHADOWCOORDS = (0x2 | 0x4 | 0x8);
static const unsigned LPS_LIGHT0 = (0x10 | 0x20 | 0x40);
static const unsigned LPS_LIGHT1 = (0x80 | 0x100 | 0x200);
static const unsigned LPS_LIGHT2 = (0x400 | 0x800 | 0x1000);
static const unsigned LPS_LIGHT3 = (0x2000 | 0x4000 | 0x8000);

static const CullMode cullModeFlip[] =
{
    CULL_NONE,
    CULL_NONE,
    CULL_BACK,
    CULL_FRONT
};

const String geometryDefines[] =
{
    "",
    "INSTANCED"
};

const String lightDefines[] = 
{
    "AMBIENT",
    "NUMSHADOWCOORDS",
    "DIRLIGHT",
    "POINTLIGHT",
    "SPOTLIGHT",
    "SHADOW"
};

inline bool CompareLights(Light* lhs, Light* rhs)
{
    return lhs->Distance() < rhs->Distance();
}

Renderer::Renderer() :
    frameNumber(0),
    instanceTransformsDirty(false)
{
}

Renderer::~Renderer()
{
}

void Renderer::SetupShadowMaps(size_t num, int size, ImageFormat format)
{
    if (size < 1)
        size = 1;
    size = NextPowerOfTwo(size);

    shadowMaps.Resize(num);
    for (auto it = shadowMaps.Begin(); it != shadowMaps.End(); ++it)
    {
        if (it->texture->Define(TEX_2D, USAGE_RENDERTARGET, IntVector2(size, size), format, 1))
        {
            // Setup shadow map sampling with hardware depth compare
            it->texture->DefineSampler(COMPARE_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP, 1);
        }
    }
}

bool Renderer::PrepareView(Scene* scene_, Camera* camera_, const Vector<PassDesc>& passes)
{
    if (!CollectObjects(scene_, camera_))
        return false;
    
    CollectLightInteractions();
    CollectBatches(passes);
    return true;
}

bool Renderer::CollectObjects(Scene* scene_, Camera* camera_)
{
    PROFILE(CollectObjects);

    // Acquire Graphics subsystem now, which needs to be initialized with a screen mode
    if (!graphics)
        Initialize();

    geometries.Clear();
    lights.Clear();
    instanceTransforms.Clear();
    lightLists.Clear();
    lightPasses.Clear();
    for (auto it = batchQueues.Begin(); it != batchQueues.End(); ++it)
        it->second.Clear();
    for (auto it = shadowMaps.Begin(); it != shadowMaps.End(); ++it)
        it->Clear();
    usedShadowViews = 0;

    scene = scene_;
    camera = camera_;
    octree = scene ? scene->FindChild<Octree>() : nullptr;
    if (!scene || !camera || !octree)
        return false;

    // Increment frame number. Never use 0, as that is the default for objects that have never been rendered
    ++frameNumber;
    if (!frameNumber)
        ++frameNumber;

    // Reinsert moved objects to the octree
    octree->Update();

    frustum = camera->WorldFrustum();
    viewMask = camera->ViewMask();
    octree->FindNodes(frustum, this, &Renderer::CollectGeometriesAndLights);

    return true;
}

void Renderer::CollectLightInteractions()
{
    PROFILE(CollectLightInteractions);

    {
        // Sort lights by increasing distance. Directional lights will have distance 0 which ensure they have the first
        // opportunity to allocate shadow maps
        PROFILE(SortLights);
        Sort(lights.Begin(), lights.End(), CompareLights);
    }

    for (auto it = lights.Begin(), end = lights.End(); it != end; ++it)
    {
        Light* light = *it;
        unsigned lightMask = light->LightMask();

        litGeometries.Clear();
        bool hasReceivers = false;

        // Create a light list that contains only this light. It will be used for nodes that have no light interactions so far
        unsigned long long key = (unsigned long long)light;
        LightList* lightList = &lightLists[key];
        lightList->lights.Push(light);
        lightList->key = key;
        lightList->useCount = 0;
        
        switch (light->GetLightType())
        {
        case LIGHT_DIRECTIONAL:
            for (auto gIt = geometries.Begin(), gEnd = geometries.End(); gIt != gEnd; ++gIt)
            {
                GeometryNode* node = *gIt;
                if (node->LayerMask() & lightMask)
                {
                    AddLightToNode(node, light, lightList);
                    hasReceivers = true;
                }
            }
            break;

        case LIGHT_POINT:
            octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(litGeometries), light->WorldSphere(), NF_ENABLED |
                NF_GEOMETRY, lightMask);
            for (auto gIt = litGeometries.Begin(), gEnd = litGeometries.End(); gIt != gEnd; ++gIt)
            {
                GeometryNode* node = *gIt;
                // Add light only to nodes which are actually inside the frustum this frame
                if (node->LastFrameNumber() == frameNumber)
                {
                    AddLightToNode(node, light, lightList);
                    hasReceivers = true;
                }
            }
            break;

        case LIGHT_SPOT:
            octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(litGeometries), light->WorldFrustum(), NF_ENABLED |
                NF_GEOMETRY, lightMask);
            for (auto gIt = litGeometries.Begin(), gEnd = litGeometries.End(); gIt != gEnd; ++gIt)
            {
                GeometryNode* node = *gIt;
                if (node->LastFrameNumber() == frameNumber)
                {
                    AddLightToNode(node, light, lightList);
                    hasReceivers = true;
                }
            }
            break;
        }

        if (!light->CastShadows() || !hasReceivers)
        {
            light->SetShadowMap(nullptr);
            continue;
        }

        // Try to allocate shadow map rectangle. Retry with smaller size two times if fails
        IntVector2 request = light->TotalShadowMapSize();
        IntRect shadowRect;
        size_t retries = 3;
        size_t index = 0;

        while (retries--)
        {
            for (index = 0; index < shadowMaps.Size(); ++index)
            {
                ShadowMap& shadowMap = shadowMaps[index];
                int x, y;
                if (shadowMap.allocator.Allocate(request.x, request.y, x, y))
                {
                    light->SetShadowMap(shadowMaps[index].texture, IntRect(x, y, x + request.x, y + request.y));
                    break;
                }
            }

            if (index < shadowMaps.Size())
                break;
            else
            {
                request.x /= 2;
                request.y /= 2;
            }
        }

        // If no room in any shadow map, render unshadowed
        if (index >= shadowMaps.Size())
        {
            light->SetShadowMap(nullptr);
            continue;
        }

        // Setup shadow cameras & find shadow casters
        size_t startIndex = usedShadowViews;
        light->SetupShadowViews(camera, shadowViews, usedShadowViews);
        bool hasShadowBatches = false;

        for (size_t i = startIndex; i < usedShadowViews; ++i)
        {
            ShadowView* view = shadowViews[i].Get();
            Frustum shadowFrustum = view->shadowCamera.WorldFrustum();
            BatchQueue& shadowQueue = view->shadowQueue;
            shadowQueue.sort = SORT_STATE;
            shadowQueue.lit = false;
            shadowQueue.baseIndex = Material::PassIndex("shadow");
            shadowQueue.additiveIndex = 0;

            switch (light->GetLightType())
            {
            case LIGHT_DIRECTIONAL:
                // Directional light needs a new frustum query for each split, as the shadow cameras are typically far outside
                // the main view
                litGeometries.Clear();
                octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(litGeometries),
                    shadowFrustum, NF_ENABLED | NF_GEOMETRY | NF_CASTSHADOWS, light->LightMask());
                CollectShadowBatches(litGeometries, shadowQueue, shadowFrustum, false, false);
                break;

            case LIGHT_POINT:
                // Check which lit geometries are shadow casters and inside each shadow frustum. First check whether the
                // shadow frustum is inside the view at all
                /// \todo Could use a frustum-frustum test for more accuracy
                if (frustum.IsInsideFast(BoundingBox(shadowFrustum)))
                    CollectShadowBatches(litGeometries, shadowQueue, shadowFrustum, true, true);
                break;

            case LIGHT_SPOT:
                // For spot light only need to check which lit geometries are shadow casters
                CollectShadowBatches(litGeometries, shadowQueue, shadowFrustum, true, false);
                break;
            }

            shadowQueue.Sort(instanceTransforms);

            // Mark shadow map for rendering only if it has a view with some batches
            if (shadowQueue.batches.Size())
            {
                shadowMaps[index].shadowViews.Push(view);
                shadowMaps[index].used = true;
                hasShadowBatches = true;
            }
        }

        if (!hasShadowBatches)
        {
            // Light did not have any shadow batches: convert to unshadowed and reuse the views
            light->SetShadowMap(nullptr);
            usedShadowViews = startIndex;
        }
    }

    {
        PROFILE(BuildLightPasses);

        for (auto it = lightLists.Begin(), end = lightLists.End(); it != end; ++it)
        {
            LightList& list = it->second;
            if (!list.useCount)
                continue;

            // Sort lights according to the light pointer to prevent camera angle from changing the light list order and
            // causing extra shader variations to be compiled
            Sort(list.lights.Begin(), list.lights.End());

            size_t lightsLeft = list.lights.Size();
            static Vector<bool> lightDone;
            static Vector<Light*> currentPass;
            lightDone.Resize(lightsLeft);
            for (size_t i = 0; i < lightsLeft; ++i)
                lightDone[i] = false;
            
            size_t index = 0;
            while (lightsLeft)
            {
                // Find lights to the current pass, while obeying rules for shadow coord allocations (shadowed directional & spot
                // lights can not share a pass)
                currentPass.Clear();
                size_t startIndex = index;
                size_t shadowCoordsLeft = MAX_LIGHTS_PER_PASS;
                while (lightsLeft && currentPass.Size() < MAX_LIGHTS_PER_PASS)
                {
                    if (!lightDone[index])
                    {
                        Light* light = list.lights[index];
                        size_t shadowCoords = light->NumShadowCoords();
                        if (shadowCoords <= shadowCoordsLeft)
                        {
                            lightDone[index] = true;
                            currentPass.Push(light);
                            shadowCoordsLeft -= shadowCoords;
                            --lightsLeft;
                        }
                    }

                    index = (index + 1) % list.lights.Size();
                    if (index == startIndex)
                        break;
                }

                unsigned long long passKey = 0;
                for (size_t i = 0; i < currentPass.Size(); ++i)
                    passKey += (unsigned long long)currentPass[i] << (i * 16);
                if (list.lightPasses.IsEmpty())
                    ++passKey; // First pass includes ambient light

                HashMap<unsigned long long, LightPass>::Iterator lpIt = lightPasses.Find(passKey);
                if (lpIt != lightPasses.End())
                    list.lightPasses.Push(&lpIt->second);
                else
                {
                    LightPass* newLightPass = &lightPasses[passKey];
                    newLightPass->vsBits = 0;
                    newLightPass->psBits = list.lightPasses.IsEmpty() ? LPS_AMBIENT : 0;
                    for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
                        newLightPass->shadowMaps[i] = nullptr;

                    size_t numShadowCoords = 0;
                    for (size_t i = 0; i < currentPass.Size(); ++i)
                    {
                        Light* light = currentPass[i];
                        newLightPass->psBits |= (light->GetLightType() + 1) << (i * 3 + 4);

                        float cutoff = cosf(light->Fov() * 0.5f * M_DEGTORAD);
                        newLightPass->lightPositions[i] = Vector4(light->WorldPosition(), 1.0f);
                        newLightPass->lightDirections[i] = Vector4(-light->WorldDirection(), 0.0f);
                        newLightPass->lightAttenuations[i] = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f /
                            (1.0f - cutoff), 0.0f);
                        newLightPass->lightColors[i] = light->GetColor();

                        if (light->ShadowMap())
                        {
                            // Enable shadowed shader variation, setup shadow parameters
                            newLightPass->psBits |= 4 << (i * 3 + 4);
                            newLightPass->shadowMaps[i] = light->ShadowMap();

                            const Vector<Matrix4>& shadowMatrices = light->ShadowMatrices();
                            for (size_t j = 0; j < shadowMatrices.Size() && numShadowCoords < MAX_LIGHTS_PER_PASS; ++j)
                                newLightPass->shadowMatrices[numShadowCoords++] = shadowMatrices[j];

                            newLightPass->shadowParameters[i] = light->ShadowParameters();

                            if (light->GetLightType() == LIGHT_DIRECTIONAL)
                            {
                                float fadeStart = light->ShadowFadeStart() * light->MaxShadowDistance() / camera->FarClip();
                                float fadeRange = light->MaxShadowDistance() / camera->FarClip() - fadeStart;
                                newLightPass->dirShadowSplits = light->ShadowSplits() / camera->FarClip();
                                newLightPass->dirShadowFade = Vector4(fadeStart / fadeRange, 1.0f / fadeRange, 0.0f, 0.0f);
                            }
                            else if (light->GetLightType() == LIGHT_POINT)
                                newLightPass->pointShadowParameters[i] = light->PointShadowParameters();
                        }

                        newLightPass->vsBits |= numShadowCoords << 2;
                        newLightPass->psBits |= numShadowCoords << 1;
                    }

                    list.lightPasses.Push(newLightPass);
                }
            }
        }
    }
}

void Renderer::CollectBatches(const Vector<PassDesc>& passes)
{
    PROFILE(CollectBatches);

    // Setup batch queues for each requested pass
    static Vector<BatchQueue*> currentQueues;
    currentQueues.Resize(passes.Size());
    for (size_t i = 0; i < passes.Size(); ++i)
    {
        const PassDesc& srcPass = passes[i];
        unsigned char baseIndex = Material::PassIndex(srcPass.name);
        BatchQueue* batchQueue = &batchQueues[baseIndex];
        currentQueues[i] = batchQueue;
        batchQueue->sort = srcPass.sort;
        batchQueue->lit = srcPass.lit;
        batchQueue->baseIndex = baseIndex;
        batchQueue->additiveIndex = srcPass.lit ? Material::PassIndex(srcPass.name + "add") : 0;
    }

    // Loop through geometry nodes
    for (auto gIt = geometries.Begin(), gEnd = geometries.End(); gIt != gEnd; ++gIt)
    {
        GeometryNode* node = *gIt;
        LightList* lightList = node->GetLightList();

        Batch newBatch;
        newBatch.type = node->GetGeometryType();
        newBatch.worldMatrix = &node->WorldTransform();

        // Loop through node's geometries
        for (auto bIt = node->Batches().Begin(), bEnd = node->Batches().End(); bIt != bEnd; ++bIt)
        {
            newBatch.geometry = bIt->geometry.Get();
            Material* material = bIt->material.Get();
            assert(material);

            // Loop through requested queues
            for (auto qIt = currentQueues.Begin(); qIt != currentQueues.End(); ++qIt)
            {
                BatchQueue& batchQueue = **qIt;
                newBatch.pass = material->GetPass(batchQueue.baseIndex);
                // Material may not have the requested pass at all, skip further processing as fast as possible in that case
                if (!newBatch.pass)
                    continue;

                newBatch.lights = batchQueue.lit ? lightList ? lightList->lightPasses[0] : &ambientLightPass : nullptr;
                if (batchQueue.sort < SORT_BACK_TO_FRONT)
                    newBatch.CalculateSortKey();
                else
                    newBatch.distance = node->Distance();

                batchQueue.batches.Push(newBatch);

                // Create additive light batches if necessary
                if (batchQueue.lit && lightList && lightList->lightPasses.Size() > 1)
                {
                    newBatch.pass = material->GetPass(batchQueue.additiveIndex);
                    if (!newBatch.pass)
                        continue;

                    for (size_t i = 1; i < lightList->lightPasses.Size(); ++i)
                    {
                        newBatch.lights = lightList->lightPasses[i];
                        if (batchQueue.sort != SORT_BACK_TO_FRONT)
                        {
                            newBatch.CalculateSortKey();
                            batchQueue.additiveBatches.Push(newBatch);
                        }
                        else
                        {
                            // In back-to-front mode base and additive batches must be mixed. Manipulate distance to make
                            // the additive batches render later
                            newBatch.distance = node->Distance() * 0.99999f;
                            batchQueue.batches.Push(newBatch);
                        }
                    }
                }
            }
        }
    }

    size_t oldSize = instanceTransforms.Size();

    for (auto qIt = currentQueues.Begin(); qIt != currentQueues.End(); ++qIt)
    {
        BatchQueue& batchQueue = **qIt;
        batchQueue.Sort(instanceTransforms);
    }

    // Check if more instances where added
    if (instanceTransforms.Size() != oldSize)
        instanceTransformsDirty = true;
}

void Renderer::CollectBatches(const PassDesc& pass)
{
    static Vector<PassDesc> passDescs(1);
    passDescs[0] = pass;
    CollectBatches(passDescs);
}

void Renderer::RenderShadowMaps()
{
    PROFILE(RenderShadowMaps);

    // Make sure the shadow maps are not bound on any unit
    graphics->ResetTextures();

    for (auto it = shadowMaps.Begin(); it != shadowMaps.End(); ++it)
    {
        if (!it->used)
            continue;

        graphics->SetRenderTarget(nullptr, it->texture);
        graphics->Clear(CLEAR_DEPTH, Color::BLACK, 1.0f);

        for (auto vIt = it->shadowViews.Begin(); vIt < it->shadowViews.End(); ++vIt)
        {
            ShadowView* view = *vIt;
            Light* light = view->light;
            graphics->SetViewport(view->viewport);
            RenderBatches(view->shadowQueue.batches, &view->shadowCamera, true, true, light->DepthBias(), light->SlopeScaledDepthBias());
        }
    }
}

void Renderer::RenderBatches(const Vector<PassDesc>& passes)
{
    PROFILE(RenderBatches);

    for (size_t i = 0; i < passes.Size(); ++i)
    {
        unsigned char passIndex = Material::PassIndex(passes[i].name);
        BatchQueue& batchQueue = batchQueues[passIndex];
        RenderBatches(batchQueue.batches, camera, i == 0);
        RenderBatches(batchQueue.additiveBatches, camera, false);
    }
}

void Renderer::RenderBatches(const String& pass)
{
    PROFILE(RenderBatches);

    unsigned char passIndex = Material::PassIndex(pass);
    BatchQueue& batchQueue = batchQueues[passIndex];
    RenderBatches(batchQueue.batches, camera);
    RenderBatches(batchQueue.additiveBatches, camera, false);
}

void Renderer::Initialize()
{
    graphics = Subsystem<Graphics>();
    assert(graphics && graphics->IsInitialized());

    Vector<Constant> constants;

    vsFrameConstantBuffer = new ConstantBuffer();
    constants.Push(Constant(ELEM_MATRIX3X4, "viewMatrix"));
    constants.Push(Constant(ELEM_MATRIX4, "projectionMatrix"));
    constants.Push(Constant(ELEM_MATRIX4, "viewProjMatrix"));
    constants.Push(Constant(ELEM_VECTOR4, "depthParameters"));
    vsFrameConstantBuffer->Define(USAGE_DEFAULT, constants);

    psFrameConstantBuffer = new ConstantBuffer();
    constants.Clear();
    constants.Push(Constant(ELEM_VECTOR4, "ambientColor"));
    psFrameConstantBuffer->Define(USAGE_DEFAULT, constants);

    vsObjectConstantBuffer = new ConstantBuffer();
    constants.Clear();
    constants.Push(Constant(ELEM_MATRIX3X4, "worldMatrix"));
    vsObjectConstantBuffer->Define(USAGE_DEFAULT, constants);

    vsLightConstantBuffer = new ConstantBuffer();
    constants.Clear();
    constants.Push(Constant(ELEM_MATRIX4, "shadowMatrices", MAX_LIGHTS_PER_PASS));
    vsLightConstantBuffer->Define(USAGE_DEFAULT, constants);

    psLightConstantBuffer = new ConstantBuffer();
    constants.Clear();
    constants.Push(Constant(ELEM_VECTOR4, "lightPositions", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "lightDirections", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "lightColors", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "lightAttenuations", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "shadowParameters", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "pointShadowParameters", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "dirShadowSplits"));
    constants.Push(Constant(ELEM_VECTOR4, "dirShadowFade"));
    psLightConstantBuffer->Define(USAGE_DEFAULT, constants);

    // Instance vertex buffer contains texcoords 4-6 which define the instances' world matrices
    instanceVertexBuffer = new VertexBuffer();
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 1, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 2, true));

    // Setup ambient light only -pass
    ambientLightPass.vsBits = 0;
    ambientLightPass.psBits = LPS_AMBIENT;

    // Setup point light face selection textures
    faceSelectionTexture1 = new Texture();
    faceSelectionTexture2 = new Texture();
    DefineFaceSelectionTextures();
}

void Renderer::DefineFaceSelectionTextures()
{
    PROFILE(DefineFaceSelectionTextures);

    const float faceSelectionData1[] = { 
        1.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    const float faceSelectionData2[] = {
        -0.5f, 0.5f, 0.5f, 1.5f,
        0.5f, 0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, 1.5f, 1.5f,
        -0.5f, -0.5f, 1.5f, 0.5f,
        0.5f, 0.5f, 2.5f, 1.5f,
        -0.5f, 0.5f, 2.5f, 0.5f
    };

    Vector<ImageLevel> faces1;
    Vector<ImageLevel> faces2;
    for (size_t i = 0; i < MAX_CUBE_FACES; ++i)
    {
        ImageLevel level;
        level.rowSize = 4 * sizeof(float);
        level.data = (unsigned char*)&faceSelectionData1[4 * i];
        faces1.Push(level);
        level.data = (unsigned char*)&faceSelectionData2[4 * i];
        faces2.Push(level);
    }

    faceSelectionTexture1->Define(TEX_CUBE, USAGE_DEFAULT, IntVector2(1, 1), FMT_RGBA32F, 1, &faces1[0]);
    faceSelectionTexture1->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
    faceSelectionTexture1->SetDataLost(false);

    faceSelectionTexture2->Define(TEX_CUBE, USAGE_DEFAULT, IntVector2(1, 1), FMT_RGBA32F, 1, &faces2[0]);
    faceSelectionTexture2->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
    faceSelectionTexture2->SetDataLost(false);
}

void Renderer::CollectGeometriesAndLights(Vector<OctreeNode*>::ConstIterator begin, Vector<OctreeNode*>::ConstIterator end,
    bool inside)
{
    if (inside)
    {
        for (auto it = begin; it != end; ++it)
        {
            OctreeNode* node = *it;
            unsigned short flags = node->Flags();
            if ((flags & NF_ENABLED) && (flags & (NF_GEOMETRY | NF_LIGHT)) && (node->LayerMask() & viewMask))
            {
                if (flags & NF_GEOMETRY)
                {
                    GeometryNode* geometry = static_cast<GeometryNode*>(node);
                    geometry->OnPrepareRender(frameNumber, camera);
                    geometries.Push(geometry);
                }
                else
                {
                    Light* light = static_cast<Light*>(node);
                    light->OnPrepareRender(frameNumber, camera);
                    lights.Push(light);
                }
            }
        }
    }
    else
    {
        for (auto it = begin; it != end; ++it)
        {
            OctreeNode* node = *it;
            unsigned short flags = node->Flags();
            if ((flags & NF_ENABLED) && (flags & (NF_GEOMETRY | NF_LIGHT)) && (node->LayerMask() & viewMask) &&
                frustum.IsInsideFast(node->WorldBoundingBox()))
            {
                if (flags & NF_GEOMETRY)
                {
                    GeometryNode* geometry = static_cast<GeometryNode*>(node);
                    geometry->OnPrepareRender(frameNumber, camera);
                    geometries.Push(geometry);
                }
                else
                {
                    Light* light = static_cast<Light*>(node);
                    light->OnPrepareRender(frameNumber, camera);
                    lights.Push(light);
                }
            }
        }
    }
}

void Renderer::AddLightToNode(GeometryNode* node, Light* light, LightList* lightList)
{
    LightList* oldList = node->GetLightList();

    if (!oldList)
    {
        // First light assigned on this frame
        node->SetLightList(lightList);
        ++lightList->useCount;
    }
    else
    {
        // Create new light list based on the node's existing one
        --oldList->useCount;
        unsigned long long newListKey = oldList->key;
        newListKey += (unsigned long long)light << ((oldList->lights.Size() & 3) * 16);
        HashMap<unsigned long long, LightList>::Iterator it = lightLists.Find(newListKey);
        if (it != lightLists.End())
        {
            LightList* newList = &it->second;
            node->SetLightList(newList);
            ++newList->useCount;
        }
        else
        {
            LightList* newList = &lightLists[newListKey];
            newList->key = newListKey;
            newList->lights = oldList->lights;
            newList->lights.Push(light);
            newList->useCount = 1;
            node->SetLightList(newList);
        }
    }
}

void Renderer::CollectShadowBatches(const Vector<GeometryNode*>& nodes, BatchQueue& batchQueue, const Frustum& frustum,
    bool checkShadowCaster, bool checkFrustum)
{
    Batch newBatch;
    newBatch.lights = nullptr;
    
    for (auto gIt = nodes.Begin(), gEnd = nodes.End(); gIt != gEnd; ++gIt)
    {
        GeometryNode* node = *gIt;
        if (checkShadowCaster && !(node->Flags() & NF_CASTSHADOWS))
            continue;
        if (checkFrustum && !frustum.IsInsideFast(node->WorldBoundingBox()))
            continue;

        // Node was possibly not in the main view. Update geometry first in that case
        if (node->LastFrameNumber() != frameNumber)
            node->OnPrepareRender(frameNumber, camera);

        newBatch.type = node->GetGeometryType();
        newBatch.worldMatrix = &node->WorldTransform();

        // Loop through node's geometries
        for (auto bIt = node->Batches().Begin(), bEnd = node->Batches().End(); bIt != bEnd; ++bIt)
        {
            newBatch.geometry = bIt->geometry.Get();
            Material* material = bIt->material.Get();
            assert(material);

            newBatch.pass = material->GetPass(batchQueue.baseIndex);
            // Material may not have the requested pass at all, skip further processing as fast as possible in that case
            if (!newBatch.pass)
                continue;

            newBatch.CalculateSortKey();
            batchQueue.batches.Push(newBatch);
        }
    }
}

void Renderer::RenderBatches(const Vector<Batch>& batches, Camera* camera_, bool setPerFrameConstants, bool overrideDepthBias,
    int depthBias, float slopeScaledDepthBias)
{
    if (faceSelectionTexture1->IsDataLost() || faceSelectionTexture2->IsDataLost())
        DefineFaceSelectionTextures();

    // Bind point light shadow face selection textures
    graphics->SetTexture(MAX_MATERIAL_TEXTURE_UNITS + MAX_LIGHTS_PER_PASS, faceSelectionTexture1);
    graphics->SetTexture(MAX_MATERIAL_TEXTURE_UNITS + MAX_LIGHTS_PER_PASS + 1, faceSelectionTexture2);

    // If rendering to a texture on OpenGL, flip the camera vertically to ensure similar texture coordinate addressing
    #ifdef TURSO3D_OPENGL
    bool flipVertical = camera_->FlipVertical();
    if (graphics->RenderTarget(0) || graphics->DepthStencil())
        camera_->SetFlipVertical(!flipVertical);
    #endif

    if (setPerFrameConstants)
    {
        // Set per-frame values to the frame constant buffers
        Matrix3x4 viewMatrix = camera_->ViewMatrix();
        Matrix4 projectionMatrix = camera_->ProjectionMatrix();
        Matrix4 viewProjMatrix = projectionMatrix * viewMatrix;
        Vector4 depthParameters(Vector4::ZERO);
        depthParameters.x = camera_->NearClip();
        depthParameters.y = camera_->FarClip();
        if (camera_->IsOrthographic())
        {
            #ifdef USE_OPENGL
            depthParameters.z = 0.5f;
            depthParameters.w = 0.5f;
            #else
            depthParameters.z = 1.0f;
            #endif
        }
        else
            depthParameters.w = 1.0f / camera->FarClip();

        vsFrameConstantBuffer->SetConstant(VS_FRAME_VIEW_MATRIX, viewMatrix);
        vsFrameConstantBuffer->SetConstant(VS_FRAME_PROJECTION_MATRIX, projectionMatrix);
        vsFrameConstantBuffer->SetConstant(VS_FRAME_VIEWPROJ_MATRIX, viewProjMatrix);
        vsFrameConstantBuffer->SetConstant(VS_FRAME_DEPTH_PARAMETERS, depthParameters);
        vsFrameConstantBuffer->Apply();

        /// \todo Add also fog settings
        psFrameConstantBuffer->SetConstant(PS_FRAME_AMBIENT_COLOR, camera_->AmbientColor());
        psFrameConstantBuffer->Apply();

        graphics->SetConstantBuffer(SHADER_VS, CB_FRAME, vsFrameConstantBuffer);
        graphics->SetConstantBuffer(SHADER_PS, CB_FRAME, psFrameConstantBuffer);
    }

    if (instanceTransformsDirty && instanceTransforms.Size())
    {
        if (instanceVertexBuffer->NumVertices() < instanceTransforms.Size())
            instanceVertexBuffer->Define(USAGE_DYNAMIC, instanceTransforms.Size(), instanceVertexElements, false, &instanceTransforms[0]);
        else
            instanceVertexBuffer->SetData(0, instanceTransforms.Size(), &instanceTransforms[0]);
        graphics->SetVertexBuffer(1, instanceVertexBuffer);
        instanceTransformsDirty = false;
    }
    
    {
        Pass* lastPass = nullptr;
        Material* lastMaterial = nullptr;
        LightPass* lastLights = nullptr;

        for (auto it = batches.Begin(); it != batches.End();)
        {
            const Batch& batch = *it;
            bool instanced = batch.type == GEOM_INSTANCED;

            Pass* pass = batch.pass;
            if (!pass->shadersLoaded)
                LoadPassShaders(pass);

            // Check that pass is legal
            if (pass->shaders[SHADER_VS].Get() && pass->shaders[SHADER_PS].Get())
            {
                // Get the shader variations
                LightPass* lights = batch.lights;
                ShaderVariation* vs = FindShaderVariation(SHADER_VS, pass, (unsigned short)batch.type | (lights ? lights->vsBits : 0));
                ShaderVariation* ps = FindShaderVariation(SHADER_PS, pass, lights ? lights->psBits : 0);
                graphics->SetShaders(vs, ps);

                Geometry* geometry = batch.geometry;
                assert(geometry);

                // Apply pass render state
                if (pass != lastPass)
                {
                    graphics->SetColorState(pass->blendMode, pass->alphaToCoverage, pass->colorWriteMask);
                    
                    if (!overrideDepthBias)
                        graphics->SetDepthState(pass->depthFunc, pass->depthWrite, pass->depthClip);
                    else
                        graphics->SetDepthState(pass->depthFunc, pass->depthWrite, pass->depthClip, depthBias, slopeScaledDepthBias);
                    
                    if (!camera_->UseReverseCulling())
                        graphics->SetRasterizerState(pass->cullMode, pass->fillMode);
                    else
                        graphics->SetRasterizerState(cullModeFlip[pass->cullMode], pass->fillMode);

                    lastPass = pass;
                }

                // Apply material render state
                Material* material = pass->Parent();
                if (material != lastMaterial)
                {
                    for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i)
                    {
                        if (material->textures[i])
                            graphics->SetTexture(i, material->textures[i]);
                    }
                    graphics->SetConstantBuffer(SHADER_VS, CB_MATERIAL, material->constantBuffers[SHADER_VS].Get());
                    graphics->SetConstantBuffer(SHADER_PS, CB_MATERIAL, material->constantBuffers[SHADER_PS].Get());

                    lastMaterial = material;
                }

                // Apply object render state
                if (geometry->constantBuffers[SHADER_VS])
                    graphics->SetConstantBuffer(SHADER_VS, CB_OBJECT, geometry->constantBuffers[SHADER_VS].Get());
                else if (!instanced)
                {
                    vsObjectConstantBuffer->SetConstant(VS_OBJECT_WORLD_MATRIX, *batch.worldMatrix);
                    vsObjectConstantBuffer->Apply();
                    graphics->SetConstantBuffer(SHADER_VS, CB_OBJECT, vsObjectConstantBuffer.Get());
                }
                graphics->SetConstantBuffer(SHADER_PS, CB_OBJECT, geometry->constantBuffers[SHADER_PS].Get());

                // Apply light constant buffers and shadow maps
                if (lights && lights != lastLights)
                {
                    // If light queue is ambient only, no need to update the constants
                    if (lights->psBits > LPS_AMBIENT)
                    {
                        if (lights->vsBits & LVS_NUMSHADOWCOORDS)
                        {
                            vsLightConstantBuffer->SetData(lights->shadowMatrices);
                            graphics->SetConstantBuffer(SHADER_VS, CB_LIGHTS, vsLightConstantBuffer.Get());
                        }
                        
                        psLightConstantBuffer->SetData(lights->lightPositions);
                        graphics->SetConstantBuffer(SHADER_PS, CB_LIGHTS, psLightConstantBuffer.Get());

                        for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
                            graphics->SetTexture(MAX_MATERIAL_TEXTURE_UNITS + i, lights->shadowMaps[i]);
                    }

                    lastLights = lights;
                }
                
                // Set vertex / index buffers and draw
                if (instanced)
                    geometry->DrawInstanced(graphics, batch.instanceStart, batch.instanceCount);
                else
                    geometry->Draw(graphics);
            }

            // Advance. If instanced, skip over the batches that were converted
            it += instanced ? batch.instanceCount : 1;
        }
    }

    // Restore original camera vertical flipping state now
    #ifdef TURSO3D_OPENGL
    camera_->SetFlipVertical(flipVertical);
    #endif
}

void Renderer::LoadPassShaders(Pass* pass)
{
    PROFILE(LoadPassShaders);

    ResourceCache* cache = Subsystem<ResourceCache>();
    // Use different extensions for GLSL & HLSL shaders
    #ifdef TURSO3D_OPENGL
    pass->shaders[SHADER_VS] = cache->LoadResource<Shader>(pass->ShaderName(SHADER_VS) + ".vert");
    pass->shaders[SHADER_PS] = cache->LoadResource<Shader>(pass->ShaderName(SHADER_PS) + ".frag");
    #else
    pass->shaders[SHADER_VS] = cache->LoadResource<Shader>(pass->ShaderName(SHADER_VS) + ".vs");
    pass->shaders[SHADER_PS] = cache->LoadResource<Shader>(pass->ShaderName(SHADER_PS) + ".ps");
    #endif

    pass->shadersLoaded = true;
}

ShaderVariation* Renderer::FindShaderVariation(ShaderStage stage, Pass* pass, unsigned short bits)
{
    /// \todo Evaluate whether the hash lookup is worth the memory save vs using just straightforward vectors
    HashMap<unsigned short, WeakPtr<ShaderVariation> >& variations = pass->shaderVariations[stage];
    HashMap<unsigned short, WeakPtr<ShaderVariation> >::Iterator it = variations.Find(bits);

    if (it != variations.End())
        return it->second.Get();
    else
    {
        if (stage == SHADER_VS)
        {
            String vsString = pass->CombinedShaderDefines(stage) + " " + geometryDefines[bits & LVS_GEOMETRY];
            if (bits & LVS_NUMSHADOWCOORDS)
                vsString += " " + lightDefines[1] + "=" + String((bits & LVS_NUMSHADOWCOORDS) >> 2);

            it = variations.Insert(MakePair(bits, WeakPtr<ShaderVariation>(pass->shaders[stage]->CreateVariation(vsString.Trimmed()))));
            return it->second.Get();
        }
        else
        {
            String psString = pass->CombinedShaderDefines(stage);
            if (bits & LPS_AMBIENT)
                psString += " " + lightDefines[0];
            if (bits & LPS_NUMSHADOWCOORDS)
                psString += " " + lightDefines[1] + "=" + String((bits & LPS_NUMSHADOWCOORDS) >> 1);
            for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
            {
                unsigned short lightBits = (bits >> (i * 3 + 4)) & 7;
                if (lightBits)
                    psString += " " + lightDefines[(lightBits & 3) + 1] + String((int)i);
                if (lightBits & 4)
                    psString += " " + lightDefines[5] + String((int)i);
            }

            it = variations.Insert(MakePair(bits, WeakPtr<ShaderVariation>(pass->shaders[stage]->CreateVariation(psString.Trimmed()))));
            return it->second.Get();
        }
    }
}

void RegisterRendererLibrary()
{
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    // Scene node base attributes are needed
    RegisterSceneLibrary();
    Octree::RegisterObject();
    Camera::RegisterObject();
    OctreeNode::RegisterObject();
    GeometryNode::RegisterObject();
    StaticModel::RegisterObject();
    Light::RegisterObject();
    Material::RegisterObject();
    Model::RegisterObject();
}

}
