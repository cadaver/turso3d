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

inline bool CompareBatchState(Batch& lhs, Batch& rhs)
{
    return lhs.sortKey < rhs.sortKey;
}

inline bool CompareBatchDistance(Batch& lhs, Batch& rhs)
{
    return lhs.distance > rhs.distance;
}

inline bool CompareLights(Light* lhs, Light* rhs)
{
    return lhs->Distance() < rhs->Distance();
}

void BatchQueue::Clear()
{
    batches.Clear();
    instanceDatas.Clear();
    instanceLookup.Clear();
}

void BatchQueue::AddBatch(Batch& batch, GeometryNode* node, bool isAdditive)
{
    if (sort == SORT_STATE)
    {
        if (batch.type == GEOM_STATIC)
        {
            batch.type = GEOM_INSTANCED;
            batch.CalculateSortKey(isAdditive);

            // Check if instance batch already exists
            auto iIt = instanceLookup.Find(batch.sortKey);
            if (iIt != instanceLookup.End())
                instanceDatas[iIt->second].worldMatrices.Push(&node->WorldTransform());
            else
            {
                // Begin new instanced batch
                size_t newInstanceDataIndex = instanceDatas.Size();
                instanceLookup[batch.sortKey] = newInstanceDataIndex;
                batch.instanceDataIndex = newInstanceDataIndex;
                batches.Push(batch);
                instanceDatas.Resize(newInstanceDataIndex + 1);
                InstanceData& newInstanceData = instanceDatas.Back();
                newInstanceData.skipBatches = false;
                newInstanceData.worldMatrices.Push(&node->WorldTransform());
            }
        }
        else
        {
            batch.worldMatrix = &node->WorldTransform();
            batch.CalculateSortKey(isAdditive);
            batches.Push(batch);
        }
    }
    else
    {
        batch.worldMatrix = &node->WorldTransform();
        batch.distance = node->Distance();
        // Push additive passes slightly to front to make them render after base passes
        if (isAdditive)
            batch.distance *= 0.999999f;
        batches.Push(batch);
    }
}

void BatchQueue::Sort()
{
    PROFILE(SortBatches);

    if (sort == SORT_STATE)
        Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchState);
    else
    {
        Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchDistance);

        // After sorting batches by distance, we need a separate step to build instances if adjacent batches have the same state
        Batch* start = nullptr;
        for (auto it = batches.Begin(), end = batches.End(); it != end; ++it)
        {
            Batch* current = &*it;
            if (start && current->type == GEOM_STATIC && current->pass == start->pass && current->geometry == start->geometry &&
                current->lights == start->lights)
            {
                if (start->type == GEOM_INSTANCED)
                    instanceDatas[start->instanceDataIndex].worldMatrices.Push(current->worldMatrix);
                else
                {
                    // Begin new instanced batch
                    start->type = GEOM_INSTANCED;
                    size_t newInstanceDataIndex = instanceDatas.Size();
                    instanceDatas.Resize(newInstanceDataIndex + 1);
                    InstanceData& newInstanceData = instanceDatas.Back();
                    newInstanceData.skipBatches = true;
                    newInstanceData.worldMatrices.Push(start->worldMatrix);
                    newInstanceData.worldMatrices.Push(current->worldMatrix);
                    start->instanceDataIndex = newInstanceDataIndex; // Overwrites the non-instanced world matrix
                }
            }
            else
                start = (current->type == GEOM_STATIC) ? current : nullptr;
        }
    }
}

ShadowMap::ShadowMap()
{
    // Construct texture but do not define its size yet
    texture = new Texture();
}

ShadowMap::~ShadowMap()
{
}

void ShadowMap::Clear()
{
    allocator.Reset(texture->Width(), texture->Height(), 0, 0, false);
    shadowViews.Clear();
    used = false;
}

void ShadowView::Clear()
{
    shadowCasters.Clear();
    shadowQueue.Clear();
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
        it->texture->Define(TEX_2D, USAGE_RENDERTARGET, IntVector2(size, size), format, 1);
        // Setup shadow map sampling with hardware depth compare
        it->texture->DefineSampler(COMPARE_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP, 1);
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

    {
        PROFILE(CollectLitObjects);

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

            // Remove lights that do not have receivers from further processing
            if (!hasReceivers)
                *it = nullptr;
        }
    }

    {
        /// \todo Thread this
        PROFILE(ProcessShadowLights);

        for (auto it = lights.Begin(), end = lights.End(); it != end; ++it)
        {
            Light* light = *it;
            // Previous loop may have left null pointer holes
            if (!light)
                continue;

            // If light is not shadowed, release shadow view structures
            if (!light->CastShadows())
            {
                light->ResetShadowViews();
                continue;
            }

            // Try to allocate shadow map rectangle
            /// \todo Fall back to smaller size on failure
            IntVector2 shadowSize = light->ShadowRectSize();
            IntRect shadowRect;
            size_t index;
            for (index = 0; index < shadowMaps.Size(); ++index)
            {
                ShadowMap& shadowMap = shadowMaps[index];
                int x, y;
                if (shadowMap.allocator.Allocate(shadowSize.x, shadowSize.y, x, y))
                {
                    light->SetShadowMap(shadowMaps[index].texture, IntRect(x, y, x + shadowSize.x, y + shadowSize.y));
                    break;
                }
            }

            // If no room in any shadow map, render unshadowed
            if (index >= shadowMaps.Size())
            {
                light->SetShadowMap(nullptr, IntRect::ZERO);
                continue;
            }

            light->SetupShadowViews(camera);
            const Vector<AutoPtr<ShadowView> >& shadowViews = light->ShadowViews();
            bool hasShadowBatches = false;

            for (size_t i = 0; i < shadowViews.Size(); ++i)
            {
                ShadowView* view = shadowViews[i].Get();
                /// \todo Spotlights should reuse the lit geometries frustum query
                octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(view->shadowCasters),
                    view->shadowCamera.WorldFrustum(), NF_ENABLED | NF_GEOMETRY | NF_CASTSHADOWS, light->LightMask());

                CollectShadowBatches(view);

                // Mark shadow map for rendering only if it has a view with some batches
                if (view->shadowQueue.batches.Size())
                {
                    hasShadowBatches = true;
                    shadowMaps[index].shadowViews.Push(view);
                    shadowMaps[index].used = true;
                }
            }

            if (!hasShadowBatches)
            {
                // Light did not have any shadow batches: convert to unshadowed. At this point we have allocated the shadow map
                // unnecessarily, but not sampling it will still be faster
                light->SetShadowMap(nullptr, IntRect::ZERO);
            }
        }
    }

    {
        PROFILE(BuildLightPasses);

        for (auto it = lightLists.Begin(), end = lightLists.End(); it != end; ++it)
        {
            LightList& list = it->second;
            if (!list.useCount)
                continue;

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
                            Texture* shadowMap = light->ShadowMap();

                            // Enable shadowed shader variation, setup shadow matrices if needed
                            newLightPass->shadowMaps[i] = shadowMap;
                            newLightPass->psBits |= 4 << (i * 3 + 4);
                            light->SetupShadowMatrices(newLightPass->shadowMatrices, numShadowCoords);

                            // Depth reconstruction parameters
                            Camera* shadowCamera = light->ShadowCamera(0);
                            float nearClip = shadowCamera->NearClip();
                            float farClip = shadowCamera->FarClip();
                            float q = farClip / (farClip - nearClip);
                            float r = -q * nearClip;

                            // Shadow map offsets
                            newLightPass->shadowParameters[i] = Vector4(0.5f / (float)shadowMap->Width(), 0.5f /
                                (float)shadowMap->Height(), q, r);

                            // Additional parameters for directional and point lights
                            if (light->GetLightType() == LIGHT_DIRECTIONAL)
                            {
                                float fadeStart = light->ShadowFadeStart() * light->MaxShadowDistance() / camera->FarClip();
                                float fadeRange = light->MaxShadowDistance() / camera->FarClip() - fadeStart;
                                newLightPass->dirShadowSplits = light->ShadowSplits() / camera->FarClip();
                                newLightPass->dirShadowFade = Vector4(fadeStart / fadeRange, 1.0f / fadeRange, 0.0f, 0.0f);
                            }
                            else if (light->GetLightType() == LIGHT_POINT)
                            {
                                float shadowMapSize = (float)light->ShadowMapSize();
                                const IntRect& shadowRect = light->ShadowRect();
                                newLightPass->pointShadowParameters[i] = Vector4(
                                    shadowMapSize / (float)shadowMap->Width(),
                                    shadowMapSize / (float)shadowMap->Height(),
                                    (float)shadowRect.left / (float)shadowMap->Width(),
                                    (float)shadowRect.top / (float)shadowMap->Height());
                            }
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
    if (passes.Size())
        CollectBatches(passes.Size(), &passes[0]);
}

void Renderer::CollectBatches(const PassDesc& pass)
{
    CollectBatches(1, &pass);
}

void Renderer::CollectBatches(size_t numPasses, const PassDesc* passes_)
{
    PROFILE(CollectBatches);

    // Setup batch queues for each requested pass
    static Vector<BatchQueue*> currentQueues;
    currentQueues.Resize(numPasses);
    for (size_t i = 0; i < numPasses; ++i)
    {
        const PassDesc& srcPass = passes_[i];
        unsigned char baseIndex = Material::PassIndex(srcPass.name);
        BatchQueue* batchQueue = &batchQueues[baseIndex];
        currentQueues[i] = batchQueue;
        batchQueue->sort = srcPass.sort;
        batchQueue->lit = srcPass.lit;
        batchQueue->baseIndex = baseIndex;
        batchQueue->additiveIndex = srcPass.lit ? Material::PassIndex(srcPass.name + "add") : 0;
    }

    {
        PROFILE(BuildBatches);

        Vector<BatchQueue*>::Iterator pBegin = currentQueues.Begin();
        Vector<BatchQueue*>::Iterator pEnd = currentQueues.End();

        // Loop through geometry nodes
        for (auto gIt = geometries.Begin(), gEnd = geometries.End(); gIt != gEnd; ++gIt)
        {
            GeometryNode* node = *gIt;
            LightList* lightList = node->GetLightList();

            // Loop through node's geometries
            for (auto bIt = node->Batches().Begin(), bEnd = node->Batches().End(); bIt != bEnd; ++bIt)
            {
                Batch newBatch;
                newBatch.geometry = bIt->geometry.Get();
                Material* material = bIt->material.Get();
                assert(material);

                // Loop through requested passes
                for (auto pIt = pBegin; pIt != pEnd; ++pIt)
                {
                    BatchQueue& batchQueue = **pIt;
                    newBatch.pass = material->GetPass(batchQueue.baseIndex);
                    // Material may not have the requested pass at all, skip further processing as fast as possible in that case
                    if (!newBatch.pass)
                        continue;
                    
                    newBatch.lights = lightList ? lightList->lightPasses[0] : batchQueue.lit ? &ambientLightPass : nullptr;
                    newBatch.type = node->GetGeometryType();
                    batchQueue.AddBatch(newBatch, node, false);

                    // Create additive light batches if necessary
                    if (lightList && lightList->lightPasses.Size() > 1)
                    {
                        newBatch.pass = material->GetPass(batchQueue.additiveIndex);
                        if (!newBatch.pass)
                            continue;

                        for (size_t i = 1; i < lightList->lightPasses.Size(); ++i)
                        {
                            newBatch.lights = lightList->lightPasses[i];
                            newBatch.type = node->GetGeometryType();
                            batchQueue.AddBatch(newBatch, node, true);
                        }
                    }
                }
            }
        }
    }

    for (auto pIt = currentQueues.Begin(); pIt != currentQueues.End(); ++pIt)
    {
        BatchQueue& batchQueue = **pIt;
        batchQueue.Sort();
        CopyInstanceTransforms(batchQueue);
    }
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
            RenderBatches(view->shadowQueue, &view->shadowCamera, true, true, light->DepthBias(), light->SlopeScaledDepthBias());
        }
    }
}

void Renderer::RenderBatches(const Vector<PassDesc>& passes)
{
    if (passes.Size())
        RenderBatches(passes.Size(), &passes.Front());
}

void Renderer::RenderBatches(const String& pass)
{
    unsigned char passIndex = Material::PassIndex(pass);
    BatchQueue& batchQueue = batchQueues[passIndex];
    RenderBatches(batchQueue, camera);
}

void Renderer::RenderBatches(size_t numPasses, const PassDesc* passes)
{
    bool first = true;

    while (numPasses--)
    {
        unsigned char passIndex = Material::PassIndex(passes->name);
        BatchQueue& batchQueue = batchQueues[passIndex];
        RenderBatches(batchQueue, camera, first);
        first = false;
        ++passes;
    }
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

void Renderer::CollectShadowBatches(ShadowView* view)
{
    BatchQueue& batchQueue = view->shadowQueue;

    {
        PROFILE(CollectShadowBatches);

        unsigned char passIndex = Material::PassIndex("shadow");
        batchQueue.sort = SORT_STATE;
        batchQueue.lit = false;

        // Loop through shadow caster nodes
        for (auto gIt = view->shadowCasters.Begin(), gEnd = view->shadowCasters.End(); gIt != gEnd; ++gIt)
        {
            GeometryNode* node = *gIt;
            // Node may not be in main view. In that case update it now
            if (node->LastFrameNumber() != frameNumber)
                node->OnPrepareRender(frameNumber, camera);
            
            // Loop through node's geometries
            for (auto bIt = node->Batches().Begin(), bEnd = node->Batches().End(); bIt != bEnd; ++bIt)
            {
                Batch newBatch;
                newBatch.geometry = bIt->geometry.Get();
                Material* material = bIt->material.Get();
                assert(material);

                newBatch.pass = material->GetPass(passIndex);
                // Material may not have the requested pass at all, skip further processing as fast as possible in that case
                if (!newBatch.pass)
                    continue;

                newBatch.lights = nullptr;
                newBatch.type = node->GetGeometryType();
                batchQueue.AddBatch(newBatch, node, false);
            }
        }
    }

    batchQueue.Sort();
    CopyInstanceTransforms(batchQueue);
}

void Renderer::CopyInstanceTransforms(BatchQueue& batchQueue)
{
    PROFILE(CopyInstanceTransforms);

    // Now go through all instance batches and copy to the global buffer
    size_t oldSize = instanceTransforms.Size();
    for (auto it = batchQueue.instanceDatas.Begin(), end = batchQueue.instanceDatas.End(); it != end; ++it)
    {
        size_t index = instanceTransforms.Size();
        InstanceData& instance = *it;
        instance.startIndex = index;
        instanceTransforms.Resize(index + instance.worldMatrices.Size());
        for (auto mIt = instance.worldMatrices.Begin(), mEnd = instance.worldMatrices.End(); mIt != mEnd; ++mIt)
            instanceTransforms[index++] = **mIt;
    }

    if (instanceTransforms.Size() != oldSize)
        instanceTransformsDirty = true;
}

void Renderer::RenderBatches(BatchQueue& batchQueue, Camera* camera_, bool setPerFrameConstants, bool overrideDepthBias, int depthBias, float slopeScaledDepthBias)
{
    PROFILE(RenderBatches);

    if (faceSelectionTexture1->IsDataLost() || faceSelectionTexture2->IsDataLost())
        DefineFaceSelectionTextures();

    // Bind point light shadow face selection textures
    graphics->SetTexture(MAX_MATERIAL_TEXTURE_UNITS + MAX_LIGHTS_PER_PASS, faceSelectionTexture1);
    graphics->SetTexture(MAX_MATERIAL_TEXTURE_UNITS + MAX_LIGHTS_PER_PASS + 1, faceSelectionTexture2);

    // If rendering to a texture on OpenGL, flip the camera vertically to ensure similar texture coordinate addressing
    #ifdef TURSO3D_OPENGL
    camera_->SetFlipVertical(graphics->RenderTarget(0) || graphics->DepthStencil());
    #endif

    if (setPerFrameConstants)
    {
        PROFILE(SetPerFrameConstants);

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

    if (batchQueue.batches.IsEmpty())
        return;

    if (instanceTransformsDirty && instanceTransforms.Size())
    {
        PROFILE(SetInstanceTransforms);

        if (instanceVertexBuffer->NumVertices() < instanceTransforms.Size())
            instanceVertexBuffer->Define(USAGE_DYNAMIC, instanceTransforms.Size(), instanceVertexElements, false, &instanceTransforms[0]);
        else
            instanceVertexBuffer->SetData(0, instanceTransforms.Size(), &instanceTransforms[0]);
        graphics->SetVertexBuffer(1, instanceVertexBuffer);
        instanceTransformsDirty = false;
    }

    {
        PROFILE(SubmitDrawCalls);

        Vector<Batch>& batches = batchQueue.batches;
        Pass* lastPass = nullptr;
        Material* lastMaterial = nullptr;
        LightPass* lastLights = nullptr;

        for (auto it = batches.Begin(); it != batches.End();)
        {
            const Batch& batch = *it;
            InstanceData* instance = batch.type == GEOM_INSTANCED ? &batchQueue.instanceDatas[batch.instanceDataIndex] : nullptr;

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

                // Set batch geometry
                Geometry* geometry = batch.geometry;
                assert(geometry);
                VertexBuffer* vb = geometry->vertexBuffer.Get();
                IndexBuffer* ib = geometry->indexBuffer.Get();
                graphics->SetVertexBuffer(0, vb);
                if (ib)
                    graphics->SetIndexBuffer(ib);

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
                else if (!instance)
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
                            vsLightConstantBuffer->Apply();
                            graphics->SetConstantBuffer(SHADER_VS, CB_LIGHTS, vsLightConstantBuffer.Get());
                        }
                        
                        psLightConstantBuffer->SetData(lights->lightPositions);
                        psLightConstantBuffer->Apply();
                        graphics->SetConstantBuffer(SHADER_PS, CB_LIGHTS, psLightConstantBuffer.Get());

                        for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
                            graphics->SetTexture(MAX_MATERIAL_TEXTURE_UNITS + i, lights->shadowMaps[i]);
                    }

                    lastLights = lights;
                }

                if (instance)
                {
                    if (ib)
                    {
                        graphics->DrawIndexedInstanced(geometry->primitiveType, geometry->drawStart, geometry->drawCount, 0,
                            instance->startIndex, instance->worldMatrices.Size());
                    }
                    else
                    {
                        graphics->DrawInstanced(geometry->primitiveType, geometry->drawStart, geometry->drawCount,
                            instance->startIndex, instance->worldMatrices.Size());
                    }
                }
                else
                {
                    if (ib)
                        graphics->DrawIndexed(geometry->primitiveType, geometry->drawStart, geometry->drawCount, 0);
                    else
                        graphics->Draw(geometry->primitiveType, geometry->drawStart, geometry->drawCount);
                }
            }

            // Advance. If necessary, skip over the batches that were converted
            it += instance ? instance->skipBatches ? instance->worldMatrices.Size() : 1 : 1;
        }
    }
}

void Renderer::LoadPassShaders(Pass* pass)
{
    PROFILE(LoadPassShaders);

    pass->shadersLoaded = true;

    Material* material = pass->Parent();
    if (!material)
        return;

    // Combine and trim the shader defines from material & pass
    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        const String& materialDefines = material->shaderDefines[i];
        if (materialDefines.Length())
            pass->combinedShaderDefines[i] = (materialDefines.Trimmed() + " " + pass->shaderDefines[i]).Trimmed();
        else
            pass->combinedShaderDefines[i] = pass->shaderDefines[i].Trimmed();
    }

    ResourceCache* cache = Subsystem<ResourceCache>();
    // Use different extensions for GLSL & HLSL shaders
    #ifdef TURSO3D_OPENGL
    pass->shaders[SHADER_VS] = cache->LoadResource<Shader>(pass->shaderNames[SHADER_VS] + ".vert");
    pass->shaders[SHADER_PS] = cache->LoadResource<Shader>(pass->shaderNames[SHADER_PS] + ".frag");
    #else
    pass->shaders[SHADER_VS] = cache->LoadResource<Shader>(pass->shaderNames[SHADER_VS] + ".vs");
    pass->shaders[SHADER_PS] = cache->LoadResource<Shader>(pass->shaderNames[SHADER_PS] + ".ps");
    #endif
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
            String vsString = pass->combinedShaderDefines[stage] + " " + geometryDefines[bits & LVS_GEOMETRY];
            if (bits & LVS_NUMSHADOWCOORDS)
                vsString += " " + lightDefines[1] + "=" + String((bits & LVS_NUMSHADOWCOORDS) >> 2);

            it = variations.Insert(MakePair(bits, WeakPtr<ShaderVariation>(pass->shaders[stage]->CreateVariation(vsString.Trimmed()))));
            return it->second.Get();
        }
        else
        {
            String psString = pass->combinedShaderDefines[SHADER_PS];
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
