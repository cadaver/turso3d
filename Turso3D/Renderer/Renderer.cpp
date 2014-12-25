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

void Batch::CalculateSortKey()
{
    // Shaders (pass + light queue) have highest priority, then material and finally geometry
    sortKey = ((((unsigned long long)pass + (unsigned long long)lights) & 0xffffff) << 48) |
        ((((unsigned long long)pass->Parent()) & 0xffffff) << 24) |
        (((unsigned long long)geometry) & 0xffffff);
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
    for (auto it = batchQueues.Begin(); it != batchQueues.End(); ++it)
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
    Vector<Batch>& batchQueue = batchQueues[passIndex];

    {
        PROFILE(CollectBatches);

        for (auto it = geometries.Begin(); it != geometries.End(); ++it)
        {
            GeometryNode* node = *it;
            const Vector<SourceBatch>& sourceBatches = node->Batches();
            const Matrix3x4& worldMatrix = node->WorldTransform();
            GeometryType type = node->GetGeometryType();
            float distance = node->SquaredDistance();

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
                newBatch.lights = nullptr;
                newBatch.worldMatrix = &worldMatrix;

                if (sort == SORT_STATE)
                    newBatch.CalculateSortKey();
                else
                    newBatch.distance = distance;

                batchQueue.Push(newBatch);
            }
        }
    }

    {
        PROFILE(SortBatches);
        
        if (sort == SORT_STATE)
            Sort(batchQueue.Begin(), batchQueue.End(), CompareBatchState);
        else
            Sort(batchQueue.Begin(), batchQueue.End(), CompareBatchDistance);
    }

    {
        PROFILE(BuildInstances);

        size_t oldSize = instanceTransforms.Size();

        // Build instances when batches next to each other have the same state
        Batch* start = nullptr;
        for (auto it = batchQueue.Begin(); it != batchQueue.End(); ++it)
        {
            Batch* current = it.ptr;
            if (start && current->type == GEOM_STATIC && current->pass == start->pass && current->geometry == start->geometry &&
                current->lights == start->lights)
            {
                if (start->type == GEOM_INSTANCED)
                {
                    instanceTransforms.Push(*current->worldMatrix);
                    ++start->instanceCount;
                }
                else
                {
                    // Begin new instanced batch
                    instanceTransforms.Push(*start->worldMatrix);
                    instanceTransforms.Push(*current->worldMatrix);
                    start->type = GEOM_INSTANCED;
                    start->instanceStart = instanceTransforms.Size() - 2;
                    start->instanceCount = 2;
                }
            }
            else
                start = (current->type == GEOM_STATIC) ? current : nullptr;
        }

        if (instanceTransforms.Size() != oldSize)
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

        graphics->SetVertexBuffer(1, instanceVertexBuffer);
        instanceTransformsDirty = false;
    }

    {
        PROFILE(RenderBatches);

        size_t passIndex = Material::PassIndex(pass);
        Vector<Batch>& batchQueue = batchQueues[passIndex];
        Pass* lastPass = nullptr;
        Material* lastMaterial = nullptr;

        for (auto it = batchQueue.Begin(); it != batchQueue.End();)
        {
            const Batch& batch = *it;
            bool instanced = batch.type == GEOM_INSTANCED;

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
                    vsVariations[vsIdx] = pass->shaders[SHADER_VS]->CreateVariation((pass->combinedShaderDefines[SHADER_VS] + " " +
                        geometryDefines[vsIdx]).Trimmed());
                }
                ShaderVariation* vs = vsVariations[vsIdx];

                // Get the pixel shader variation
                Vector<WeakPtr<ShaderVariation> >& psVariations = pass->shaderVariations[SHADER_PS];
                size_t psIdx = 0; /// \todo Handle light types
                if (psVariations.Size() <= psIdx)
                    psVariations.Resize(psIdx + 1);
                if (!psVariations[psIdx])
                    psVariations[psIdx] = pass->shaders[SHADER_PS]->CreateVariation(pass->combinedShaderDefines[SHADER_PS]);
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
                else if (!instanced)
                {
                    vsObjectConstantBuffer->SetConstant(VS_OBJECT_WORLD_MATRIX, *batch.worldMatrix);
                    vsObjectConstantBuffer->Apply();
                    graphics->SetConstantBuffer(SHADER_VS, CB_OBJECT, vsObjectConstantBuffer.Get());
                }
                graphics->SetConstantBuffer(SHADER_PS, CB_OBJECT, geometry->constantBuffers[SHADER_PS].Get());
            
                if (!instanced)
                {
                    if (ib)
                        graphics->DrawIndexed(geometry->primitiveType, geometry->drawStart, geometry->drawCount, 0);
                    else
                        graphics->Draw(geometry->primitiveType, geometry->drawStart, geometry->drawCount);
                }
                else
                {
                    if (ib)
                    {
                        graphics->DrawIndexedInstanced(geometry->primitiveType, geometry->drawStart, geometry->drawCount, 0,
                            batch.instanceStart, batch.instanceCount);
                    }
                    else
                    {
                        graphics->DrawInstanced(geometry->primitiveType, geometry->drawStart, geometry->drawCount, batch.instanceStart,
                            batch.instanceCount);
                    }
                }
            }

            // Advance. If batch is instanced, skip over the batches that were converted
            it += instanced ? batch.instanceCount : 1;
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

}
