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
#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "Model.h"
#include "Octree.h"
#include "Renderer.h"
#include "StaticModel.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static const unsigned LVS_GEOMETRY = 0x3;
static const unsigned LVS_SHADOW = 0x4;

static const unsigned LPS_AMBIENT = 0x1;
static const unsigned LPS_LIGHT0 = 0xe;
static const unsigned LPS_LIGHT1 = 0x70;
static const unsigned LPS_LIGHT2 = 0x380;
static const unsigned LPS_LIGHT3 = 0x1c00;

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
        batch.distance = node->SquaredDistance();
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

ShadowView::ShadowView() :
    shadowCamera(new Camera())
{
}

ShadowView::~ShadowView()
{
}

void ShadowView::Clear()
{
    shadowCasters.Clear();
    shadowQueue.Clear();
}

Renderer::Renderer() :
    frameNumber(0),
    usedShadowViews(0),
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

void Renderer::CollectObjects(Scene* scene_, Camera* camera_)
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
    for (auto it = shadowViews.Begin(); it != shadowViews.End(); ++it)
        (*it)->Clear();
    for (auto it = shadowMaps.Begin(); it != shadowMaps.End(); ++it)
        it->Clear();
    lastCamera = nullptr;
    usedShadowViews = 0;

    scene = scene_;
    camera = camera_;
    octree = scene ? scene->FindChild<Octree>() : nullptr;
    if (!scene || !camera || !octree)
        return;

    // Increment frame number. Never use 0, as that is the default for objects that have never been rendered
    ++frameNumber;
    if (!frameNumber)
        ++frameNumber;

    if (camera->AutoAspectRatio())
    {
        const IntRect& viewport = graphics->Viewport();
        camera->SetAspectRatioInternal((float)viewport.Width() / (float)viewport.Height());
    }

    // Reinsert moved objects to the octree
    octree->Update();

    frustum = camera->WorldFrustum();
    viewMask = camera->ViewMask();

    octree->FindNodes(frustum, this, &Renderer::CollectGeometriesAndLights);
}

void Renderer::CollectLightInteractions()
{
    PROFILE(CollectLightInteractions);

    {
        PROFILE(CollectLitObjects);

        for (auto it = lights.Begin(), end = lights.End(); it != end; ++it)
        {
            Light* light = *it;
            unsigned lightMask = light->LightMask();

            litGeometries.Clear();

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
                        AddLightToNode(node, light, lightList);
                }
                break;

            case LIGHT_POINT:
                octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(litGeometries), light->WorldSphere(), NF_ENABLED |
                    NF_GEOMETRY, lightMask);
                for (auto gIt = litGeometries.Begin(), gEnd = litGeometries.End(); gIt != gEnd; ++gIt)
                {
                    GeometryNode* node = *gIt;
                    // Add light only to nodes which are actually inside the frustum this frame
                    if (node->lastFrameNumber == frameNumber)
                        AddLightToNode(node, light, lightList);
                }
                break;

            case LIGHT_SPOT:
                octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(litGeometries), light->WorldFrustum(), NF_ENABLED |
                    NF_GEOMETRY, lightMask);
                for (auto gIt = litGeometries.Begin(), gEnd = litGeometries.End(); gIt != gEnd; ++gIt)
                {
                    GeometryNode* node = *gIt;
                    if (node->lastFrameNumber == frameNumber)
                        AddLightToNode(node, light, lightList);
                }
                break;
            }
        }
    }

    {
        /// \todo Thread this
        PROFILE(ProcessShadowLights);

        for (auto it = lights.Begin(), end = lights.End(); it != end; ++it)
        {
            Light* light = *it;
            light->shadowMap = nullptr;
            light->shadowViews.Clear();
            if (!light->CastShadows() || !light->hasReceivers)
                continue;

            // Try to allocate shadow map rect
            /// \todo Sort lights in importance order and/or fall back to smaller size on failure
            IntVector2 shadowSize = light->TotalShadowMapSize();
            IntRect shadowRect;
            size_t idx;
            for (idx = 0; idx < shadowMaps.Size(); ++idx)
            {
                ShadowMap& shadowMap = shadowMaps[idx];
                int x, y;
                if (shadowMap.allocator.Allocate(shadowSize.x, shadowSize.y, x, y))
                {
                    light->shadowMap = shadowMaps[idx].texture;
                    light->shadowRect = IntRect(x, y, x + shadowSize.x, y + shadowSize.y);
                    shadowMap.used = true;
                    break;
                }
            }

            // No room in any shadow map
            if (idx >= shadowMaps.Size())
                continue;
            
            // Create new shadow views as necessary. These are persistent to avoid constant allocation
            size_t numShadowViews = light->NumShadowViews();
            if (shadowViews.Size() < usedShadowViews + numShadowViews)
                shadowViews.Resize(usedShadowViews + numShadowViews);
            for (size_t i = usedShadowViews; i < usedShadowViews + numShadowViews; ++i)
            {
                if (!shadowViews[i])
                    shadowViews[i] = new ShadowView();
                light->shadowViews.Push(shadowViews[i]);
            }
            
            light->SetupShadowViews();

            for (size_t i = 0; i < numShadowViews; ++i)
            {
                ShadowView* view = light->shadowViews[i];
                /// \todo Spotlights should reuse the lit geometries frustum query
                octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(view->shadowCasters),
                    view->shadowCamera->WorldFrustum(), NF_ENABLED | NF_GEOMETRY | NF_CASTSHADOWS, light->LightMask());
                // Record shadow view also to the shadow map so that all shadows can be rendered with minimal rendertarget switches
                shadowMaps[idx].shadowViews.Push(view);
            }

            usedShadowViews += numShadowViews;
        }

        // Finally collect batches from all shadow cameras
        for (size_t i = 0; i < usedShadowViews; ++i)
            CollectShadowBatches(shadowViews[i]);
    }

    {
        PROFILE(BuildLightPasses);

        /// \todo Shadowed dir light uses up all light matrices, so it can not be combined with other shadowed dir or spot lights
        for (auto it = lightLists.Begin(), end = lightLists.End(); it != end; ++it)
        {
            LightList& list = it->second;
            if (!list.useCount)
                continue;

            for (size_t i = 0; i < list.lights.Size(); i += MAX_LIGHTS_PER_PASS)
            {
                unsigned long long passKey = 0;
                for (size_t j = i; j < list.lights.Size() && j < i + MAX_LIGHTS_PER_PASS; ++j)
                    passKey += (unsigned long long)list.lights[j] << ((j & 3) * 16);
                if (i == 0)
                    ++passKey; // First pass includes ambient light

                HashMap<unsigned long long, LightPass>::Iterator lpIt = lightPasses.Find(passKey);
                if (lpIt != lightPasses.End())
                    list.lightPasses.Push(&lpIt->second);
                else
                {
                    LightPass* newLightPass = &lightPasses[passKey];
                    newLightPass->vsBits = 0;
                    newLightPass->psBits = (i == 0) ? LPS_AMBIENT : 0;
                    for (size_t j = 0; j < MAX_LIGHTS_PER_PASS; ++j)
                        newLightPass->shadowMaps[j] = nullptr;

                    size_t numShadowMatrices = 0;
                    size_t k = 0;

                    for (size_t j = i; j < list.lights.Size() && j < i + MAX_LIGHTS_PER_PASS; ++j, ++k)
                    {
                        Light* light = list.lights[j];

                        newLightPass->psBits |= (light->GetLightType() + 1) << (k * 3 + 1);
                        float cutoff = cosf(light->Fov() * 0.5f * M_DEGTORAD);
                        newLightPass->lightPositions[k] = Vector4(light->WorldPosition(), 1.0f);
                        newLightPass->lightDirections[k] = Vector4(-light->WorldDirection(), 0.0f);
                        newLightPass->lightAttenuations[k] = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f /
                            (1.0f - cutoff), 0.0f);
                        newLightPass->lightColors[k] = light->GetColor();

                        if (light->shadowMap)
                        {
                            newLightPass->shadowMaps[k] = light->shadowMap;
                            newLightPass->shadowParameters[k] = Vector4(
                                0.5f / (float)light->shadowMap->Width(),
                                0.5f / (float)light->shadowMap->Height(),
                                0.0f, 
                                0.0f
                            );
                            light->SetupShadowMatrices(newLightPass->shadowMatrices, numShadowMatrices);
                            newLightPass->vsBits |= LVS_SHADOW;
                            newLightPass->psBits |= 4 << (k * 3 + 1);
                        }
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
            LightList* lightList = node->lightList;

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

    for (auto it = shadowMaps.Begin(); it != shadowMaps.End(); ++it)
    {
        if (!it->used)
            continue;

        if (it == shadowMaps.Begin())
        {
            for (size_t i = TU_SHADOWMAP; i < TU_SHADOWMAP + MAX_LIGHTS_PER_PASS; ++i)
                graphics->SetTexture(i, nullptr);
        }

        graphics->SetRenderTarget(nullptr, it->texture);
        graphics->Clear(CLEAR_DEPTH);

        for (auto vIt = it->shadowViews.Begin(); vIt < it->shadowViews.End(); ++vIt)
        {
            ShadowView* view = *vIt;
            Light* light = view->light;
            graphics->SetViewport(view->viewport);
            RenderBatches(view->shadowQueue, view->shadowCamera, true, light->DepthBias(), light->SlopeScaledDepthBias());
        }
    }
}

void Renderer::RenderBatches(const String& pass)
{
    unsigned char passIndex = Material::PassIndex(pass);
    BatchQueue& batchQueue = batchQueues[passIndex];
    RenderBatches(batchQueue, camera);
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
    psLightConstantBuffer->Define(USAGE_DEFAULT, constants);

    // Instance vertex buffer contains texcoords 4-6 which define the instances' world matrices
    instanceVertexBuffer = new VertexBuffer();
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 1, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 2, true));

    // Setup ambient light only -pass
    ambientLightPass.vsBits = 0;
    ambientLightPass.psBits = LPS_AMBIENT;
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
                    geometry->OnPrepareRender(camera);
                    geometry->lightList = nullptr;
                    geometry->lastFrameNumber = frameNumber;
                    geometries.Push(geometry);
                }
                else
                {
                    Light* light = static_cast<Light*>(node);
                    light->hasReceivers = false;
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
                    geometry->OnPrepareRender(camera);
                    geometry->lightList = nullptr;
                    geometry->lastFrameNumber = frameNumber;
                    geometries.Push(geometry);
                }
                else
                {
                    Light* light = static_cast<Light*>(node);
                    light->hasReceivers = false;
                    lights.Push(light);
                }
            }
        }
    }
}

void Renderer::AddLightToNode(GeometryNode* node, Light* light, LightList* lightList)
{
    light->hasReceivers = true;

    if (!node->lightList)
    {
        // First light assigned on this frame
        node->lightList = lightList;
        ++lightList->useCount;
    }
    else
    {
        // Create new light list based on the node's existing one
        LightList* oldList = node->lightList;
        --oldList->useCount;
        unsigned long long newListKey = oldList->key;
        newListKey += (unsigned long long)light << ((oldList->lights.Size() & 3) * 16);
        HashMap<unsigned long long, LightList>::Iterator it = lightLists.Find(newListKey);
        if (it != lightLists.End())
        {
            LightList* newList = &it->second;
            node->lightList = newList;
            ++newList->useCount;
        }
        else
        {
            LightList* newList = &lightLists[newListKey];
            newList->key = newListKey;
            newList->lights = oldList->lights;
            newList->lights.Push(light);
            newList->useCount = 1;
            node->lightList = newList;
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
        size_t idx = instanceTransforms.Size();
        InstanceData& instance = *it;
        instance.startIndex = idx;
        instanceTransforms.Resize(idx + instance.worldMatrices.Size());
        for (auto mIt = instance.worldMatrices.Begin(), mEnd = instance.worldMatrices.End(); mIt != mEnd; ++mIt)
            instanceTransforms[idx++] = **mIt;
    }

    if (instanceTransforms.Size() != oldSize)
        instanceTransformsDirty = true;
}

void Renderer::RenderBatches(BatchQueue& batchQueue, Camera* camera, bool overrideDepthBias, int depthBias, float slopeScaledDepthBias)
{
    PROFILE(RenderBatches);

    if (camera != lastCamera)
    {
        PROFILE(SetPerFrameConstants);

        // Set per-frame values to the frame constant buffers
        Matrix3x4 viewMatrix = camera->ViewMatrix();
        Matrix4 projectionMatrix = camera->ProjectionMatrix();
        Matrix4 viewProjMatrix = projectionMatrix * viewMatrix;

        vsFrameConstantBuffer->SetConstant(VS_FRAME_VIEW_MATRIX, viewMatrix);
        vsFrameConstantBuffer->SetConstant(VS_FRAME_PROJECTION_MATRIX, projectionMatrix);
        vsFrameConstantBuffer->SetConstant(VS_FRAME_VIEWPROJ_MATRIX, viewProjMatrix);
        vsFrameConstantBuffer->Apply();

        /// \todo Add also fog settings
        psFrameConstantBuffer->SetConstant(PS_FRAME_AMBIENT_COLOR, camera->AmbientColor());
        psFrameConstantBuffer->Apply();

        graphics->SetConstantBuffer(SHADER_VS, CB_FRAME, vsFrameConstantBuffer);
        graphics->SetConstantBuffer(SHADER_PS, CB_FRAME, psFrameConstantBuffer);

        lastCamera = camera;
    }

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
                        graphics->SetDepthState(pass->depthFunc, pass->depthWrite, pass->depthClip, depthBias, M_INFINITY, slopeScaledDepthBias);
                    
                    if (!camera->UseReverseCulling())
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
                        if (lights->vsBits & LVS_SHADOW)
                        {
                            vsLightConstantBuffer->SetData(lights->shadowMatrices);
                            vsLightConstantBuffer->Apply();
                            graphics->SetConstantBuffer(SHADER_VS, CB_LIGHTS, vsLightConstantBuffer.Get());
                        }
                        
                        psLightConstantBuffer->SetData(lights->lightPositions);
                        psLightConstantBuffer->Apply();
                        graphics->SetConstantBuffer(SHADER_PS, CB_LIGHTS, psLightConstantBuffer.Get());

                        for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
                            graphics->SetTexture(TU_SHADOWMAP + i, lights->shadowMaps[i]);
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
    // Note: includes slow string manipulation, but only when the shader variation is not cached yet
    Vector<WeakPtr<ShaderVariation> >& variations = pass->shaderVariations[stage];
    if (bits < variations.Size() && variations[bits].Get())
        return variations[bits].Get();
    else
    {
        if (variations.Size() <= bits)
            variations.Resize(bits + 1);

        if (stage == SHADER_VS)
        {
            String vsString = pass->combinedShaderDefines[stage] + " " + geometryDefines[bits & LVS_GEOMETRY];
            if (bits & LVS_SHADOW)
                vsString += " " + lightDefines[4];

            variations[bits] = pass->shaders[stage]->CreateVariation(vsString.Trimmed());
            return variations[bits].Get();
        }
        else
        {
            String psString = pass->combinedShaderDefines[SHADER_PS];
            if (bits & LPS_AMBIENT)
                psString += " " + lightDefines[0];
            bool hasShadows = false;
            for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
            {
                unsigned short lightBits = (bits >> (i * 3 + 1)) & 7;
                if (lightBits)
                    psString += " " + lightDefines[lightBits & 3] + String((int)i);
                if (lightBits & 4)
                {
                    psString += " " + lightDefines[4] + String((int)i);
                    hasShadows = true;
                }
            }
            if (hasShadows)
                psString += " " + lightDefines[4];

            variations[bits] = pass->shaders[stage]->CreateVariation(psString.Trimmed());
            return variations[bits].Get();
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
