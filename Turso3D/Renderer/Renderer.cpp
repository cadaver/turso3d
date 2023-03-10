// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/FrameBuffer.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/RenderBuffer.h"
#include "../Graphics/Shader.h"
#include "../Graphics/ShaderProgram.h"
#include "../Graphics/Texture.h"
#include "../Graphics/UniformBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../IO/Log.h"
#include "../Math/Random.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "AnimatedModel.h"
#include "Animation.h"
#include "Batch.h"
#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "Model.h"
#include "Octree.h"
#include "Renderer.h"
#include "StaticModel.h"

#include <glew.h>
#include <algorithm>
#include <cstring>
#include <tracy/Tracy.hpp>

static const size_t NODES_PER_BATCH_TASK = 128;

static const GLenum glCompareFuncs[] =
{
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS,
};

static const GLenum glSrcBlend[] =
{
    GL_ONE,
    GL_ONE,
    GL_DST_COLOR,
    GL_SRC_ALPHA,
    GL_SRC_ALPHA,
    GL_ONE,
    GL_ONE_MINUS_DST_ALPHA,
    GL_ONE,
    GL_SRC_ALPHA
};

static const unsigned glDestBlend[] =
{
    GL_ZERO,
    GL_ONE,
    GL_ZERO,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ONE,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE,
    GL_ONE
};

static const unsigned glBlendOp[] =
{
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT
};

inline bool CompareLights(Light* lhs, Light* rhs)
{
    return lhs->Distance() < rhs->Distance();
}

void ThreadOctantResult::Clear()
{
    octantListIt = octants.begin();
    nodeAcc = 0;
    batchTaskIdx = 0;
    lights.clear();

    for (auto it = octants.begin(); it != octants.end(); ++it)
        it->clear();
}

void ThreadBatchResult::Clear()
{
    minZ = M_MAX_FLOAT;
    maxZ = 0.0f;
    geometryBounds.Undefine();
    opaqueBatches.clear();
    alphaBatches.clear();
}

ShadowMap::ShadowMap()
{
    // Construct texture but do not define its size yet
    texture = new Texture();
    fbo = new FrameBuffer();
}

ShadowMap::~ShadowMap()
{
}

void ShadowMap::Clear()
{
    freeQueueIdx = 0;
    freeCasterListIdx = 0;
    allocator.Reset(texture->Width(), texture->Height(), 0, 0, false);
    shadowViews.clear();
    instanceTransforms.clear();

    for (auto it = shadowBatches.begin(); it != shadowBatches.end(); ++it)
        it->Clear();
    for (auto it = shadowCasters.begin(); it != shadowCasters.end(); ++it)
        it->clear();
}

Renderer::Renderer() :
    workQueue(Subsystem<WorkQueue>()),
    frameNumber(0),
    clusterFrustumsDirty(true),
    hasInstancing(false),
    lastPerMaterialUniforms(0),
    lastBlendMode(MAX_BLEND_MODES),
    lastCullMode(MAX_CULL_MODES),
    lastDepthTest(MAX_COMPARE_MODES),
    instancingEnabled(false),
    lastColorWrite(true),
    lastDepthWrite(true),
    lastDepthBias(false),
    depthBiasMul(1.0f),
    slopeScaleBiasMul(1.0f)
{
    assert(workQueue);
    assert(Object::Subsystem<Graphics>()->IsInitialized());

    RegisterSubsystem(this);
    RegisterRendererLibrary();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // Use texcoords 3-5 for instancing if supported
    if (glVertexAttribDivisorARB)
    {
        hasInstancing = true;

        glVertexAttribDivisorARB(7, 1);
        glVertexAttribDivisorARB(8, 1);
        glVertexAttribDivisorARB(9, 1);

        instanceVertexBuffer = new VertexBuffer();
        instanceVertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, 3));
        instanceVertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, 4));
        instanceVertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, 5));
    }

    DefineQuadVertexBuffer();

    clusterTexture = new Texture();
    clusterTexture->Define(TEX_3D, IntVector3(NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), FMT_RGBA32U, 1);
    clusterTexture->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

    clusterFrustums = new Frustum[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    clusterBoundingBoxes = new BoundingBox[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    clusterData = new unsigned char[MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    lightData = new LightData[MAX_LIGHTS + 1];

    perViewDataBuffer = new UniformBuffer();
    perViewDataBuffer->Define(USAGE_DYNAMIC, sizeof(PerViewUniforms));

    lightDataBuffer = new UniformBuffer();
    lightDataBuffer->Define(USAGE_DYNAMIC, MAX_LIGHTS * sizeof(LightData));

    octantResults.resize(NUM_OCTANTS + 1);
    batchResults.resize(workQueue->NumThreads());

    for (size_t i = 0; i < NUM_OCTANTS + 1; ++i)
        collectOctantsTasks[i] = new MemberFunctionTask<Renderer>(this, &Renderer::CollectOctantsWork, nullptr, (void*)i);

    for (size_t i = 0; i < NUM_CLUSTER_Z; ++i)
        cullLightsTasks[i] = new MemberFunctionTask<Renderer>(this, &Renderer::CullLightsToFrustumWork, (void*)i);

    processLightsTask = new MemberFunctionTask<Renderer>(this, &Renderer::ProcessLightsWork);
    processShadowCastersTask = new MemberFunctionTask<Renderer>(this, &Renderer::ProcessShadowCastersWork);
}

Renderer::~Renderer()
{
    RemoveSubsystem(this);
}

void Renderer::SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format)
{
    shadowMaps.resize(2);

    for (size_t i = 0; i < shadowMaps.size(); ++i)
    {
        ShadowMap& shadowMap = shadowMaps[i];

        shadowMap.texture->Define(TEX_2D, i == 0 ? IntVector2(dirLightSize * 2, dirLightSize) : IntVector2(lightAtlasSize, lightAtlasSize), format, 1);
        shadowMap.texture->DefineSampler(COMPARE_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP, 1);
        shadowMap.fbo->Define(nullptr, shadowMap.texture);
    }

    staticObjectShadowBuffer = new RenderBuffer();
    staticObjectShadowBuffer->Define(IntVector2(lightAtlasSize, lightAtlasSize), format, 1);
    staticObjectShadowFbo = new FrameBuffer();
    staticObjectShadowFbo->Define(nullptr, staticObjectShadowBuffer);

    DefineFaceSelectionTextures();

    shadowMapsDirty = true;
}

void Renderer::SetShadowDepthBiasMul(float depthBiasMul_, float slopeScaleBiasMul_)
{
    depthBiasMul = depthBiasMul_;
    slopeScaleBiasMul = slopeScaleBiasMul_;
    
    // Need to rerender all shadow maps with changed bias
    shadowMapsDirty = true;
}

void Renderer::PrepareView(Scene* scene_, Camera* camera_, bool drawShadows_)
{
    ZoneScoped;

    if (!scene_ || !camera_)
        return;

    scene = scene_;
    camera = camera_;
    octree = scene->FindChild<Octree>();
    if (!octree)
        return;

    // Framenumber is never 0
    ++frameNumber;
    if (!frameNumber)
        ++frameNumber;

    drawShadows = shadowMaps.size() ? drawShadows_ : false;
    frustum = camera->WorldFrustum();
    viewMask = camera->ViewMask();

    // Clear results from last frame
    dirLight = nullptr;
    lastCamera = nullptr;
    opaqueBatches.Clear();
    alphaBatches.Clear();
    lights.clear();
    instanceTransforms.clear();
    
    minZ = M_MAX_FLOAT;
    maxZ = 0.0f;
    geometryBounds.Undefine();

    for (size_t i = 0; i < octantResults.size(); ++i)
        octantResults[i].Clear();
    for (size_t i = 0; i < batchResults.size(); ++i)
        batchResults[i].Clear();
    for (auto it = shadowMaps.begin(); it != shadowMaps.end(); ++it)
        it->Clear();

    numPendingBatchTasks.store(NUM_OCTANT_TASKS); // For safely keeping track of both batch + octant task progress before main batches can be sorted
    numPendingShadowViews[0].store(0);
    numPendingShadowViews[1].store(0);

    // First process moved / animated objects' octree reinsertions
    octree->Update(frameNumber);

    // Enable threaded update during geometry / light gathering in case nodes' OnPrepareRender() causes further reinsertion queuing
    octree->SetThreadedUpdate(workQueue->NumThreads() > 1);

    // Find octants in view and their plane masks for node frustum culling. At the same time, find lights and process them
    // When octant collection tasks complete, they queue tasks for collecting batches from those octants.
    for (size_t i = 0; i < NUM_OCTANT_TASKS; ++i)
    {
        collectOctantsTasks[i]->start = !i ? octree->Root() : octree->Root()->children[i - 1];
        processLightsTask->AddDependency(collectOctantsTasks[i]);
    }

    // Ensure shadow view processing doesn't happen before lights have been found and processed
    processShadowCastersTask->AddDependency(processLightsTask);

    workQueue->QueueTasks(NUM_OCTANT_TASKS, reinterpret_cast<Task**>(&collectOctantsTasks[0]));

    // Execute tasks until can sort the main batches. Perform that in the main thread to potentially execute faster
    while (numPendingBatchTasks.load() > 0)
        workQueue->TryComplete();

    SortMainBatches();

    // Now finish all other threaded work
    workQueue->Complete();

    // No more threaded reinsertion will take place
    octree->SetThreadedUpdate(false);
}

void Renderer::RenderShadowMaps()
{
    ZoneScoped;

    Texture::Unbind(8);
    Texture::Unbind(9);

    for (size_t i = 0; i < shadowMaps.size(); ++i)
    {
        ShadowMap& shadowMap = shadowMaps[i];
        if (shadowMap.shadowViews.empty())
            continue;

        UpdateInstanceTransforms(shadowMap.instanceTransforms);

        shadowMap.fbo->Bind();

        // First render static objects for those shadowmaps that need to store static objects. Do all of them to avoid FBO changes
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            ShadowView* view = shadowMap.shadowViews[j];

            if (view->renderMode == RENDER_STATIC_LIGHT_STORE_STATIC)
            {
                BatchQueue& batchQueue = shadowMap.shadowBatches[view->staticQueueIdx];

                Clear(false, true, view->viewport);

                if (batchQueue.HasBatches())
                {
                    SetViewport(view->viewport);
                    SetDepthBias(view->light->DepthBias() * depthBiasMul, view->light->SlopeScaleBias() * slopeScaleBiasMul);
                    RenderBatches(view->shadowCamera, batchQueue);
                }
            }
        }

        // Now do the shadowmap -> static shadowmap storage blits as necessary
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            ShadowView* view = shadowMap.shadowViews[j];

            if (view->renderMode == RENDER_STATIC_LIGHT_STORE_STATIC)
                FrameBuffer::Blit(staticObjectShadowFbo, view->viewport, shadowMap.fbo, view->viewport, false, true, FILTER_POINT);
        }

        // Rebind shadowmap
        shadowMap.fbo->Bind();

        // First do all the clears or static shadowmap -> shadowmap blits
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            ShadowView* view = shadowMap.shadowViews[j];

            if (view->renderMode == RENDER_DYNAMIC_LIGHT)
                Clear(false, true, view->viewport);
            else if (view->renderMode == RENDER_STATIC_LIGHT_RESTORE_STATIC)
                FrameBuffer::Blit(shadowMap.fbo, view->viewport, staticObjectShadowFbo, view->viewport, false, true, FILTER_POINT);
        }

        // Finally render the dynamic objects
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            ShadowView* view = shadowMap.shadowViews[j];

            if (view->renderMode != RENDER_STATIC_LIGHT_CACHED)
            {
                BatchQueue& batchQueue = shadowMap.shadowBatches[view->dynamicQueueIdx];

                if (batchQueue.HasBatches())
                {
                    SetViewport(view->viewport);
                    SetDepthBias(view->light->DepthBias() * depthBiasMul, view->light->SlopeScaleBias() * slopeScaleBiasMul);
                    RenderBatches(view->shadowCamera, batchQueue);
                }
            }
        }
    }

    SetDepthBias(0.0f, 0.0f);
}

void Renderer::RenderOpaque()
{
    ZoneScoped;

    // Update main batches' instance transforms & light data
    UpdateInstanceTransforms(instanceTransforms);
    ImageLevel clusterLevel(IntVector3(NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), FMT_RG32U, clusterData.Get());
    clusterTexture->SetData(0, IntBox(0, 0, 0, NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), clusterLevel);
    lightDataBuffer->SetData(0, lights.size() * sizeof(LightData), lightData.Get());

    if (shadowMaps.size())
    {
        shadowMaps[0].texture->Bind(8);
        shadowMaps[1].texture->Bind(9);
        faceSelectionTexture1->Bind(10);
        faceSelectionTexture2->Bind(11);
    }

    clusterTexture->Bind(12);
    lightDataBuffer->Bind(UB_LIGHTDATA);

    RenderBatches(camera, opaqueBatches);
}

void Renderer::RenderAlpha()
{
    ZoneScoped;

    if (shadowMaps.size())
    {
        shadowMaps[0].texture->Bind(8);
        shadowMaps[1].texture->Bind(9);
        faceSelectionTexture1->Bind(10);
        faceSelectionTexture2->Bind(11);
    }

    clusterTexture->Bind(12);
    lightDataBuffer->Bind(UB_LIGHTDATA);

    RenderBatches(camera, alphaBatches);
}

void Renderer::SetRenderState(BlendMode blendMode, CullMode cullMode, CompareMode depthTest, bool colorWrite, bool depthWrite)
{
    if (blendMode != lastBlendMode)
    {
        if (blendMode == BLEND_REPLACE)
            glDisable(GL_BLEND);
        else
        {
            glEnable(GL_BLEND);
            glBlendFunc(glSrcBlend[blendMode], glDestBlend[blendMode]);
            glBlendEquation(glBlendOp[blendMode]);
        }

        lastBlendMode = blendMode;
    }

    if (cullMode != lastCullMode)
    {
        if (cullMode == CULL_NONE)
            glDisable(GL_CULL_FACE);
        else
        {
            // Use Direct3D convention, ie. clockwise vertices define a front face
            glEnable(GL_CULL_FACE);
            glCullFace(cullMode == CULL_BACK ? GL_FRONT : GL_BACK);
        }

        lastCullMode = cullMode;
    }

    if (depthTest != lastDepthTest)
    {
        glDepthFunc(glCompareFuncs[depthTest]);
        lastDepthTest = depthTest;
    }

    if (colorWrite != lastColorWrite)
    {
        GLboolean newColorWrite = colorWrite ? GL_TRUE : GL_FALSE;
        glColorMask(newColorWrite, newColorWrite, newColorWrite, newColorWrite);
        lastColorWrite = colorWrite;
    }

    if (depthWrite != lastDepthWrite)
    {
        GLboolean newDepthWrite = depthWrite ? GL_TRUE : GL_FALSE;
        glDepthMask(newDepthWrite);
        lastDepthWrite = depthWrite;
    }
}

void Renderer::SetDepthBias(float constantBias, float slopeScaleBias)
{
    if (constantBias <= 0.0f && slopeScaleBias <= 0.0f)
    {
        if (lastDepthBias)
        {
            glDisable(GL_POLYGON_OFFSET_FILL);
            lastDepthBias = false;
        }
    }
    else
    {
        if (!lastDepthBias)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            lastDepthBias = true;
        }

        glPolygonOffset(slopeScaleBias, constantBias);
    }
}

void Renderer::SetUniform(ShaderProgram* program, const char* name, float value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform1f(location, value);
    }
}

void Renderer::SetUniform(ShaderProgram* program, const char* name, const Vector2& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform2fv(location, 1, value.Data());
    }
}

void Renderer::SetUniform(ShaderProgram* program, const char* name, const Vector3& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform3fv(location, 1, value.Data());
    }
}

void Renderer::SetUniform(ShaderProgram* program, const char* name, const Vector4& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform4fv(location, 1, value.Data());
    }
}

void Renderer::SetUniform(ShaderProgram* program, const char* name, const Matrix3x4& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniformMatrix3x4fv(location, 1, GL_FALSE, value.Data());
    }
}

void Renderer::SetUniform(ShaderProgram* program, const char* name, const Matrix4& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniformMatrix4fv(location, 1, GL_FALSE, value.Data());
    }
}

void Renderer::SetViewport(const IntRect& viewRect)
{
    glViewport(viewRect.left, viewRect.top, viewRect.right - viewRect.left, viewRect.bottom - viewRect.top);
}

ShaderProgram* Renderer::SetProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    Shader* shader = cache->LoadResource<Shader>(shaderName);
    if (!shader)
        return nullptr;

    ShaderProgram* program = shader->CreateProgram(vsDefines, fsDefines);
    return program->Bind() ? program : nullptr;
}

void Renderer::Clear(bool clearColor, bool clearDepth, const IntRect& clearRect, const Color& backgroundColor)
{
    if (clearColor)
    {
        glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        lastColorWrite = true;
    }
    if (clearDepth)
    {
        glDepthMask(GL_TRUE);
        lastDepthWrite = true;
    }

    GLenum glClearBits = 0;
    if (clearColor)
        glClearBits |= GL_COLOR_BUFFER_BIT;
    if (clearDepth)
        glClearBits |= GL_DEPTH_BUFFER_BIT;

    if (clearRect == IntRect::ZERO)
        glClear(glClearBits);
    else
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(clearRect.left, clearRect.top, clearRect.right - clearRect.left, clearRect.bottom - clearRect.top);
        glClear(glClearBits);
        glDisable(GL_SCISSOR_TEST);
    }
}

void Renderer::DrawQuad()
{
    quadVertexBuffer->Bind(0x11);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

Texture* Renderer::ShadowMapTexture(size_t index) const
{
    return index < shadowMaps.size() ? shadowMaps[index].texture : nullptr;
}

void Renderer::CollectOctantsAndLights(Octant* octant, ThreadOctantResult& result, bool threaded, bool recursive, unsigned char planeMask)
{
    if (planeMask)
    {
        planeMask = frustum.IsInsideMasked(octant->cullingBox, planeMask);
        if (planeMask == 0xff)
            return;
    }

    for (auto it = octant->nodes.begin(); it != octant->nodes.end(); ++it)
    {
        OctreeNode* node = *it;
        unsigned short flags = node->Flags();
        if (flags & NF_LIGHT)
        {
            if ((node->LayerMask() & viewMask) && (!planeMask || frustum.IsInsideMaskedFast(node->WorldBoundingBox(), planeMask)))
            {
                if (node->OnPrepareRender(frameNumber, camera))
                {
                    Light* light = static_cast<Light*>(node);
                    result.lights.push_back(light);
                }
            }
        }
        // Lights are sorted first in octants, so break when first geometry encountered. Store the octant for batch collecting
        else
        {
            if (result.octantListIt == result.octants.end())
            {
                result.octants.resize(result.octants.size() + 1);
                result.octantListIt = --result.octants.end();
            }

            result.octantListIt->push_back(std::make_pair(octant, planeMask));
            result.nodeAcc += octant->nodes.size();
            break;
        }
    }

    // Setup and queue batches collection task if over the node limit now. Note: if not threaded, defer to the end
    if (threaded && result.nodeAcc >= NODES_PER_BATCH_TASK)
    {
        if (result.collectBatchesTasks.size() <= result.batchTaskIdx)
            result.collectBatchesTasks.push_back(new MemberFunctionTask<Renderer>(this, &Renderer::CollectBatchesWork));

        Task* batchTask = result.collectBatchesTasks[result.batchTaskIdx];
        batchTask->start = &*(result.octantListIt);
        processShadowCastersTask->AddDependency(batchTask);
        numPendingBatchTasks.fetch_add(1);
        workQueue->QueueTask(batchTask);

        result.nodeAcc = 0;
        ++result.batchTaskIdx;
        ++result.octantListIt;
    }

    if (recursive)
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectOctantsAndLights(octant->children[i], result, threaded, true, planeMask);
        }
    }
}

bool Renderer::AllocateShadowMap(Light* light)
{
    size_t index = light->GetLightType() == LIGHT_DIRECTIONAL ? 0 : 1;
    ShadowMap& shadowMap = shadowMaps[index];

    IntVector2 request = light->TotalShadowMapSize();

    // If light already has its preferred shadow rect from the previous frame, try to reallocate it for shadow map caching
    IntRect oldRect = light->ShadowRect();
    if (request.x == oldRect.Width() && request.y == oldRect.Height())
    {
        if (shadowMap.allocator.AllocateSpecific(oldRect))
        {
            light->SetShadowMap(shadowMaps[index].texture, light->ShadowRect());
            return true;
        }
    }

    IntRect shadowRect;
    size_t retries = 3;

    while (retries--)
    {
        int x, y;
        if (shadowMap.allocator.Allocate(request.x, request.y, x, y))
        {
            light->SetShadowMap(shadowMaps[index].texture, IntRect(x, y, x + request.x, y + request.y));
            return true;
        }

        request.x /= 2;
        request.y /= 2;
    }

    // No room in atlas
    light->SetShadowMap(nullptr);
    return false;
}

void Renderer::SortMainBatches()
{
    ZoneScoped;

    for (auto it = batchResults.begin(); it != batchResults.end(); ++it)
    {
        if (it->opaqueBatches.size())
            opaqueBatches.batches.insert(opaqueBatches.batches.end(), it->opaqueBatches.begin(), it->opaqueBatches.end());
        if (it->alphaBatches.size())
            alphaBatches.batches.insert(alphaBatches.batches.end(), it->alphaBatches.begin(), it->alphaBatches.end());
    }

    opaqueBatches.Sort(instanceTransforms, SORT_STATE_AND_DISTANCE, hasInstancing);
    alphaBatches.Sort(instanceTransforms, SORT_DISTANCE, hasInstancing);
}

void Renderer::SortShadowBatches(ShadowMap& shadowMap)
{
    ZoneScoped;

    for (size_t i = 0; i < shadowMap.shadowViews.size(); ++i)
    {
        ShadowView& view = *shadowMap.shadowViews[i];

        Light* light = view.light;

        // Check if view was discarded during shadowcaster collecting
        if (!light)
            continue;

        BatchQueue* destStatic = (view.renderMode == RENDER_STATIC_LIGHT_STORE_STATIC) ? &shadowMap.shadowBatches[view.staticQueueIdx] : nullptr;
        BatchQueue* destDynamic = &shadowMap.shadowBatches[view.dynamicQueueIdx];

        if (destStatic && destStatic->HasBatches())
            destStatic->Sort(shadowMap.instanceTransforms, SORT_STATE, hasInstancing);

        if (destDynamic->HasBatches())
            destDynamic->Sort(shadowMap.instanceTransforms, SORT_STATE, hasInstancing);
    }
}

void Renderer::UpdateInstanceTransforms(const std::vector<Matrix3x4>& transforms)
{
    if (hasInstancing && transforms.size())
    {
        if (instanceVertexBuffer->NumVertices() < transforms.size())
            instanceVertexBuffer->Define(USAGE_DYNAMIC, transforms.size(), instanceVertexElements, &transforms[0]);
        else
            instanceVertexBuffer->SetData(0, transforms.size(), &transforms[0]);
    }
}

void Renderer::RenderBatches(Camera* camera_, const BatchQueue& queue)
{
    ZoneScoped;

    lastMaterial = nullptr;
    lastPass = nullptr;

    if (camera_ != lastCamera)
    {
        perViewData.projectionMatrix = camera_->ProjectionMatrix();
        perViewData.viewMatrix = camera_->ViewMatrix();
        perViewData.viewProjMatrix = perViewData.projectionMatrix * perViewData.viewMatrix;
        perViewData.depthParameters = Vector4(camera_->NearClip(), camera_->FarClip(), camera_->IsOrthographic() ? 0.5f : 0.0f, camera_->IsOrthographic() ? 0.5f : 1.0f / camera_->FarClip());

        size_t dataSize = sizeof(Matrix3x4) + 2 * sizeof(Matrix4) + 5 * sizeof(Vector4);

        // Set the dir light parameters only in the main view
        if (!dirLight || camera_ != camera)
        {
            perViewData.dirLightData[0] = Vector4::ZERO;
            perViewData.dirLightData[1] = Vector4::ZERO;
            perViewData.dirLightData[3] = Vector4::ONE;
        }
        else
        {
            perViewData.dirLightData[0] = Vector4(-dirLight->WorldDirection(), 0.0f);
            perViewData.dirLightData[1] = dirLight->GetColor().Data();

            if (dirLight->ShadowMap())
            {
                Vector2 cascadeSplits = dirLight->ShadowCascadeSplits();
                float farClip = camera->FarClip();
                float firstSplit = cascadeSplits.x / farClip;
                float secondSplit = cascadeSplits.y / farClip;

                perViewData.dirLightData[2] = Vector4(firstSplit, secondSplit, dirLight->ShadowFadeStart() * secondSplit, 1.0f / (secondSplit - dirLight->ShadowFadeStart() * secondSplit));
                perViewData.dirLightData[3] = dirLight->ShadowParameters();
                if (dirLight->ShadowViews().size() >= 2)
                {
                    *reinterpret_cast<Matrix4*>(&perViewData.dirLightData[4]) = dirLight->ShadowViews()[0].shadowMatrix;
                    *reinterpret_cast<Matrix4*>(&perViewData.dirLightData[8]) = dirLight->ShadowViews()[1].shadowMatrix;
                    dataSize += 8 * sizeof(Vector4);
                }
            }
            else
                perViewData.dirLightData[3] = Vector4::ONE;
        }

        perViewDataBuffer->SetData(0, dataSize, &perViewData);

        lastCamera = camera_;
    }

    perViewDataBuffer->Bind(UB_PERVIEWDATA);

    for (auto it = queue.batches.begin(); it != queue.batches.end(); ++it)
    {
        const Batch& batch = *it;
        unsigned char geometryBits = batch.programBits & SP_GEOMETRYBITS;

        ShaderProgram* program = batch.pass->GetShaderProgram(batch.programBits);
        if (!program->Bind())
            continue;

        Material* material = batch.pass->Parent();
        if (batch.pass != lastPass)
        {
            if (material != lastMaterial)
            {
                for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i)
                {
                    Texture* texture = material->GetTexture(i);
                    if (texture)
                        texture->Bind(i);
                }

                lastMaterial = material;
                ++lastPerMaterialUniforms;
                if (!lastPerMaterialUniforms)
                    ++lastPerMaterialUniforms;
            }

            CullMode cullMode = material->GetCullMode();
            if (camera_->UseReverseCulling())
            {
                if (cullMode == CULL_BACK)
                    cullMode = CULL_FRONT;
                else if (cullMode == CULL_FRONT)
                    cullMode = CULL_BACK;
            }

            SetRenderState(batch.pass->GetBlendMode(), cullMode, batch.pass->GetDepthTest(), batch.pass->GetColorWrite(), batch.pass->GetDepthWrite());

            lastPass = batch.pass;
        }

        if (program->lastPerMaterialUniforms != lastPerMaterialUniforms)
        {
            const std::map<PresetUniform, Vector4>& uniformValues = material->UniformValues();
            for (auto uIt = uniformValues.begin(); uIt != uniformValues.end(); ++uIt)
            {
                int location = program->Uniform(uIt->first);
                if (location >= 0)
                    glUniform4fv(location, 1, uIt->second.Data());
            }

            program->lastPerMaterialUniforms = lastPerMaterialUniforms;
        }

        Geometry* geometry = batch.geometry;

        if (geometryBits == GEOM_INSTANCED)
        {
            if (!instancingEnabled)
            {
                glEnableVertexAttribArray(7);
                glEnableVertexAttribArray(8);
                glEnableVertexAttribArray(9);
                instancingEnabled = true;
            }

            const size_t instanceVertexSize = sizeof(Matrix3x4);
            
            instanceVertexBuffer->Bind(0);
            glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(batch.instanceStart * instanceVertexSize));
            glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(batch.instanceStart * instanceVertexSize + sizeof(Vector4)));
            glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(batch.instanceStart * instanceVertexSize + 2 * sizeof(Vector4)));

            VertexBuffer* vb = geometry->vertexBuffer;
            IndexBuffer* ib = geometry->indexBuffer;
            vb->Bind(program->Attributes());
            if (ib)
                ib->Bind();

            glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)geometry->drawCount, ib->IndexSize() == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, 
                (const void*)(geometry->drawStart* ib->IndexSize()), batch.instanceCount);

            it += batch.instanceCount - 1;
        }
        else
        {
            if (instancingEnabled)
            {
                glDisableVertexAttribArray(7);
                glDisableVertexAttribArray(8);
                glDisableVertexAttribArray(9);
                instancingEnabled = false;
            }
            
            if (!geometryBits)
                glUniformMatrix3x4fv(program->Uniform(U_WORLDMATRIX), 1, GL_FALSE, batch.worldTransform->Data());
            else
                batch.node->OnRender(batch.geomIndex, program);

            VertexBuffer* vb = geometry->vertexBuffer;
            IndexBuffer* ib = geometry->indexBuffer;
            vb->Bind(program->Attributes());
            if (ib)
                ib->Bind();

            if (!ib)
                glDrawArrays(GL_TRIANGLES, (GLsizei)geometry->drawStart, (GLsizei)geometry->drawCount);
            else
                glDrawElements(GL_TRIANGLES, (GLsizei)geometry->drawCount, ib->IndexSize() == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, 
                    (const void*)(geometry->drawStart * ib->IndexSize()));
        }
    }
}

void Renderer::DefineFaceSelectionTextures()
{
    if (faceSelectionTexture1 && faceSelectionTexture2)
        return;

    faceSelectionTexture1 = new Texture();
    faceSelectionTexture2 = new Texture();

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

    std::vector<ImageLevel> faces1;
    std::vector<ImageLevel> faces2;

    for (size_t i = 0; i < MAX_CUBE_FACES; ++i)
    {
        faces1.push_back(ImageLevel(IntVector2(1, 1), FMT_RGBA32F, &faceSelectionData1[4 * i]));
        faces2.push_back(ImageLevel(IntVector2(1, 1), FMT_RGBA32F, &faceSelectionData2[4 * i]));
    }

    faceSelectionTexture1->Define(TEX_CUBE, IntVector3(1, 1, MAX_CUBE_FACES), FMT_RGBA32F, 1, 1, &faces1[0]);
    faceSelectionTexture1->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

    faceSelectionTexture2->Define(TEX_CUBE, IntVector3(1, 1, MAX_CUBE_FACES), FMT_RGBA32F, 1, 1, &faces2[0]);
    faceSelectionTexture2->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
}

void Renderer::DefineQuadVertexBuffer()
{
    quadVertexBuffer = new VertexBuffer();

    float quadVertexData[] = {
        // Position         // UV
        -1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f
    };

    std::vector<VertexElement> vertexDeclaration;
    vertexDeclaration.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
    vertexDeclaration.push_back(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
    quadVertexBuffer = new VertexBuffer();
    quadVertexBuffer->Define(USAGE_DEFAULT, 6, vertexDeclaration, quadVertexData);
}

void Renderer::DefineClusterFrustums()
{
    Matrix4 cameraProj = camera->ProjectionMatrix(false);
    if (lastClusterFrustumProj != cameraProj)
        clusterFrustumsDirty = true;

    if (clusterFrustumsDirty)
    {
        ZoneScoped;

        Matrix4 cameraProjInverse = cameraProj.Inverse();
        float cameraNearClip = camera->NearClip();
        float cameraFarClip = camera->FarClip();
        size_t idx = 0;

        float xStep = 2.0f / NUM_CLUSTER_X;
        float yStep = 2.0f / NUM_CLUSTER_Y;
        float zStep = 1.0f / NUM_CLUSTER_Z;

        for (size_t z = 0; z < NUM_CLUSTER_Z; ++z)
        {
            Vector4 nearVec = cameraProj * Vector4(0.0f, 0.0f, z > 0 ? powf(z * zStep, 2.0f) * cameraFarClip : cameraNearClip, 1.0f);
            Vector4 farVec = cameraProj * Vector4(0.0f, 0.0f, powf((z + 1) * zStep, 2.0f) * cameraFarClip, 1.0f);
            float near = nearVec.z / nearVec.w;
            float far = farVec.z / farVec.w;

            for (size_t y = 0; y < NUM_CLUSTER_Y; ++y)
            {
                for (size_t x = 0; x < NUM_CLUSTER_X; ++x)
                {
                    clusterFrustums[idx].vertices[0] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * y, near);
                    clusterFrustums[idx].vertices[1] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * (y + 1), near);
                    clusterFrustums[idx].vertices[2] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * (y + 1), near);
                    clusterFrustums[idx].vertices[3] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * y, near);
                    clusterFrustums[idx].vertices[4] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * y, far);
                    clusterFrustums[idx].vertices[5] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * (y + 1), far);
                    clusterFrustums[idx].vertices[6] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * (y + 1), far);
                    clusterFrustums[idx].vertices[7] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * y, far);
                    clusterFrustums[idx].UpdatePlanes();
                    clusterBoundingBoxes[idx].Define(clusterFrustums[idx]);
                    ++idx;
                }
            }
        }

        lastClusterFrustumProj = cameraProj;
        clusterFrustumsDirty = false;
    }
}

void Renderer::CollectOctantsWork(Task* task, unsigned)
{
    ZoneScoped;

    Octant* octant = reinterpret_cast<Octant*>(task->start);
    if (octant)
    {
        // Go through octants in this task's octree branch
        ThreadOctantResult& result = octantResults[(size_t)task->end];
        CollectOctantsAndLights(octant, result, workQueue->NumThreads() > 1, octant != octree->Root());

        // Queue final batch task for leftover nodes if needed
        if (result.nodeAcc)
        {
            if (result.collectBatchesTasks.size() <= result.batchTaskIdx)
                result.collectBatchesTasks.push_back(new MemberFunctionTask<Renderer>(this, &Renderer::CollectBatchesWork));
            
            Task* batchTask = result.collectBatchesTasks[result.batchTaskIdx];
            batchTask->start = &*(result.octantListIt);
            processShadowCastersTask->AddDependency(batchTask);
            numPendingBatchTasks.fetch_add(1);
            workQueue->QueueTask(batchTask);
        }
    }
    
    numPendingBatchTasks.fetch_add(-1);
}

void Renderer::ProcessLightsWork(Task*, unsigned)
{
    ZoneScoped;

    // Merge the light collection results
    for (auto it = octantResults.begin(); it != octantResults.end(); ++it)
        lights.insert(lights.end(), it->lights.begin(), it->lights.end());

    // Find the directional light if any
    for (auto it = lights.begin(); it != lights.end(); )
    {
        Light* light = *it;
        if (light->GetLightType() == LIGHT_DIRECTIONAL)
        {
            if (!dirLight || light->GetColor().Average() > dirLight->GetColor().Average())
                dirLight = light;
            it = lights.erase(it);
        }
        else
            ++it;
    }

    // Sort localized lights by increasing distance
    std::sort(lights.begin(), lights.end(), CompareLights);

    // Clamp to maximum supported
    if (lights.size() > MAX_LIGHTS)
        lights.resize(MAX_LIGHTS);

    // Pre-step for shadow map caching: reallocate all lights' shadow map rectangles which are non-zero at this point.
    // If shadow maps were dirtied (size or bias change) reset all allocations instead
    for (auto it = lights.begin(); it != lights.end(); ++it)
    {
        Light* light = *it;
        if (shadowMapsDirty)
            light->SetShadowMap(nullptr);
        else if (drawShadows && light->ShadowStrength() < 1.0f && light->ShadowRect() != IntRect::ZERO)
            AllocateShadowMap(light);
    }

    // Check if directional light needs shadows
    if (dirLight)
    {
        if (shadowMapsDirty)
            dirLight->SetShadowMap(nullptr);

        if (!drawShadows || dirLight->ShadowStrength() >= 1.0f || !AllocateShadowMap(dirLight))
            dirLight->SetShadowMap(nullptr);
    }

    shadowMapsDirty = false;

    size_t lightTaskIdx = 0;

    // Go through lights and setup shadowcaster collection tasks
    for (size_t i = 0; i < lights.size(); ++i)
    {
        ShadowMap& shadowMap = shadowMaps[1];

        Light* light = lights[i];
        float cutoff = light->GetLightType() == LIGHT_SPOT ? cosf(light->Fov() * 0.5f * M_DEGTORAD) : 0.0f;

        lightData[i].position = Vector4(light->WorldPosition(), 1.0f);
        lightData[i].direction = Vector4(-light->WorldDirection(), 0.0f);
        lightData[i].attenuation = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f / (1.0f - cutoff), 1.0f);
        lightData[i].color = light->EffectiveColor();
        lightData[i].shadowParameters = Vector4::ONE; // Assume unshadowed

        // Check if not shadowcasting or beyond shadow range
        if (!drawShadows || light->ShadowStrength() >= 1.0f)
        {
            light->SetShadowMap(nullptr);
            continue;
        }

        // Now retry shadow map allocation if necessary. If it's a new allocation, must rerender the shadow map
        if (!light->ShadowMap())
        {
            if (!AllocateShadowMap(light))
                continue;
        }

        light->InitShadowViews();
        std::vector<ShadowView>& shadowViews = light->ShadowViews();

        // Preallocate shadowcaster list
        size_t casterListIdx = shadowMap.freeCasterListIdx++;
        if (shadowMap.shadowCasters.size() < shadowMap.freeCasterListIdx)
            shadowMap.shadowCasters.resize(shadowMap.freeCasterListIdx);

        for (size_t j = 0; j < shadowViews.size(); ++j)
        {
            ShadowView& view = shadowViews[j];

            // Preallocate shadow batch queues
            view.casterListIdx = casterListIdx;

            if (light->IsStatic())
            {
                view.staticQueueIdx = shadowMap.freeQueueIdx++;
                view.dynamicQueueIdx = shadowMap.freeQueueIdx++;
            }
            else
                view.dynamicQueueIdx = shadowMap.freeQueueIdx++;

            if (shadowMap.shadowBatches.size() < shadowMap.freeQueueIdx)
                shadowMap.shadowBatches.resize(shadowMap.freeQueueIdx);

            shadowMap.shadowViews.push_back(&view);
        }

        if (collectShadowCastersTasks.size() <= lightTaskIdx)
            collectShadowCastersTasks.push_back(new MemberFunctionTask<Renderer>(this, &Renderer::CollectShadowCastersWork));

        collectShadowCastersTasks[lightTaskIdx]->start = light;
        processShadowCastersTask->AddDependency(collectShadowCastersTasks[lightTaskIdx]);
        ++lightTaskIdx;
    }

    if (dirLight && dirLight->ShadowMap())
    {
        ShadowMap& shadowMap = shadowMaps[0];

        dirLight->InitShadowViews();
        std::vector<ShadowView>& shadowViews = dirLight->ShadowViews();

        for (size_t i = 0; i < shadowViews.size(); ++i)
        {
            ShadowView& view = shadowViews[i];

            // Directional light needs a new frustum query for each split, as the shadow cameras are typically far outside the main view
            // But queries are only performed later when the shadow map can be focused to visible scene
            view.casterListIdx = shadowMap.freeCasterListIdx++;
            if (shadowMap.shadowCasters.size() < shadowMap.freeCasterListIdx)
                shadowMap.shadowCasters.resize(shadowMap.freeCasterListIdx);

            view.dynamicQueueIdx = shadowMap.freeQueueIdx++;
            if (shadowMap.shadowBatches.size() < shadowMap.freeQueueIdx)
                shadowMap.shadowBatches.resize(shadowMap.freeQueueIdx);

            shadowMap.shadowViews.push_back(&view);
        }
    }

    // Now queue all shadowcaster collection tasks
    if (lightTaskIdx > 0)
        workQueue->QueueTasks(lightTaskIdx, reinterpret_cast<Task**>(&collectShadowCastersTasks[0]));
}

void Renderer::CollectBatchesWork(Task* task, unsigned threadIndex)
{
    ZoneScoped;

    ThreadBatchResult& result = batchResults[threadIndex];
    bool threaded = workQueue->NumThreads() > 1;

    std::vector<std::pair<Octant*, unsigned char> >& octants = *reinterpret_cast<std::vector<std::pair<Octant*, unsigned char> >*>(task->start);
    std::vector<Batch>& opaqueQueue = threaded ? result.opaqueBatches : opaqueBatches.batches;
    std::vector<Batch>& alphaQueue = threaded ? result.alphaBatches : alphaBatches.batches;

    const Matrix3x4& viewMatrix = camera->ViewMatrix();
    Vector3 viewZ = Vector3(viewMatrix.m20, viewMatrix.m21, viewMatrix.m22);
    Vector3 absViewZ = viewZ.Abs();
    float farClipMul = 32767.0f / camera->FarClip();

    // Scan octants for geometries
    for (auto it = octants.begin(); it != octants.end(); ++it)
    {
        Octant* octant = it->first;
        unsigned char planeMask = it->second;
        std::vector<OctreeNode*>& nodes = octant->nodes;

        for (auto nIt = nodes.begin(); nIt != nodes.end(); ++nIt)
        {
            OctreeNode* node = *nIt;
            unsigned short flags = node->Flags();

            if ((flags & NF_GEOMETRY) && (node->LayerMask() & viewMask) && (!planeMask || frustum.IsInsideMaskedFast(node->WorldBoundingBox(), planeMask)))
            {
                if (node->OnPrepareRender(frameNumber, camera))
                {
                    const BoundingBox& geometryBox = node->WorldBoundingBox();
                    result.geometryBounds.Merge(geometryBox);

                    Vector3 center = geometryBox.Center();
                    Vector3 edge = geometryBox.Size() * 0.5f;

                    float viewCenterZ = viewZ.DotProduct(center) + viewMatrix.m23;
                    float viewEdgeZ = absViewZ.DotProduct(edge);
                    result.minZ = Min(result.minZ, viewCenterZ - viewEdgeZ);
                    result.maxZ = Max(result.maxZ, viewCenterZ + viewEdgeZ);
 
                    GeometryNode* geometryNode = static_cast<GeometryNode*>(node);

                    Batch newBatch;

                    unsigned short distance = (unsigned short)(geometryNode->Distance() * farClipMul);
                    const SourceBatches& batches = geometryNode->Batches();
                    size_t numGeometries = batches.NumGeometries();
        
                    for (size_t j = 0; j < numGeometries; ++j)
                    {
                        Material* material = batches.GetMaterial(j);

                        // Assume opaque first
                        newBatch.pass = material->GetPass(PASS_OPAQUE);
                        newBatch.geometry = batches.GetGeometry(j);
                        newBatch.programBits = (unsigned char)geometryNode->GetGeometryType();
                        newBatch.geomIndex = (unsigned char)j;

                        if (!newBatch.programBits)
                            newBatch.worldTransform = &node->WorldTransform();
                        else
                            newBatch.node = geometryNode;

                        if (newBatch.pass)
                        {
                            // Perform distance sort in addition to state sort
                            if (newBatch.pass->lastSortKey.first != frameNumber || newBatch.pass->lastSortKey.second > distance)
                            {
                                newBatch.pass->lastSortKey.first = frameNumber;
                                newBatch.pass->lastSortKey.second = distance;
                            }
                            if (newBatch.geometry->lastSortKey.first != frameNumber || newBatch.geometry->lastSortKey.second > distance + (unsigned short)j)
                            {
                                newBatch.geometry->lastSortKey.first = frameNumber;
                                newBatch.geometry->lastSortKey.second = distance + (unsigned short)j;
                            }

                            opaqueQueue.push_back(newBatch);
                        }
                        else
                        {
                            // If not opaque, try transparent
                            newBatch.pass = material->GetPass(PASS_ALPHA);
                            if (!newBatch.pass)
                                continue;

                            newBatch.distance = node->Distance();
                            alphaQueue.push_back(newBatch);
                        }
                    }
                }
            }
        }
    }

    numPendingBatchTasks.fetch_add(-1);
}

void Renderer::CollectShadowCastersWork(Task* task, unsigned)
{
    ZoneScoped;

    Light* light = (Light*)task->start;
    LightType lightType = light->GetLightType();
    std::vector<ShadowView>& shadowViews = light->ShadowViews();

    // Directional lights perform queries later, here only point & spot lights (in shadow atlas) are considered
    ShadowMap& shadowMap = shadowMaps[1];

    if (lightType == LIGHT_POINT)
    {
        // Point light: perform only one sphere query, then check which of the point light sides are visible
        for (size_t i = 0; i < shadowViews.size(); ++i)
        {
            // Check if each of the sides is in view. Do not process if isn't. Rendering will be no-op this frame, but cached contents are discarded once comes into view again
            light->SetupShadowView(i, camera);
            ShadowView& view = shadowViews[i];

            if (!frustum.IsInsideFast(BoundingBox(view.shadowFrustum)))
            {
                view.renderMode = RENDER_STATIC_LIGHT_CACHED;
                view.viewport = IntRect::ZERO;
                view.lastViewport = IntRect::ZERO;
            }
        }

        std::vector<GeometryNode*>& shadowCasters = shadowMap.shadowCasters[shadowViews[0].casterListIdx];
        octree->FindNodes(reinterpret_cast<std::vector<OctreeNode*>&>(shadowCasters), light->WorldSphere(), NF_GEOMETRY | NF_CAST_SHADOWS);
    }
    else if (lightType == LIGHT_SPOT)
    {
        // Spot light: perform query for the spot frustum
        light->SetupShadowView(0, camera);
        ShadowView& view = shadowViews[0];

        std::vector<GeometryNode*>& shadowCasters = shadowMap.shadowCasters[view.casterListIdx];
        octree->FindNodesMasked(reinterpret_cast<std::vector<OctreeNode*>&>(shadowCasters), view.shadowFrustum, NF_GEOMETRY | NF_CAST_SHADOWS);
    }
}

void Renderer::ProcessShadowCastersWork(Task*, unsigned)
{
    ZoneScoped;

    // Shadow batches collection needs accurate scene min / max Z results, combine them from per-thread data
    for (auto it = batchResults.begin(); it != batchResults.end(); ++it)
    {
        minZ = Min(minZ, it->minZ);
        maxZ = Max(maxZ, it->maxZ);
        if (it->geometryBounds.IsDefined())
            geometryBounds.Merge(it->geometryBounds);
    }

    minZ = Max(minZ, camera->NearClip());

    // Clear per-cluster light data from previous frame, update cluster frustums and bounding boxes if camera changed, then queue light culling tasks for the needed scene range
    DefineClusterFrustums();
    memset(numClusterLights, 0, sizeof numClusterLights);
    memset(clusterData.Get(), 0, MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z);
    for (size_t z = 0; z < NUM_CLUSTER_Z; ++z)
    {
        size_t idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
        if (minZ > clusterFrustums[idx].vertices[4].z || maxZ < clusterFrustums[idx].vertices[0].z)
            continue;
        workQueue->QueueTask(cullLightsTasks[z]);
    }

    // Queue shadow batch collection tasks. These will also perform shadow batch sorting tasks when done
    size_t shadowTaskIdx = 0;
    Light* lastLight = nullptr;

    for (size_t i = 0; i < shadowMaps.size(); ++i)
    {
        ShadowMap& shadowMap = shadowMaps[i];
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            Light* light = shadowMap.shadowViews[j]->light;
            // For a point light, make only one task that will handle all of the views and skip rest
            if (light->GetLightType() == LIGHT_POINT && light == lastLight)
                continue;

            lastLight = light;

            if (collectShadowBatchesTasks.size() <= shadowTaskIdx)
                collectShadowBatchesTasks.push_back(new MemberFunctionTask<Renderer>(this, &Renderer::CollectShadowBatchesWork));
            collectShadowBatchesTasks[shadowTaskIdx]->start = (void*)i;
            collectShadowBatchesTasks[shadowTaskIdx]->end = (void*)j;
            numPendingShadowViews[i].fetch_add(1);
            ++shadowTaskIdx;
        }
    }

    if (shadowTaskIdx > 0)
        workQueue->QueueTasks(shadowTaskIdx, reinterpret_cast<Task**>(&collectShadowBatchesTasks[0]));

    // Finally copy correct shadow matrices for the light data now that shadowcaster collection has finalized them
    for (size_t i = 0; i < lights.size(); ++i)
    {
        Light* light = lights[i];

        if (light->ShadowMap())
        {
            lightData[i].shadowParameters = light->ShadowParameters();
            lightData[i].shadowMatrix = light->ShadowViews()[0].shadowMatrix;
        }
    }
}

void Renderer::CollectShadowBatchesWork(Task* task, unsigned)
{
    ZoneScoped;

    size_t shadowMapIdx = (size_t)task->start;
    size_t shadowViewIdx = (size_t)task->end;
    ShadowMap& shadowMap = shadowMaps[shadowMapIdx];

    for (;;)
    {
        ShadowView& view = *shadowMap.shadowViews[shadowViewIdx];

        Light* light = view.light;
        LightType lightType = light->GetLightType();

        float splitMinZ = minZ, splitMaxZ = maxZ;

        // Focus directional light shadow camera to the visible geometry combined bounds before collecting the actual geometries, and collect shadowcasters late
        if (lightType == LIGHT_DIRECTIONAL)
        {
            light->SetupShadowView(shadowViewIdx, camera, &geometryBounds);
            splitMinZ = Max(splitMinZ, view.splitMinZ);
            splitMaxZ = Min(splitMaxZ, view.splitMaxZ);

            // Check for degenerate depth range or frustum outside split
            if (splitMinZ >= splitMaxZ || splitMinZ > view.splitMaxZ || splitMaxZ < view.splitMinZ || view.viewport == IntRect::ZERO)
                view.viewport = IntRect::ZERO;
            else
                octree->FindNodesMasked(reinterpret_cast<std::vector<OctreeNode*>&>(shadowMap.shadowCasters[view.casterListIdx]), view.shadowFrustum, NF_GEOMETRY | NF_CAST_SHADOWS);
        }

        // Skip view? (no geometry, out of range or point light face not in view)
        if (view.viewport == IntRect::ZERO)
        {
            view.renderMode = RENDER_STATIC_LIGHT_CACHED;
            view.lastViewport = IntRect::ZERO;
        }
        else
        {
            const Frustum& shadowFrustum = view.shadowFrustum;
            const Matrix3x4& lightView = view.shadowCamera->ViewMatrix();
            const std::vector<GeometryNode*>& initialShadowCasters = shadowMap.shadowCasters[view.casterListIdx];

            bool dynamicOrDirLight = lightType == LIGHT_DIRECTIONAL || !light->IsStatic();
            bool dynamicCastersMoved = false;
            bool staticCastersMoved = false;

            size_t totalShadowCasters = 0;
            size_t staticShadowCasters = 0;

            Frustum lightViewFrustum = camera->WorldSplitFrustum(splitMinZ, splitMaxZ).Transformed(lightView);
            BoundingBox lightViewFrustumBox(lightViewFrustum);

            BatchQueue* destStatic = !dynamicOrDirLight ? &shadowMap.shadowBatches[view.staticQueueIdx] : nullptr;
            BatchQueue* destDynamic = &shadowMap.shadowBatches[view.dynamicQueueIdx];

            for (auto it = initialShadowCasters.begin(); it != initialShadowCasters.end(); ++it)
            {
                GeometryNode* node = *it;
                const BoundingBox& geometryBox = node->WorldBoundingBox();

                bool inView = node->InView(frameNumber);
                bool staticNode = node->IsStatic();

                // Check shadowcaster frustum visibility for point lights; may be visible in view, but not in each cube map face
                if (lightType == LIGHT_POINT && !shadowFrustum.IsInsideFast(geometryBox))
                    continue;

                // Furthermore, check by bounding box extrusion if out-of-view or directional light shadowcaster actually contributes to visible geometry shadowing or if it can be skipped
                // This is done only for dynamic objects or dynamic lights' shadows; cached static shadowmap needs to render everything
                if ((!staticNode || dynamicOrDirLight) && !inView)
                {
                    BoundingBox lightViewBox = geometryBox.Transformed(lightView);

                    if (lightType == LIGHT_DIRECTIONAL)
                    {
                        lightViewBox.max.z = Max(lightViewBox.max.z, lightViewFrustumBox.max.z);
                        if (!lightViewFrustum.IsInsideFast(lightViewBox))
                            continue;
                    }
                    else
                    {
                        // For perspective lights, extrusion direction depends on the position of the shadow caster
                        Vector3 center = lightViewBox.Center();
                        Ray extrusionRay(center, center);

                        float extrusionDistance = view.shadowCamera->FarClip();
                        float originalDistance = Clamp(center.Length(), M_EPSILON, extrusionDistance);

                        // Because of the perspective, the bounding box must also grow when it is extruded to the distance
                        float sizeFactor = extrusionDistance / originalDistance;

                        // Calculate the endpoint box and merge it to the original. Because it's axis-aligned, it will be larger
                        // than necessary, so the test will be conservative
                        Vector3 newCenter = extrusionDistance * extrusionRay.direction;
                        Vector3 newHalfSize = lightViewBox.Size() * sizeFactor * 0.5f;
                        BoundingBox extrudedBox(newCenter - newHalfSize, newCenter + newHalfSize);
                        lightViewBox.Merge(extrudedBox);

                        if (!lightViewFrustum.IsInsideFast(lightViewBox))
                            continue;
                    }
                }

                // If not in view, let the node prepare itself for render now
                if (!inView)
                {
                    if (!node->OnPrepareRender(frameNumber, camera))
                        continue;
                }

                ++totalShadowCasters;

                if (staticNode)
                {
                    ++staticShadowCasters;
                    if (node->LastUpdateFrameNumber() == frameNumber)
                        staticCastersMoved = true;
                }
                else
                {
                    if (node->LastUpdateFrameNumber() == frameNumber)
                        dynamicCastersMoved = true;
                }

                // If did not allocate a static queue, just put everything to dynamic
                BatchQueue& dest = destStatic ? (staticNode ? *destStatic : *destDynamic) : *destDynamic;
                const SourceBatches& batches = node->Batches();
                size_t numGeometries = batches.NumGeometries();

                Batch newBatch;

                for (size_t j = 0; j < numGeometries; ++j)
                {
                    Material* material = batches.GetMaterial(j);
                    newBatch.pass = material->GetPass(PASS_SHADOW);
                    if (!newBatch.pass)
                        continue;

                    newBatch.geometry = batches.GetGeometry(j);
                    newBatch.programBits = (unsigned char)node->GetGeometryType();
                    newBatch.geomIndex = (unsigned char)j;

                    if (!newBatch.programBits)
                        newBatch.worldTransform = &node->WorldTransform();
                    else
                        newBatch.node = node;

                    dest.batches.push_back(newBatch);
                }
            }

            // Now determine which kind of caching can be used for the shadow map
            // Dynamic or directional lights
            if (dynamicOrDirLight)
            {
                // If light atlas allocation changed, light moved, or amount of objects in view changed, render an optimized shadow map
                if (view.lastViewport != view.viewport || !view.lastShadowMatrix.Equals(view.shadowMatrix, 0.0001f) || view.lastNumGeometries != totalShadowCasters || dynamicCastersMoved || staticCastersMoved)
                    view.renderMode = RENDER_DYNAMIC_LIGHT;
                else
                    view.renderMode = RENDER_STATIC_LIGHT_CACHED;
            }
            // Static lights
            else
            {
                // If light atlas allocation has changed, or the static light changed, render a full shadow map now that can be cached next frame
                if (view.lastViewport != view.viewport || !view.lastShadowMatrix.Equals(view.shadowMatrix, 0.0001f))
                    view.renderMode = RENDER_STATIC_LIGHT_STORE_STATIC;
                else
                {
                    view.renderMode = RENDER_STATIC_LIGHT_CACHED;

                    // If static shadowcasters updated themselves (e.g. LOD change), render shadow map fully
                    // If dynamic casters moved, need to restore shadowmap and rerender
                    if (staticCastersMoved)
                        view.renderMode = RENDER_STATIC_LIGHT_STORE_STATIC;
                    else
                    {
                        if (dynamicCastersMoved || view.lastNumGeometries != totalShadowCasters)
                            view.renderMode = staticShadowCasters > 0 ? RENDER_STATIC_LIGHT_RESTORE_STATIC : RENDER_DYNAMIC_LIGHT;
                    }
                }
            }

            // If no rendering to be done, use the last rendered shadow projection matrix to avoid artifacts when rotating camera
            if (view.renderMode == RENDER_STATIC_LIGHT_CACHED)
                view.shadowMatrix = view.lastShadowMatrix;
            else
            {
                view.lastViewport = view.viewport;
                view.lastNumGeometries = totalShadowCasters;
                view.lastShadowMatrix = view.shadowMatrix;

                // Clear static batch queue if not needed
                if (destStatic && view.renderMode != RENDER_STATIC_LIGHT_STORE_STATIC)
                    destStatic->Clear();
            }
        }

        // For a point light, process all its views in the same task
        if (lightType == LIGHT_POINT && shadowViewIdx < shadowMap.shadowViews.size() - 1 && shadowMap.shadowViews[shadowViewIdx + 1]->light == light)
            ++shadowViewIdx;
        else
            break;
    }

    // Sort shadow batches if was the last
    if (numPendingShadowViews[shadowMapIdx].fetch_add(-1) == 1)
        SortShadowBatches(shadowMap);
}

void Renderer::CullLightsToFrustumWork(Task* task, unsigned)
{
    ZoneScoped;

    // Cull lights against each cluster frustum on the given Z-level
    size_t z = (size_t)task->start;
    const Matrix3x4& cameraView = camera->ViewMatrix();

    for (size_t i = 0; i < lights.size(); ++i)
    {
        Light* light = lights[i];
        LightType lightType = light->GetLightType();

        if (lightType == LIGHT_POINT)
        {
            Sphere bounds(cameraView * light->WorldPosition(), light->Range());
            float minViewZ = bounds.center.z - light->Range();
            float maxViewZ = bounds.center.z + light->Range();

            size_t idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
            if (minViewZ > clusterFrustums[idx].vertices[4].z || maxViewZ < clusterFrustums[idx].vertices[0].z || numClusterLights[idx] >= MAX_LIGHTS_CLUSTER)
                continue;

            for (size_t y = 0; y < NUM_CLUSTER_Y; ++y)
            {
                for (size_t x = 0; x < NUM_CLUSTER_X; ++x)
                {
                    if (bounds.IsInsideFast(clusterBoundingBoxes[idx]) && clusterFrustums[idx].IsInsideFast(bounds))
                        clusterData[(idx << 4) + numClusterLights[idx]++] = (unsigned char)(i + 1);

                    ++idx;
                }
            }
        }
        else if (lightType == LIGHT_SPOT)
        {
            Frustum bounds(light->WorldFrustum().Transformed(cameraView));
            BoundingBox boundsBox(bounds);
            float minViewZ = boundsBox.min.z;
            float maxViewZ = boundsBox.max.z;

            size_t idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
            if (minViewZ > clusterFrustums[idx].vertices[4].z || maxViewZ < clusterFrustums[idx].vertices[0].z || numClusterLights[idx] >= MAX_LIGHTS_CLUSTER)
                continue;

            for (size_t y = 0; y < NUM_CLUSTER_Y; ++y)
            {
                for (size_t x = 0; x < NUM_CLUSTER_X; ++x)
                {
                    if (bounds.IsInsideFast(clusterBoundingBoxes[idx]) && clusterFrustums[idx].IsInsideFast(boundsBox))
                        clusterData[(idx << 4) + numClusterLights[idx]++] = (unsigned char)(i + 1);

                    ++idx;
                }
            }
        }
    }
}

void RegisterRendererLibrary()
{
    static bool registered = false;
    if (registered)
        return;

    // Scene node base attributes are needed
    RegisterSceneLibrary();
    Octree::RegisterObject();
    Camera::RegisterObject();
    OctreeNode::RegisterObject();
    GeometryNode::RegisterObject();
    StaticModel::RegisterObject();
    Bone::RegisterObject();
    AnimatedModel::RegisterObject();
    Light::RegisterObject();
    Material::RegisterObject();
    Model::RegisterObject();
    Animation::RegisterObject();

    registered = true;
}
