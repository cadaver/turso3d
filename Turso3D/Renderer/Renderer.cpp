// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Profiler.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Shader.h"
#include "../Graphics/ShaderVariation.h"
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

inline bool CompareBatchState(Batch* lhs, Batch* rhs)
{
    return lhs->sortKey < rhs->sortKey;
}

inline bool CompareBatchDistance(Batch* lhs, Batch* rhs)
{
    return lhs->distance > rhs->distance;
}

void Batch::CalculateSortKey()
{
    // Pass, geometry type and light queue determine the shader to use (highest priority)
    // Material determines texture and renderstate to use (middle priority)
    // Finally geometry (index and vertex buffer assignment) has lowest priority
    sortKey = 
        ((((unsigned long long)pass * source->geometryType * (unsigned long long)lights) & 0xffffff) << 48) |
        ((((unsigned long long)source->material.Get()) & 0xffffff) << 24) |
        (((unsigned)(source->vertexBuffer.Get()) + (unsigned)(source->indexBuffer.Get())) & 0xffffff);
}

Renderer::Renderer()
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

    geometries.Clear();
    batches.Clear();
    sortedBatches.Clear();
    instanceTransforms.Clear();

    scene = scene_;
    camera = camera_;
    octree = scene ? scene->FindChild<Octree>() : nullptr;
    if (!scene || !camera || !octree)
        return;

    Graphics* graphics = Subsystem<Graphics>();
    if (camera->AutoAspectRatio())
    {
        const IntRect& viewport = graphics->Viewport();
        camera->SetAspectRatio((float)viewport.Width() / (float)viewport.Height());
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
        Subsystem<Graphics>()->SetConstantBuffer(SHADER_VS, CB_FRAME, vsFrameConstantBuffer);
    }
}

void Renderer::CollectBatches(const String& pass, BatchSortMode sort)
{
    PROFILE(CollectBatches);

    size_t passIndex = Material::PassIndex(pass);
    Vector<Batch>& batchQueue = batches[pass];
    Vector<Batch*>& sortedBatchQueue = sortedBatches[pass];

    if (sort == SORT_STATE)
    {
        PROFILE(CollectObjectBatches);
        CollectBatchesByState(passIndex, batchQueue);
    }
    else
    {
        PROFILE(CollectObjectBatches);
        CollectBatchesByDistance(passIndex, batchQueue);
    }

    sortedBatchQueue.Resize(batchQueue.Size());
    for (size_t i = 0; i < batchQueue.Size(); ++i)
        sortedBatchQueue[i] = &batchQueue[i];
    
    if (sort == SORT_STATE)
    {
        PROFILE(SortBatchesByState);
        Sort(sortedBatchQueue.Begin(), sortedBatchQueue.End(), CompareBatchState);
    }
    else
    {
        PROFILE(SortBatchesByDistance);
        Sort(sortedBatchQueue.Begin(), sortedBatchQueue.End(), CompareBatchDistance);
    }

    // Batches are sorted. Assign shaders now
    /// \todo Convert static batches which match criteria to instanced
    {
        PROFILE(SetBatchShaders);
        for (auto it = sortedBatchQueue.Begin(); it != sortedBatchQueue.End(); ++it)
            SetBatchShaders(**it);
    }
}

void Renderer::DrawBatches(const String& pass)
{
    PROFILE(DrawBatches);

    Graphics* graphics = Subsystem<Graphics>();
    Vector<Batch*>& sortedBatchQueue = sortedBatches[pass];
    Material* currentMaterial = nullptr;
    Pass* currentPass = nullptr;
    
    for (auto it = sortedBatchQueue.Begin(); it != sortedBatchQueue.End(); ++it)
    {
        Batch* batch = *it;
        const SourceBatch* source = batch->source;
        ShaderVariation* vs = batch->shaders[SHADER_VS];
        ShaderVariation* ps = batch->shaders[SHADER_PS];
        VertexBuffer* vb = source->vertexBuffer;
        IndexBuffer* ib = source->indexBuffer;

        // Shaders and vertex buffer are required
        if (!vs || !ps || !vb)
            continue;

        graphics->SetShaders(vs, ps);

        // Apply pass render state
        if (batch->pass != currentPass)
        {
            currentPass = batch->pass;
            graphics->SetColorState(currentPass->blendMode, currentPass->alphaToCoverage);
            /// \todo Handle depth bias
            graphics->SetDepthState(currentPass->depthFunc, currentPass->depthWrite, currentPass->depthClip);
            graphics->SetRasterizerState(currentPass->cullMode, currentPass->fillMode);
        }

        // Apply material render state
        if (currentPass->Parent() != currentMaterial)
        {
            currentMaterial = currentPass->Parent();
            const HashMap<size_t, SharedPtr<Texture> >& textures = currentMaterial->textures;
            for (auto tIt = textures.Begin(); tIt != textures.End(); ++tIt)
            {
                if (tIt->second)
                    graphics->SetTexture(tIt->first, tIt->second);
            }
            graphics->SetConstantBuffer(SHADER_VS, CB_MATERIAL, currentMaterial->constantBuffers[SHADER_VS]);
            graphics->SetConstantBuffer(SHADER_PS, CB_MATERIAL, currentMaterial->constantBuffers[SHADER_PS]);
        }

        // Apply object render state
        if (source->constantBuffers[SHADER_VS])
            graphics->SetConstantBuffer(SHADER_VS, CB_OBJECT, source->constantBuffers[SHADER_VS]);
        else
        {
            vsObjectConstantBuffer->SetConstant(VS_OBJECT_WORLD_MATRIX, (*source->worldMatrix));
            vsObjectConstantBuffer->Apply();
            graphics->SetConstantBuffer(SHADER_VS, CB_OBJECT, vsObjectConstantBuffer);
        }
        graphics->SetConstantBuffer(SHADER_PS, CB_OBJECT, batch->source->constantBuffers[SHADER_PS]);

        // Draw
        graphics->SetVertexBuffer(0, vb);
        if (ib)
        {
            graphics->SetIndexBuffer(ib);
            graphics->DrawIndexed(source->primitiveType, source->drawStart, batch->source->drawCount, 0);
        }
        else
            graphics->Draw(source->primitiveType, source->drawStart, source->drawCount);
    }
}

void Renderer::CollectBatchesByState(size_t passIndex, Vector<Batch>& batchQueue)
{
    for (auto it = geometries.Begin(); it != geometries.End(); ++it)
    {
        const Vector<SourceBatch>& batches = (*it)->Batches();

        for (auto bIt = batches.Begin(); bIt != batches.End(); ++bIt)
        {
            const SourceBatch& source = *bIt;
            if (!source.material)
                continue;
            Pass* pass = source.material->GetPass(passIndex);
            if (!pass)
                continue;
            
            Batch newBatch;
            newBatch.source = &source;
            newBatch.pass = pass;
            newBatch.lights = nullptr;
            newBatch.instanceCount = 0;
            // Actual shaders are not chosen yet, because it depends on whether the batch will be converted to instanced
            newBatch.CalculateSortKey();
            batchQueue.Push(newBatch);
        }
    }
}

void Renderer::CollectBatchesByDistance(size_t passIndex, Vector<Batch>& batchQueue)
{
    for (auto it = geometries.Begin(); it != geometries.End(); ++it)
    {
        auto batches = (*it)->Batches();

        for (auto bIt = batches.Begin(); bIt != batches.End(); ++bIt)
        {
            const SourceBatch& source = *bIt;
            if (!source.material || !source.vertexBuffer || !source.indexBuffer)
                continue;
            Pass* pass = source.material->GetPass(passIndex);
            if (!pass)
                continue;

            Batch newBatch;
            newBatch.source = &source;
            newBatch.pass = pass;
            newBatch.lights = nullptr;
            newBatch.instanceCount = 0;
            // Actual shaders are not chosen yet, because it depends on whether the batch will be converted to instanced
            newBatch.distance = source.distance;
            batchQueue.Push(newBatch);
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

void Renderer::SetBatchShaders(Batch& batch)
{
    Pass* pass = batch.pass;
    if (!pass->shadersLoaded)
        LoadPassShaders(pass);

    if (pass->shaders[SHADER_VS])
    {
        Vector<WeakPtr<ShaderVariation> >& variations = pass->shaderVariations[SHADER_VS];
        size_t idx = (size_t)batch.source->geometryType;

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

}
