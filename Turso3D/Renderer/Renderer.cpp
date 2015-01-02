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

static const unsigned LIGHTBIT_AMBIENT = 0x1;
static const unsigned LIGHTBITS_LIGHT0 = 0x6;
static const unsigned LIGHTBITS_LIGHT1 = 0x18;
static const unsigned LIGHTBITS_LIGHT2 = 0x60;
static const unsigned LIGHTBITS_LIGHT3 = 0x180;

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

ShadowView::ShadowView() :
    shadowCamera(new Camera())
{
}

ShadowView::~ShadowView()
{
}

Renderer::Renderer() :
    frameNumber(0),
    usedShadowViews(0),
    instanceTransformsDirty(false),
    perFrameConstantsSet(false)
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
        it->texture->Define(TEX_2D, USAGE_RENDERTARGET, IntVector2(size, size), format, 1);
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
    shadowLights.Clear();
    for (auto it = shadowViews.Begin(); it != shadowViews.End(); ++it)
        it->shadowQueue.Clear();
    usedShadowViews = 0;
    perFrameConstantsSet = false;

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
    viewMatrix = camera->ViewMatrix();
    projectionMatrix = camera->ProjectionMatrix();
    viewProjMatrix = projectionMatrix * viewMatrix;
    viewMask = camera->ViewMask();

    octree->FindNodes(frustum, this, &Renderer::CollectGeometriesAndLights);
    
    CollectLightInteractions();
}

void Renderer::CollectLightInteractions()
{
    {
        PROFILE(CollectLightInteractions);

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
        PROFILE(BuildLightPasses);
        
        /// \todo Some light lists may not be referred to by any nodes. Creating passes for them is a waste
        for (auto it = lightLists.Begin(), end = lightLists.End(); it != end; ++it)
        {
            LightList& list = it->second;
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
                    newLightPass->psIdx = (i == 0) ? LIGHTBIT_AMBIENT : 0;
                    for (size_t j = i; j < list.lights.Size() && j < i + MAX_LIGHTS_PER_PASS; ++j)
                    {
                        Light* light = list.lights[j];
                        size_t k = j & 3;
                        newLightPass->psIdx |= (light->GetLightType() + 1) << (k * 2 + 1);
                        float cutoff = cosf(light->Fov() * 0.5f * M_DEGTORAD);
                        newLightPass->lightPositions[k] = Vector4(light->WorldPosition(), 1.0f);
                        newLightPass->lightDirections[k] = Vector4(-light->WorldDirection(), 0.0f);
                        newLightPass->lightAttenuations[k] = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f /
                            (1.0f - cutoff), 0.0f);
                        newLightPass->lightColors[k] = light->GetColor();
                    }
                    list.lightPasses.Push(newLightPass);
                }
            }
        }
    }

    {
        /// \todo Thread this
        PROFILE(ProcessShadowLights);

        for (auto it = lights.Begin(), end = lights.End(); it != end; ++it)
        {
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

        // Loop through scene nodes
        for (auto gIt = geometries.Begin(), gEnd = geometries.End(); gIt != gEnd; ++gIt)
        {
            GeometryNode* node = *gIt;
            const Vector<SourceBatch>& sourceBatches = node->Batches();

            LightList* lightList = node->lightList;

            // Loop through node's batches
            for (auto bIt = sourceBatches.Begin(), bEnd = sourceBatches.End(); bIt != bEnd; ++bIt)
            {
                Batch newBatch;
                newBatch.geometry = bIt->geometry.Get();
                Material* material = bIt->material.Get();
                assert(material);

                // Loop through requested passes
                for (auto pIt = currentQueues.Begin(), pEnd = currentQueues.End(); pIt != pEnd; ++pIt)
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

void Renderer::RenderBatches(const String& pass)
{
    PROFILE(RenderBatches);

    if (!perFrameConstantsSet)
    {
        PROFILE(SetPerFrameConstants);

        // Set per-frame values to the frame constant buffers
        vsFrameConstantBuffer->SetConstant(VS_FRAME_VIEW_MATRIX, viewMatrix);
        vsFrameConstantBuffer->SetConstant(VS_FRAME_PROJECTION_MATRIX, projectionMatrix);
        vsFrameConstantBuffer->SetConstant(VS_FRAME_VIEWPROJ_MATRIX, viewProjMatrix);
        vsFrameConstantBuffer->Apply();
        
        /// \todo Store the ambient color value somewhere, for example Camera. Also add fog settings
        psFrameConstantBuffer->SetConstant(PS_FRAME_AMBIENT_COLOR, Color(0.5f, 0.5f, 0.5f, 1.0f));
        psFrameConstantBuffer->Apply();

        graphics->SetConstantBuffer(SHADER_VS, CB_FRAME, vsFrameConstantBuffer);
        graphics->SetConstantBuffer(SHADER_PS, CB_FRAME, psFrameConstantBuffer);
        perFrameConstantsSet = true;
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

        unsigned char passIndex = Material::PassIndex(pass);
        BatchQueue& batchQueue = batchQueues[passIndex];
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
                ShaderVariation* vs = FindShaderVariation(SHADER_VS, pass, batch.type);
                ShaderVariation* ps = FindShaderVariation(SHADER_PS, pass, lights ? lights->psIdx : 0);
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
                    graphics->SetColorState(pass->blendMode, pass->alphaToCoverage);
                    /// \todo Handle depth bias
                    graphics->SetDepthState(pass->depthFunc, pass->depthWrite, pass->depthClip);
                    graphics->SetRasterizerState(pass->cullMode, pass->fillMode);

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
            
                // Apply light constant buffer
                if (lights && lights != lastLights)
                {
                    // If light queue is ambient only, no need to update the constants
                    if (lights->psIdx > LIGHTBIT_AMBIENT)
                    {
                        psLightConstantBuffer->SetData(lights->lightPositions);
                        psLightConstantBuffer->Apply();
                        graphics->SetConstantBuffer(SHADER_PS, CB_LIGHTS, psLightConstantBuffer.Get());
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

    psLightConstantBuffer = new ConstantBuffer();
    constants.Clear();
    constants.Push(Constant(ELEM_VECTOR4, "lightPositions", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "lightDirections", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "lightColors", MAX_LIGHTS_PER_PASS));
    constants.Push(Constant(ELEM_VECTOR4, "lightAttenuations", MAX_LIGHTS_PER_PASS));
    psLightConstantBuffer->Define(USAGE_DEFAULT, constants);

    // Instance vertex buffer contains texcoords 4-6 which define the instances' world matrices
    instanceVertexBuffer = new VertexBuffer();
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 1, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 2, true));

    // Setup ambient light only -pass
    ambientLightPass.psIdx = LIGHTBIT_AMBIENT;
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
            if ((node->LayerMask() & viewMask) && ((flags & (NF_ENABLED | NF_GEOMETRY | NF_LIGHT)) > NF_ENABLED))
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
            if ((node->LayerMask() & viewMask) && ((flags & (NF_ENABLED | NF_GEOMETRY | NF_LIGHT)) > NF_ENABLED) &&
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
    }
    else
    {
        // Create new light list based on the node's existing one
        LightList* oldList = node->lightList;
        unsigned long long newListKey = oldList->key;
        newListKey += (unsigned long long)light << ((oldList->lights.Size() & 3) * 16);
        HashMap<unsigned long long, LightList>::Iterator it = lightLists.Find(newListKey);
        if (it != lightLists.End())
            node->lightList = &it->second;
        else
        {
            LightList* newList = &lightLists[newListKey];
            newList->key = newListKey;
            newList->lights = oldList->lights;
            newList->lights.Push(light);
            node->lightList = newList;
        }
    }
}

void Renderer::CopyInstanceTransforms(BatchQueue& batchQueue)
{
    PROFILE(CopyInstanceTransforms);

    // Now go through all instance batches and copy to the global buffer
    size_t oldSize = instanceTransforms.Size();
    for (auto it = batchQueue.instanceDatas.Begin(); it != batchQueue.instanceDatas.End(); ++it)
    {
        size_t idx = instanceTransforms.Size();
        InstanceData& instance = *it;
        instance.startIndex = idx;
        instanceTransforms.Resize(idx + instance.worldMatrices.Size());
        for (auto mIt = instance.worldMatrices.Begin(); mIt != instance.worldMatrices.End(); ++mIt)
            instanceTransforms[idx++] = **mIt;
    }

    if (instanceTransforms.Size() != oldSize)
        instanceTransformsDirty = true;
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

ShaderVariation* Renderer::FindShaderVariation(ShaderStage stage, Pass* pass, size_t idx)
{
    // Note: includes slow string manipulation, but only when the shader variation is not cached yet
    Vector<WeakPtr<ShaderVariation> >& variations = pass->shaderVariations[stage];
    if (idx < variations.Size() && variations[idx].Get())
        return variations[idx].Get();
    else
    {
        if (variations.Size() <= idx)
            variations.Resize(idx + 1);

        if (stage == SHADER_VS)
        {
            variations[idx] = pass->shaders[stage]->CreateVariation((pass->combinedShaderDefines[stage] + " " +
                geometryDefines[idx]).Trimmed());
            return variations[idx].Get();
        }
        else
        {
            String psString = pass->combinedShaderDefines[SHADER_PS];
            if (idx & LIGHTBIT_AMBIENT)
                psString += " " + lightDefines[0];
            for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
            {
                if (idx & (LIGHTBITS_LIGHT0 << (i * 2)))
                    psString += " " + lightDefines[(idx >> (i * 2 + 1)) & 3] + String((int)i);
            }
            variations[idx] = pass->shaders[stage]->CreateVariation(psString.Trimmed());
            return variations[idx].Get();
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
