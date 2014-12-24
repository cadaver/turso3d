// For conditions of distribution and use, see copyright notice in License.txt

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
#include "Material.h"
#include "Octree.h"
#include "Renderer.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const String geometryDefines[] =
{
    "",
    "INSTANCED"
};

inline bool CompareBatchState(Batch& lhs, Batch& rhs)
{
    return lhs.sortKey < rhs.sortKey;
}

inline bool CompareBatchDistance(Batch& lhs, Batch& rhs)
{
    return lhs.distance > rhs.distance;
}

inline bool CompareInstanceBatchState(InstanceBatch* lhs, InstanceBatch* rhs)
{
    return lhs->sortKey < rhs->sortKey;
}

void Batch::CalculateSortKey()
{
    // Shaders have highest priority, then material (textures) and finally geometry
    sortKey = ((((unsigned long long)shaders[SHADER_VS] + (unsigned long long)shaders[SHADER_PS]) & 0xffffff) << 48) |
        ((((unsigned long long)pass->Parent()) & 0xffffff) << 24) |
        (((unsigned long long)geometry) & 0xffffff);
}

void PassQueue::Clear()
{
    instanceBatches.Clear();
    sortedInstanceBatches.Clear();
    batches.Clear();
}

void PassQueue::Sort(BatchSortMode sort)
{
    if (sort == SORT_STATE)
    {
        Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchState);
        sortedInstanceBatches.Clear();
        // Make a vector of instance batch pointers as a custom sort of the original hashmap is not achievable
        for (auto it = instanceBatches.Begin(); it != instanceBatches.End(); ++it)
            sortedInstanceBatches.Push(&it->second);
        Turso3D::Sort(sortedInstanceBatches.Begin(), sortedInstanceBatches.End(), CompareInstanceBatchState);
    }
    else
        Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchDistance);
}

Renderer::Renderer() :
    instanceTransformsDirty(false)
{
    Vector<Constant> constants;
    
    vsFrameConstantBuffer = new ConstantBuffer();
    constants.Push(Constant(ELEM_MATRIX3X4, "viewMatrix"));
    constants.Push(Constant(ELEM_MATRIX4, "projectionMatrix"));
    constants.Push(Constant(ELEM_MATRIX4, "viewProjMatrix"));
    vsFrameConstantBuffer->Define(USAGE_DEFAULT, constants);

    /// \todo Define constants used by this buffer
    psFrameConstantBuffer = new ConstantBuffer();

    vsObjectConstantBuffer = new ConstantBuffer();
    constants.Clear();
    constants.Push(Constant(ELEM_MATRIX3X4, "worldMatrix"));
    vsObjectConstantBuffer->Define(USAGE_DEFAULT, constants);

    // Instance vertex buffer contains texcoords 4-6 which define the instances' world matrices
    instanceVertexBuffer = new VertexBuffer();
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 1, true));
    instanceVertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, INSTANCE_TEXCOORD + 2, true));
}

Renderer::~Renderer()
{
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
    Material::RegisterObject();
    Octree::RegisterObject();
}

void Renderer::CollectObjects(Scene* scene_, Camera* camera_)
{
    PROFILE(CollectObjects);

    // Acquire Graphics subsystem now
    if (!graphics)
        graphics = Subsystem<Graphics>();

    geometries.Clear();
    instanceTransforms.Clear();
    // Clear batches from each pass
    for (auto it = passQueues.Begin(); it != passQueues.End(); ++it)
        it->second.Clear();

    scene = scene_;
    camera = camera_;
    octree = scene ? scene->FindChild<Octree>() : nullptr;
    if (!scene || !camera || !octree)
        return;

    if (camera->AutoAspectRatio())
    {
        const IntRect& viewport = graphics->Viewport();
        camera->SetAspectRatioInternal((float)viewport.Width() / (float)viewport.Height());
    }

    // Make sure all scene nodes have been inserted to correct octants
    octree->Update();

    frustum = camera->WorldFrustum();
    viewMatrix = camera->ViewMatrix();
    projectionMatrix = camera->ProjectionMatrix();
    viewProjMatrix = projectionMatrix * viewMatrix;

    /// \todo When Light class exists, get lights & geometries separately. Now just collects geometries
    octree->FindNodes(reinterpret_cast<Vector<OctreeNode*>&>(geometries), frustum, NF_GEOMETRY, camera->ViewMask());

    {
        PROFILE(PrepareRender);
        for (auto it = geometries.Begin(); it != geometries.End(); ++it)
            (*it)->OnPrepareRender(camera);
    }

    {
        PROFILE(SetPerFrameConstants);
        // Set per-frame values to the frame constant buffer
        vsFrameConstantBuffer->SetConstant(VS_SCENE_VIEW_MATRIX, viewMatrix);
        vsFrameConstantBuffer->SetConstant(VS_SCENE_PROJECTION_MATRIX, projectionMatrix);
        vsFrameConstantBuffer->SetConstant(VS_SCENE_VIEWPROJ_MATRIX, viewProjMatrix);
        vsFrameConstantBuffer->Apply();
        graphics->SetConstantBuffer(SHADER_VS, CB_FRAME, vsFrameConstantBuffer);
    }
}

void Renderer::CollectBatches(const String& pass, BatchSortMode sort)
{
    size_t passIndex = Material::PassIndex(pass);
    PassQueue& passQueue = passQueues[passIndex];

    {
        PROFILE(CollectBatches);
        if (sort == SORT_STATE)
            CollectBatchesByState(passIndex, passQueue);
        else
            CollectBatchesByDistance(passIndex, passQueue);
    }

    {
        PROFILE(SortBatches);
        passQueue.Sort(sort);
    }

    {
        PROFILE(BuildInstanceTransforms);

        // Assign start index for each instance and copy the world transforms for eventual vertex buffer upload
        for (auto it = passQueue.instanceBatches.Begin(); it != passQueue.instanceBatches.End(); ++it)
        {
            it->second.startIndex = instanceTransforms.Size();
            for (auto it2 = it->second.worldMatrices.Begin(); it2 != it->second.worldMatrices.End(); ++it2)
                instanceTransforms.Push(**it2);
        }

        instanceTransformsDirty = true;
    }
}

void Renderer::RenderBatches(const String& pass)
{
    if (instanceTransformsDirty && instanceTransforms.Size())
    {
        PROFILE(SetInstanceTransforms);
        if (instanceVertexBuffer->NumVertices() < instanceTransforms.Size())
            instanceVertexBuffer->Define(USAGE_DYNAMIC, instanceTransforms.Size(), instanceVertexElements, false, &instanceTransforms[0]);
        else
            instanceVertexBuffer->SetData(0, instanceTransforms.Size(), &instanceTransforms[0]);

        instanceTransformsDirty = false;
    }

    {
        PROFILE(RenderBatches);

        size_t passIndex = Material::PassIndex(pass);
        PassQueue& passQueue = passQueues[passIndex];
        Pass* lastPass = nullptr;
        Material* lastMaterial = nullptr;
        
        if (passQueue.sortedInstanceBatches.Size())
        {
            graphics->SetVertexBuffer(1, instanceVertexBuffer.Get());
            for (auto it = passQueue.sortedInstanceBatches.Begin(); it != passQueue.sortedInstanceBatches.End(); ++it)
                RenderBatch(**it, true, lastPass, lastMaterial);
            graphics->SetVertexBuffer(1, nullptr);
        }
        for (auto it = passQueue.batches.Begin(); it != passQueue.batches.End(); ++it)
            RenderBatch(*it, false, lastPass, lastMaterial);
    }
}

void Renderer::CollectBatchesByState(size_t passIndex, PassQueue& passQueue)
{
    InstanceKey lastInstanceKey(nullptr, nullptr, nullptr);
    InstanceBatch* lastInstanceBatch = nullptr;

    for (auto it = geometries.Begin(); it != geometries.End(); ++it)
    {
        GeometryNode* node = *it;
        const Vector<SourceBatch>& sourceBatches = node->Batches();
        const Matrix3x4& worldMatrix = node->WorldTransform();
        GeometryType type = node->GetGeometryType();

        for (auto it2 = sourceBatches.Begin(); it2 != sourceBatches.End(); ++it2)
        {
            Geometry* geometry = it2->geometry.Get();
            Material* material = it2->material.Get();

            if (!geometry || !material)
                continue;
            Pass* pass = material->GetPass(passIndex);
            if (!pass)
                continue;

            // Static, state-sorted geometry always goes into an instance group
            /// \todo Check whether this makes sense
            if (type == GEOM_STATIC)
            {
                InstanceKey key(geometry, pass, nullptr);
                // Optimize if last instance batch is the same
                if (key == lastInstanceKey)
                    lastInstanceBatch->worldMatrices.Push(&worldMatrix);
                else
                {
                    auto it = passQueue.instanceBatches.Find(key);
                    if (it != passQueue.instanceBatches.End())
                    {
                        // Append to existing instance batch
                        it->second.worldMatrices.Push(&worldMatrix);
                        lastInstanceKey = key;
                        lastInstanceBatch = &it->second;
                    }
                    else
                    {
                        // Create new instance batch
                        InstanceBatch& newBatch = passQueue.instanceBatches[key];
                        newBatch.geometry = geometry;
                        newBatch.pass = pass;
                        newBatch.lights = nullptr;
                        SetBatchShaders(newBatch, GEOM_INSTANCED);
                        newBatch.CalculateSortKey();
                        
                        newBatch.worldMatrices.Push(&worldMatrix);
                        lastInstanceKey = key;
                        lastInstanceBatch = &newBatch;
                    }
                }
            }
            else
            {
                Batch newBatch;
                newBatch.geometry = geometry;
                newBatch.pass = pass;
                newBatch.lights = nullptr;
                newBatch.worldMatrix = &worldMatrix;
                SetBatchShaders(newBatch, type);
                newBatch.CalculateSortKey();
                passQueue.batches.Push(newBatch);
            }
        }
    }
}

void Renderer::CollectBatchesByDistance(size_t passIndex, PassQueue& passQueue)
{
    for (auto it = geometries.Begin(); it != geometries.End(); ++it)
    {
        GeometryNode* node = *it;
        const Vector<SourceBatch>& sourceBatches = node->Batches();
        const Matrix3x4& worldMatrix = node->WorldTransform();
        GeometryType type = node->GetGeometryType();
        float distance = node->Distance();

        for (auto it2 = sourceBatches.Begin(); it2 != sourceBatches.End(); ++it2)
        {
            Geometry* geometry = it2->geometry.Get();
            Material* material = it2->material.Get();

            if (!geometry || !material)
                continue;
            Pass* pass = material->GetPass(passIndex);
            if (!pass)
                continue;

            Batch newBatch;
            newBatch.geometry = geometry;
            newBatch.pass = pass;
            newBatch.lights = nullptr;
            newBatch.worldMatrix = &worldMatrix;
            newBatch.distance = distance;
            SetBatchShaders(newBatch, type);
            passQueue.batches.Push(newBatch);
        }
    }
}

void Renderer::SetBatchShaders(Batch& batch, GeometryType type)
{
    Pass* pass = batch.pass;
    if (!pass->shadersLoaded)
        LoadPassShaders(pass);

    if (pass->shaders[SHADER_VS])
    {
        Vector<WeakPtr<ShaderVariation> >& variations = pass->shaderVariations[SHADER_VS];
        size_t idx = type;

        if (variations.Size() <= idx)
            variations.Resize(idx + 1);
        if (!variations[idx])
            variations[idx] = pass->shaders[SHADER_VS]->CreateVariation((pass->combinedShaderDefines[SHADER_VS] + " " + geometryDefines[idx]).Trimmed());

        batch.shaders[SHADER_VS] = variations[idx];
    }
    else
        batch.shaders[SHADER_VS] = nullptr;

    if (pass->shaders[SHADER_PS])
    {
        Vector<WeakPtr<ShaderVariation> >& variations = pass->shaderVariations[SHADER_PS];
        size_t idx = 0; /// \todo Handle light types

        if (variations.Size() <= idx)
            variations.Resize(idx + 1);
        if (!variations[idx])
            variations[idx] = pass->shaders[SHADER_PS]->CreateVariation(pass->combinedShaderDefines[SHADER_PS]);

        batch.shaders[SHADER_PS] = variations[idx];
    }
    else
        batch.shaders[SHADER_PS] = nullptr;
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

void Renderer::RenderBatch(Batch& batch, bool isInstanced, Pass*& lastPass, Material*& lastMaterial)
{
    ShaderVariation* vs = batch.shaders[SHADER_VS];
    ShaderVariation* ps = batch.shaders[SHADER_PS];
    Geometry* geometry = batch.geometry;
    VertexBuffer* vb = geometry->vertexBuffer.Get();
    IndexBuffer* ib = geometry->indexBuffer.Get();

    // Shaders and vertex buffer are required
    if (!vs || !ps || !vb)
        return;

    graphics->SetShaders(vs, ps);
    graphics->SetVertexBuffer(0, vb);
    if (ib)
        graphics->SetIndexBuffer(ib);

    // Apply pass render state
    Pass* pass = batch.pass;
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
    else if (!isInstanced)
    {
        vsObjectConstantBuffer->SetConstant(VS_OBJECT_WORLD_MATRIX, *batch.worldMatrix);
        vsObjectConstantBuffer->Apply();
        graphics->SetConstantBuffer(SHADER_VS, CB_OBJECT, vsObjectConstantBuffer.Get());
    }
    graphics->SetConstantBuffer(SHADER_PS, CB_OBJECT, geometry->constantBuffers[SHADER_PS].Get());

    if (!isInstanced)
    {
        if (ib)
            graphics->DrawIndexed(geometry->primitiveType, geometry->drawStart, geometry->drawCount, 0);
        else
            graphics->Draw(geometry->primitiveType, geometry->drawStart, geometry->drawCount);
    }
    else
    {
        InstanceBatch& instanceBatch = static_cast<InstanceBatch&>(batch);

        if (ib)
        {
            graphics->DrawIndexedInstanced(geometry->primitiveType, geometry->drawStart, geometry->drawCount, 0,
                instanceBatch.startIndex, instanceBatch.worldMatrices.Size());
        }
        else
        {
            graphics->DrawInstanced(geometry->primitiveType, geometry->drawStart, geometry->drawCount, instanceBatch.startIndex,
                instanceBatch.worldMatrices.Size());
        }
    }
}

}
