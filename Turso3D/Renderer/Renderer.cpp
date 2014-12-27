// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Shader.h"
#include "../Graphics/ShaderVariation.h"
#include "../Graphics/VertexBuffer.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "Camera.h"
#include "GeometryNode.h"
#include "Light.h"
#include "Material.h"
#include "Octree.h"
#include "Renderer.h"

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

void Batch::CalculateSortKey()
{
    // Shaders (pass + light queue) have highest priority, then material and finally geometry
    sortKey = ((((unsigned long long)pass + (unsigned long long)lights + type) & 0xffffff) << 48) |
        ((((unsigned long long)pass->Parent()) & 0xffffff) << 24) |
        (((unsigned long long)geometry) & 0xffffff);
}

void BatchQueue::Clear()
{
    batches.Clear();
    instanceDatas.Clear();
    instanceLookup.Clear();
}

Renderer::Renderer() :
    frameNumber(0),
    instanceTransformsDirty(false),
    objectsPrepared(false),
    perFrameConstantsSet(false)
{
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
    psLightConstantBuffer->Define(USAGE_DEFAULT, constants);

    // Instance vertex buffer contains texcoords 4-6 which define the instances' world matrices
    instanceVertexBuffer = new VertexBuffer();
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 1, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 2, true));
}

Renderer::~Renderer()
{
}

void Renderer::CollectObjects(Scene* scene_, Camera* camera_)
{
    PROFILE(CollectObjects);

    // Acquire Graphics subsystem now
    if (!graphics)
        graphics = Subsystem<Graphics>();

    geometries.Clear();
    lights.Clear();
    instanceTransforms.Clear();
    lightQueues.Clear();
    for (auto it = batchQueues.Begin(); it != batchQueues.End(); ++it)
        it->second.Clear();

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

    // Make sure octree is up to date.
    octree->Update();

    frustum = camera->WorldFrustum();
    viewMatrix = camera->ViewMatrix();
    projectionMatrix = camera->ProjectionMatrix();
    viewProjMatrix = projectionMatrix * viewMatrix;

    octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(geometries), NF_GEOMETRY, reinterpret_cast<Vector<OctreeNode*>&>(lights),
        NF_LIGHT, frustum, camera->ViewMask());
    
    objectsPrepared = false;
    perFrameConstantsSet = false;
}

void Renderer::CollectLightInteractions()
{
    PROFILE(CollectLightInteractions);

    for (auto it = lights.Begin(); it != lights.End(); ++it)
    {
        Light* light = *it;
        unsigned lightMask = light->LightMask();

        switch (light->GetLightType())
        {
        case LIGHT_DIRECTIONAL:
            for (auto gIt = geometries.Begin(); gIt != geometries.End(); ++gIt)
            {
                GeometryNode* node = *gIt;
                if (lightMask & node->LayerMask())
                    node->AddLight(frameNumber, light);
            }
            break;

        case LIGHT_POINT:
            litGeometries.Clear();
            octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(litGeometries), light->WorldSphere(), NF_GEOMETRY, lightMask);
            for (auto gIt = litGeometries.Begin(); gIt != litGeometries.End(); ++gIt)
                (*gIt)->AddLight(frameNumber, light);
            break;

        case LIGHT_SPOT:
            litGeometries.Clear();
            octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(litGeometries), light->WorldFrustum(), NF_GEOMETRY, lightMask);
            for (auto gIt = litGeometries.Begin(); gIt != litGeometries.End(); ++gIt)
                (*gIt)->AddLight(frameNumber, light);
            break;
        }
    }
}

void Renderer::CollectBatches(const String& pass, BatchSortMode sort, bool lit)
{
    PROFILE(CollectBatches);

    size_t passIndex = Material::PassIndex(pass);
    BatchQueue& batchQueue = batchQueues[passIndex];
    
    {
        PROFILE(BuildBatches);

        for (auto it = geometries.Begin(); it != geometries.End(); ++it)
        {
            GeometryNode* node = *it;
            const Vector<SourceBatch>& sourceBatches = node->Batches();
            const Matrix3x4& worldMatrix = node->WorldTransform();
            GeometryType type = node->GetGeometryType();
            float distance = node->SquaredDistance();

            // Prepare objects for rendering when collecting the first pass for this frame
            // to avoid multiple loops through a (potentially large) number of objects
            if (!objectsPrepared)
                node->OnPrepareRender(frameNumber, camera);

            /// \todo handle light queue "spill over" if need to split into multiple additive passes
            LightQueue* lights = nullptr;
            if (lit)
            {
                const Vector<Light*>& nodeLights = node->Lights();
                unsigned long long lightQueueKey = 0;
                for (size_t i = 0; i < nodeLights.Size() && i < MAX_LIGHTS_PER_PASS; ++i)
                    lightQueueKey += ((unsigned long long)nodeLights[i] & 0xffff) << (i * 16);

                HashMap<unsigned long long, LightQueue>::Iterator it = lightQueues.Find(lightQueueKey);
                if (it != lightQueues.End())
                    lights = &it->second;
                else
                {
                    /// Create new light queue
                    LightQueue* lights = &lightQueues[lightQueueKey];
                    lights->psIdx = LIGHTBIT_AMBIENT;
                    for (size_t i = 0; i < nodeLights.Size() && i < MAX_LIGHTS_PER_PASS; ++i)
                    {
                        lights->psIdx |= ((unsigned)nodeLights[i]->GetLightType() + 1) << (i * 2 + 1);
                        lights->lightPositions[i] = Vector4(nodeLights[i]->WorldPosition(), nodeLights[i]->Range());
                        lights->lightColors[i] = nodeLights[i]->GetColor();
                        lights->lightDirections[i] = Vector4(-nodeLights[i]->WorldDirection(), nodeLights[i]->Fov());
                    }
                }
            }

            for (auto bIt = sourceBatches.Begin(); bIt != sourceBatches.End(); ++bIt)
            {
                Batch newBatch;

                Material* material = bIt->material.Get();
                newBatch.pass = material ? material->GetPass(passIndex) : nullptr;
                // Material may not have the requested pass at all, skip further processing as fast as possible in that case
                if (!newBatch.pass)
                    continue;

                newBatch.type = type;
                newBatch.geometry = bIt->geometry.Get();
                newBatch.lights = lights;

                if (sort == SORT_STATE)
                {
                    if (newBatch.type == GEOM_STATIC)
                    {
                        /// \todo Other checks for whether to instance
                        newBatch.type = GEOM_INSTANCED;
                        newBatch.CalculateSortKey();

                        // Check if instance batch already exists
                        auto iIt = batchQueue.instanceLookup.Find(newBatch.sortKey);
                        if (iIt != batchQueue.instanceLookup.End())
                            batchQueue.instanceDatas[iIt->second].worldMatrices.Push(&worldMatrix);
                        else
                        {
                            // Begin new instanced batch
                            size_t newInstanceDataIndex = batchQueue.instanceDatas.Size();
                            batchQueue.instanceLookup[newBatch.sortKey] = newInstanceDataIndex;
                            newBatch.instanceDataIndex = newInstanceDataIndex;
                            batchQueue.batches.Push(newBatch);
                            batchQueue.instanceDatas.Resize(batchQueue.instanceDatas.Size() + 1);
                            InstanceData& newInstanceData = batchQueue.instanceDatas.Back();
                            newInstanceData.skipBatches = false;
                            newInstanceData.worldMatrices.Push(&worldMatrix);
                        }
                    }
                    else
                    {
                        newBatch.worldMatrix = &worldMatrix;
                        newBatch.CalculateSortKey();
                        batchQueue.batches.Push(newBatch);
                    }
                }
                else
                {
                    newBatch.worldMatrix = &worldMatrix;
                    newBatch.distance = distance;
                    batchQueue.batches.Push(newBatch);
                }
            }
        }

        objectsPrepared = true;
    }

    {
        PROFILE(SortBatches);

        if (sort == SORT_STATE)
            Sort(batchQueue.batches.Begin(), batchQueue.batches.End(), CompareBatchState);
        else
        {
            Sort(batchQueue.batches.Begin(), batchQueue.batches.End(), CompareBatchDistance);

            // After sorting batches by distance, we need a separate step to build instances if adjacent batches have the same state
            Batch* start = nullptr;
            for (auto it = batchQueue.batches.Begin(); it != batchQueue.batches.End(); ++it)
            {
                Batch* current = it.ptr;
                if (start && current->type == GEOM_STATIC && current->pass == start->pass && current->geometry == start->geometry &&
                    current->lights == start->lights)
                {
                    if (start->type == GEOM_INSTANCED)
                        batchQueue.instanceDatas[start->instanceDataIndex].worldMatrices.Push(current->worldMatrix);
                    else
                    {
                        // Begin new instanced batch
                        start->type = GEOM_INSTANCED;
                        size_t newInstanceDataIndex = batchQueue.instanceDatas.Size();
                        batchQueue.instanceDatas.Resize(batchQueue.instanceDatas.Size() + 1);
                        InstanceData& newInstanceData = batchQueue.instanceDatas.Back();
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

    {
        PROFILE(CopyInstanceTransforms);

        // Now go through all instance batches and copy to the global buffer
        size_t oldSize = instanceTransforms.Size();
        for (auto it = batchQueue.instanceDatas.Begin(); it != batchQueue.instanceDatas.End(); ++it)
        {
            InstanceData& instance = *it;
            instance.startIndex = instanceTransforms.Size();
            for (auto mIt = instance.worldMatrices.Begin(); mIt != instance.worldMatrices.End(); ++mIt)
                instanceTransforms.Push(**mIt);
        }

        if (instanceTransforms.Size() != oldSize)
            instanceTransformsDirty = true;
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

        size_t passIndex = Material::PassIndex(pass);
        BatchQueue& batchQueue = batchQueues[passIndex];
        Vector<Batch>& batches = batchQueue.batches;
        Pass* lastPass = nullptr;
        Material* lastMaterial = nullptr;
        LightQueue* lastLights = nullptr;
        
        for (auto it = batches.Begin(); it != batches.End();)
        {
            const Batch& batch = *it;
            InstanceData* instance = batch.type == GEOM_INSTANCED ? &batchQueue.instanceDatas[batch.instanceDataIndex] : nullptr;

            Pass* pass = batch.pass;
            if (!pass->shadersLoaded)
                LoadPassShaders(pass);
            
            // Check that pass is legal
            if (pass->shaders[SHADER_VS].Get() && pass->shaders[SHADER_PS].Get() && batch.geometry)
            {
                // Get the vertex shader variation
                Vector<WeakPtr<ShaderVariation> >& vsVariations = pass->shaderVariations[SHADER_VS];
                size_t vsIdx = batch.type;
                if (vsVariations.Size() <= vsIdx)
                    vsVariations.Resize(vsIdx + 1);
                if (!vsVariations[vsIdx])
                {
                    // Note: slow string manipulation, but done only once when the shader variation is not cached yet
                    vsVariations[vsIdx] = pass->shaders[SHADER_VS]->CreateVariation((pass->combinedShaderDefines[SHADER_VS] +
                        " " + geometryDefines[vsIdx]).Trimmed());
                }
                ShaderVariation* vs = vsVariations[vsIdx];

                // Get the pixel shader variation
                LightQueue* lights = batch.lights;
                Vector<WeakPtr<ShaderVariation> >& psVariations = pass->shaderVariations[SHADER_PS];
                size_t psIdx = lights ? batch.lights->psIdx : 0;
                if (psVariations.Size() <= psIdx)
                    psVariations.Resize(psIdx + 1);
                if (!psVariations[psIdx])
                {
                    // Note: slow string manipulation, but done only once when the shader variation is not cached yet
                    String psString = pass->combinedShaderDefines[SHADER_PS];
                    if (psIdx & LIGHTBIT_AMBIENT)
                        psString += " " + lightDefines[0];
                    for (size_t i = 0; i < MAX_LIGHTS_PER_PASS; ++i)
                    {
                        if (psIdx & (LIGHTBITS_LIGHT0 << (i * 2)))
                            psString += " " + lightDefines[(psIdx >> (i * 2 + 1)) & 3] + String((int)i);
                    }
                    psVariations[psIdx] = pass->shaders[SHADER_PS]->CreateVariation(psString.Trimmed());
                }
                ShaderVariation* ps = psVariations[psIdx];

                graphics->SetShaders(vs, ps);

                // Set batch geometry
                Geometry* geometry = batch.geometry;
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
                    const HashMap<size_t, SharedPtr<Texture> >& textures = material->textures;
                    for (auto it = textures.Begin(); it != textures.End(); ++it)
                    {
                        if (it->second)
                            graphics->SetTexture(it->first, it->second.Get());
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
                        psLightConstantBuffer->SetConstant(PS_LIGHT_POSITIONS, lights->lightPositions[0]);
                        psLightConstantBuffer->SetConstant(PS_LIGHT_DIRECTIONS, lights->lightDirections[0]);
                        psLightConstantBuffer->SetConstant(PS_LIGHT_COLORS, lights->lightColors[0]);
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

void RegisterRendererLibrary()
{
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    // Scene node base attributes are needed
    RegisterSceneLibrary();
    Camera::RegisterObject();
    GeometryNode::RegisterObject();
    Light::RegisterObject();
    Material::RegisterObject();
    Octree::RegisterObject();
}

}
