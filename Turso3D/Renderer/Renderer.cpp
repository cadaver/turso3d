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
#include "../Time/Profiler.h"
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

Renderer::Renderer() :
    workQueue(Subsystem<WorkQueue>()),
    frameNumber(0),
    sortViewNumber(0),
    clusterFrustumsDirty(true),
    lastPerViewUniforms(0),
    lastPerMaterialUniforms(0),
    lastBlendMode(MAX_BLEND_MODES),
    lastCullMode(MAX_CULL_MODES),
    lastDepthTest(MAX_COMPARE_MODES),
    lastColorWrite(true),
    lastDepthWrite(true),
    lastDepthBias(false),
    hasInstancing(false),
    instancingEnabled(false),
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

    lightDataBuffer = new UniformBuffer();
    lightDataBuffer->Define(USAGE_DYNAMIC, MAX_LIGHTS * sizeof(LightData));

    geometries.resize(workQueue->NumThreads());
    initialLights.resize(workQueue->NumThreads());

    for (size_t i = 0; i < geometries.size() * 2; ++i)
    {
        AutoPtr<Task> task = new MemberFunctionTask<Renderer>(this, &Renderer::CollectNodesWork);
        collectNodeTasks.push_back(task);
    }

    shadowLightTask = new MemberFunctionTask<Renderer>(this, &Renderer::ProcessShadowLightsWork);
    shadowDirLightTask = new MemberFunctionTask<Renderer>(this, &Renderer::ProcessShadowDirLightWork);
    cullLightsTask = new MemberFunctionTask<Renderer>(this, &Renderer::CullLightsToFrustumWork);
    nodeBatchesTask = new MemberFunctionTask<Renderer>(this, &Renderer::CollectNodeBatchesWork);
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
    PROFILE(PrepareView);

    if (!scene_ || !camera_)
        return;

    scene = scene_;
    camera = camera_;
    octree = scene->FindChild<Octree>();
    if (!octree)
        return;

    drawShadows = shadowMaps.size() ? drawShadows_ : false;

    // Framenumber is never 0
    ++frameNumber;
    if (!frameNumber)
        ++frameNumber;

    frustum = camera->WorldFrustum();
    viewMask = camera->ViewMask();

    for (size_t i = 0; i < geometries.size(); ++i)
        geometries[i].clear();
    for (size_t i = 0; i < initialLights.size(); ++i)
        initialLights[i].clear();
    lights.clear();
    dirLight = nullptr;
    lastCamera = nullptr;

    opaqueBatches.Clear();
    alphaBatches.Clear();

    for (auto it = shadowMaps.begin(); it != shadowMaps.end(); ++it)
        it->Clear();

    CollectVisibleNodes();
    CollectNodeBatchesAndLights();
}

void Renderer::RenderShadowMaps()
{
    PROFILE(RenderShadowMaps);

    Texture::Unbind(8);
    Texture::Unbind(9);

    for (size_t i = 0; i < shadowMaps.size(); ++i)
    {
        ShadowMap& shadowMap = shadowMaps[i];
        if (shadowMap.shadowViews.empty())
            continue;

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

            switch (view->renderMode)
            {
            case RENDER_DYNAMIC_LIGHT:
                Clear(false, true, view->viewport);
                break;

            case RENDER_STATIC_LIGHT_RESTORE_STATIC:
                FrameBuffer::Blit(shadowMap.fbo, view->viewport, staticObjectShadowFbo, view->viewport, false, true, FILTER_POINT);
                break;
            }
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
    PROFILE(RenderOpaque);

    // Update light data now
    ImageLevel clusterLevel(IntVector3(NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), FMT_RG32U, clusterData);
    clusterTexture->SetData(0, IntBox(0, 0, 0, NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), clusterLevel);
    lightDataBuffer->SetData(0, lights.size() * sizeof(LightData), lightData);

    if (shadowMaps.size())
    {
        shadowMaps[0].texture->Bind(8);
        shadowMaps[1].texture->Bind(9);
        faceSelectionTexture1->Bind(10);
        faceSelectionTexture2->Bind(11);
    }

    clusterTexture->Bind(12);
    lightDataBuffer->Bind(0);

    RenderBatches(camera, opaqueBatches);
}

void Renderer::RenderAlpha()
{
    PROFILE(RenderAlpha);

    if (shadowMaps.size())
    {
        shadowMaps[0].texture->Bind(8);
        shadowMaps[1].texture->Bind(9);
        faceSelectionTexture1->Bind(10);
        faceSelectionTexture2->Bind(11);
    }

    clusterTexture->Bind(12);
    lightDataBuffer->Bind(0);

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
    quadVertexBuffer->Bind(0x1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::CollectVisibleNodes()
{
    PROFILE(CollectVisibleNodes);

    octree->Update(frameNumber);

    octants.clear();
    octree->FindOctantsMasked(octants, frustum);

    size_t octantsPerTask = octants.size() / collectNodeTasks.size();

    for (size_t i = 0; i < collectNodeTasks.size(); ++i)
    {
        collectNodeTasks[i]->start = &(*(octants.begin() + i * octantsPerTask));
        collectNodeTasks[i]->end = (i == collectNodeTasks.size() - 1) ? &(*octants.end()) : &(*(octants.begin() + (i + 1) * octantsPerTask));
    }

    workQueue->QueueTasks(collectNodeTasks);
    workQueue->Complete();

    for (size_t i = 0; i < initialLights.size(); ++i)
    {
        for (auto it = initialLights[i].begin(); it != initialLights[i].end(); ++it)
        {
            Light* light = *it;
            if (light->GetLightType() != LIGHT_DIRECTIONAL)
                lights.push_back(light);
            else if (dirLight == nullptr || light->GetColor().Average() > dirLight->GetColor().Average())
                dirLight = light;
        }
    }
}

void Renderer::CollectNodeBatchesAndLights()
{
    PROFILE(CollectNodeBatchesAndLights);

    // Sort localized lights by increasing distance
    std::sort(lights.begin(), lights.end(), CompareLights);

    // Clamp to maximum supported
    if (lights.size() > MAX_LIGHTS)
        lights.resize(MAX_LIGHTS);

    // Check if directional light needs shadows
    if (dirLight)
    {
        if (!drawShadows || dirLight->ShadowStrength() >= 1.0f || !AllocateShadowMap(dirLight))
            dirLight->SetShadowMap(nullptr);
        else
            workQueue->QueueTask(shadowDirLightTask);
    }

    workQueue->QueueTask(shadowLightTask);
    workQueue->QueueTask(cullLightsTask);
    workQueue->QueueTask(nodeBatchesTask);

    workQueue->Complete();

    opaqueBatches.Sort(SORT_STATE_AND_DISTANCE, hasInstancing);
    alphaBatches.Sort(SORT_DISTANCE, hasInstancing);
}

void Renderer::CollectShadowBatches(ShadowMap& shadowMap, ShadowView& view, const std::vector<GeometryNode*>& potentialShadowCasters, bool checkFrustum)
{
    Light* light = view.light;
    const Frustum& shadowFrustum = view.shadowFrustum;
    BoundingBox lightViewFrustumBox(view.lightViewFrustum);
    const Matrix3x4& lightView = view.shadowCamera->ViewMatrix();

    bool dynamicOrDirLight = light->GetLightType() == LIGHT_DIRECTIONAL || !light->Static();
    bool hasDynamicCasters = false;

    shadowCasters.clear();

    for (auto it = potentialShadowCasters.begin(); it != potentialShadowCasters.end(); ++it)
    {
        GeometryNode* node = *it;

        if (checkFrustum && !shadowFrustum.IsInsideFast(node->WorldBoundingBox()))
            continue;

        // If shadowcaster is not visible in the main view frustum, check whether its elongated bounding box is
        // This is done only for dynamic objects or dynamic lights' shadows; cached static shadowmap needs to render everything
        bool inView = node->LastFrameNumber() == frameNumber;
        // If not in view, let the node prepare itself for render now. Note: is called for each light the object casts shadows from
        if (!inView)
        {
            if (!node->OnPrepareRender(0, camera))
                continue;
        }

        bool dynamicNode = !node->Static();

        if (!inView && (dynamicNode || dynamicOrDirLight))
        { 
            BoundingBox lightViewBox = node->WorldBoundingBox().Transformed(lightView);
            
            if (view.shadowCamera->IsOrthographic())
                lightViewBox.max.z = Max(lightViewBox.max.z, lightViewFrustumBox.max.z);
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
            }

            if (!view.lightViewFrustum.IsInsideFast(lightViewBox))
                continue;
        }

        shadowCasters.push_back(node);

        if (dynamicNode)
            hasDynamicCasters = true;
    }

    // Now determine which kind of caching can be used for the shadow map, and if we need to go further
    // Dynamic or directional lights
    if (dynamicOrDirLight)
    {
        // If light atlas allocation changed, light moved, or amount of objects in view changed, render an optimized shadow map
        if (view.lastViewport != view.viewport || !view.lastShadowMatrix.Equals(view.shadowMatrix, 0.0001f) || view.lastNumGeometries != shadowCasters.size())
            view.renderMode = RENDER_DYNAMIC_LIGHT;
        else
        {
            // Assume no rendering
            view.renderMode = RENDER_STATIC_LIGHT_CACHED;

            // If any of the objects moved this frame, same as above
            for (auto it = shadowCasters.begin(); it != shadowCasters.end(); ++it)
            {
                GeometryNode* node = *it;
                if (node->LastUpdateFrameNumber() == frameNumber)
                {
                    view.renderMode = RENDER_DYNAMIC_LIGHT;
                    break;
                }
            }
        }
    }
    // Static lights
    else
    {
        // If light atlas allocation has changed, or the static light changed, render a full shadow map now that can be cached next frame
        if (view.lastViewport != view.viewport || !view.lastShadowMatrix.Equals(view.shadowMatrix, 0.0001f))
            view.renderMode = RENDER_STATIC_LIGHT_STORE_STATIC;
        else
        {
            // If has dynamic casters now or last frame, restore static shadow map
            view.renderMode = (view.lastDynamicCasters || hasDynamicCasters) ? RENDER_STATIC_LIGHT_RESTORE_STATIC : RENDER_STATIC_LIGHT_CACHED;

            // If static shadowcasters updated themselves (e.g. LOD change), render shadow map fully
            for (auto it = shadowCasters.begin(); it != shadowCasters.end(); ++it)
            {
                GeometryNode* node = *it;
                
                if (node->Static() && node->LastUpdateFrameNumber() == frameNumber)
                {
                    view.renderMode = RENDER_STATIC_LIGHT_STORE_STATIC;
                    break;
                }
            }
        }
    }

    // If no rendering to be done, return now without collecting batches
    // Note: use the last rendered shadow projection matrix to avoid artifacts when rotating camera
    if (view.renderMode == RENDER_STATIC_LIGHT_CACHED)
    {
        view.shadowMatrix = view.lastShadowMatrix;
        return;
    }

    view.lastDynamicCasters = hasDynamicCasters;
    view.lastViewport = view.viewport;
    view.lastNumGeometries = shadowCasters.size();
    view.lastShadowMatrix = view.shadowMatrix;

    // Determine batch queues to use
    BatchQueue* destStatic = nullptr;
    BatchQueue* destDynamic = nullptr;
    if (view.renderMode == RENDER_STATIC_LIGHT_STORE_STATIC)
    {
        if (shadowMap.shadowBatches.size() < shadowMap.freeQueueIdx + 2)
            shadowMap.shadowBatches.resize(shadowMap.freeQueueIdx + 2);

        destStatic = &shadowMap.shadowBatches[shadowMap.freeQueueIdx];
        destDynamic = &shadowMap.shadowBatches[shadowMap.freeQueueIdx + 1];
        destStatic->Clear();
        destDynamic->Clear();

        view.staticQueueIdx = shadowMap.freeQueueIdx;
        view.dynamicQueueIdx = shadowMap.freeQueueIdx + 1;
        shadowMap.freeQueueIdx += 2;
    }
    else
    {
        if (shadowMap.shadowBatches.size() < shadowMap.freeQueueIdx + 1)
            shadowMap.shadowBatches.resize(shadowMap.freeQueueIdx + 1);

        destDynamic = &shadowMap.shadowBatches[shadowMap.freeQueueIdx];
        destDynamic->Clear();

        view.dynamicQueueIdx = shadowMap.freeQueueIdx;
        ++shadowMap.freeQueueIdx;
    }

    Batch newBatch;

    for (auto it = shadowCasters.begin(); it != shadowCasters.end(); ++it)
    {
        GeometryNode* node = *it;
        bool dynamicNode = !node->Static();

        // Skip static casters unless is a dynamic light render, or going to store them to the static map
        if (!dynamicNode && view.renderMode > RENDER_STATIC_LIGHT_STORE_STATIC)
            continue;

        // Avoid unnecessary splitting into dynamic and static objects
        BatchQueue& dest = destStatic ? (node->Static() ? *destStatic : *destDynamic) : *destDynamic;
        const SourceBatches& batches = node->Batches();
        size_t numGeometries = batches.NumGeometries();

        for (size_t i = 0; i < numGeometries; ++i)
        {
            Material* material = batches.GetMaterial(i);
            newBatch.pass = material->GetPass(PASS_SHADOW);
            if (!newBatch.pass)
                continue;

            newBatch.geometry = batches.GetGeometry(i);
            newBatch.programBits = (unsigned char)node->GetGeometryType();

            if (!newBatch.programBits)
                newBatch.worldTransform = &node->WorldTransform();
            else
                newBatch.node = node;

            dest.batches.push_back(newBatch);
        }
    }

    if (destStatic)
        destStatic->Sort(SORT_STATE, hasInstancing);
    
    destDynamic->Sort(SORT_STATE, hasInstancing);
}

void Renderer::RenderBatches(Camera* camera_, const BatchQueue& queue)
{
    lastMaterial = nullptr;
    lastPass = nullptr;

    if (hasInstancing && queue.instanceTransforms.size())
    {
        if (instanceVertexBuffer->NumVertices() < queue.instanceTransforms.size())
            instanceVertexBuffer->Define(USAGE_DYNAMIC, queue.instanceTransforms.size(), instanceVertexElements, &queue.instanceTransforms[0]);
        else
            instanceVertexBuffer->SetData(0, queue.instanceTransforms.size(), &queue.instanceTransforms[0]);
    }

    if (camera_ != lastCamera)
    {
        lastCamera = camera_;
        ++lastPerViewUniforms;
        if (!lastPerViewUniforms)
            ++lastPerViewUniforms;
    }

    for (auto it = queue.batches.begin(); it != queue.batches.end();)
    {
        const Batch& batch = *it;
        unsigned char geometryBits = batch.programBits & SP_GEOMETRYBITS;

        ShaderProgram* program = batch.pass->GetShaderProgram(batch.programBits);
        if (!program->Bind())
        {
            it += (geometryBits == GEOM_INSTANCED) ? batch.instanceCount : 1;
            continue;
        }

        if (program->lastPerViewUniforms != lastPerViewUniforms)
        {
            Matrix4 projection = camera_->ProjectionMatrix();
            const Matrix3x4& view = camera_->ViewMatrix();

            int location = program->Uniform(U_VIEWMATRIX);
            if (location >= 0)
                glUniformMatrix3x4fv(location, 1, GL_FALSE, view.Data());
            location = program->Uniform(U_PROJECTIONMATRIX);
            if (location >= 0)
                glUniformMatrix4fv(location, 1, GL_FALSE, projection.Data());
            location = program->Uniform(U_VIEWPROJMATRIX);
            if (location >= 0)
                glUniformMatrix4fv(location, 1, GL_FALSE, (projection * view).Data());
            location = program->Uniform(U_DEPTHPARAMETERS);
            if (location >= 0)
            {
                Vector4 depthParameters(camera_->NearClip(), camera_->FarClip(), 0.0f, 0.0f);
                if (camera_->IsOrthographic())
                {
                    depthParameters.z = 0.5f;
                    depthParameters.w = 0.5f;
                }
                else
                    depthParameters.w = 1.0f / camera_->FarClip();

                glUniform4fv(location, 1, depthParameters.Data());
            }
            location = program->Uniform(U_DIRLIGHTDATA);
            if (location >= 0)
            {
                Vector4 dirLightData[12];
                bool hasShadow = false;

                if (!dirLight)
                {
                    dirLightData[0] = Vector4::ZERO;
                    dirLightData[1] = Vector4::ZERO;
                    dirLightData[3] = Vector4::ONE;
                }
                else
                {
                    dirLightData[0] = Vector4(-dirLight->WorldDirection(), 0.0f);
                    dirLightData[1] = dirLight->GetColor().Data();

                    if (dirLight->ShadowMap())
                    {
                        hasShadow = true;

                        float farClip = camera->FarClip();
                        float firstSplit = dirLight->ShadowSplit(0) / farClip;
                        float secondSplit = dirLight->ShadowSplit(1) / farClip;

                        dirLightData[2] = Vector4(firstSplit, secondSplit, dirLight->ShadowFadeStart() * secondSplit, 1.0f / (secondSplit - dirLight->ShadowFadeStart() * secondSplit));
                        dirLightData[3] = dirLight->ShadowParameters();
                        if (dirLight->ShadowViews().size() >= 2)
                        {
                            *reinterpret_cast<Matrix4*>(&dirLightData[4]) = dirLight->ShadowViews()[0].shadowMatrix;
                            *reinterpret_cast<Matrix4*>(&dirLightData[8]) = dirLight->ShadowViews()[1].shadowMatrix;
                        }
                    }
                    else
                        dirLightData[3] = Vector4::ONE;
                }

                glUniform4fv(location, hasShadow ? 12 : 4, dirLightData[0].Data());
            }

            program->lastPerViewUniforms = lastPerViewUniforms;
        }

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

            it += batch.instanceCount;
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

            VertexBuffer* vb = geometry->vertexBuffer;
            IndexBuffer* ib = geometry->indexBuffer;
            vb->Bind(program->Attributes());
            if (ib)
                ib->Bind();

            if (!geometryBits)
                glUniformMatrix3x4fv(program->Uniform(U_WORLDMATRIX), 1, GL_FALSE, batch.worldTransform->Data());
            // TODO: set per-object uniforms from node for other geometry modes (skinning, billboards...)

            if (!ib)
                glDrawArrays(GL_TRIANGLES, (GLsizei)geometry->drawStart, (GLsizei)geometry->drawCount);
            else
                glDrawElements(GL_TRIANGLES, (GLsizei)geometry->drawCount, ib->IndexSize() == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, 
                    (const void*)(geometry->drawStart * ib->IndexSize()));

            ++it;
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
        // Position
        -1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f
    };

    std::vector<VertexElement> vertexDeclaration;
    vertexDeclaration.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
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
        PROFILE(DefineClusterFrustums);

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

void Renderer::CollectNodesWork(Task* task, unsigned threadIndex)
{
    std::pair<Octant*, unsigned char>* start = reinterpret_cast<std::pair<Octant*, unsigned char>*>(task->start);
    std::pair<Octant*, unsigned char>* end = reinterpret_cast<std::pair<Octant*, unsigned char>*>(task->end);
    std::vector<GeometryNode*>& geometryQueue = geometries[threadIndex];
    std::vector<Light*>& lightQueue = initialLights[threadIndex];

    for (; start != end; ++start)
    {
        Octant* octant = start->first;
        unsigned char planeMask = start->second;
        std::vector<OctreeNode*>& nodes = octant->nodes;

        for (auto it = nodes.begin(); it != nodes.end(); ++it)
        {
            OctreeNode* node = *it;
            unsigned short flags = node->Flags();

            if ((node->LayerMask() & viewMask) && (!planeMask || frustum.IsInsideMaskedFast(node->WorldBoundingBox(), planeMask)))
            {
                if (flags & NF_GEOMETRY)
                {
                    GeometryNode* geometry = static_cast<GeometryNode*>(node);
                    if (geometry->OnPrepareRender(frameNumber, camera))
                        geometryQueue.push_back(geometry);
                }
                else if (flags & NF_LIGHT)
                {
                    Light* light = static_cast<Light*>(node);
                    if (light->OnPrepareRender(frameNumber, camera))
                        lightQueue.push_back(light);
                }
            }
        }
    }
}

void Renderer::ProcessShadowLightsWork(Task*, unsigned)
{
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
    shadowMapsDirty = false;

    for (size_t i = 0; i < lights.size(); ++i)
    {
        Light* light = lights[i];
        float cutoff = light->GetLightType() == LIGHT_SPOT ? cosf(light->Fov() * 0.5f * M_DEGTORAD) : 0.0f;

        lightData[i].position = Vector4(light->WorldPosition(), 1.0f);
        lightData[i].direction = Vector4(-light->WorldDirection(), 0.0f);
        lightData[i].attenuation = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f / (1.0f - cutoff), 1.0f);
        lightData[i].color = light->EffectiveColor();
        lightData[i].shadowParameters = Vector4::ONE; // Assume unshadowed

        if (!drawShadows || light->ShadowStrength() >= 1.0f)
        {
            light->SetShadowMap(nullptr);
            continue;
        }

        // Query for shadowcasters. Lit geometries (both opaque & alpha) are handled by frustum grid so need no bookkeeping
        pointSpotShadowCasters.clear();

        switch (light->GetLightType())
        {
        case LIGHT_POINT:
            octree->FindNodes(reinterpret_cast<std::vector<OctreeNode*>&>(pointSpotShadowCasters), light->WorldSphere(), NF_GEOMETRY | NF_CASTSHADOWS);
            break;

        case LIGHT_SPOT:
            octree->FindNodesMasked(reinterpret_cast<std::vector<OctreeNode*>&>(pointSpotShadowCasters), light->WorldFrustum(), NF_GEOMETRY | NF_CASTSHADOWS);
            break;
        }

        if (!pointSpotShadowCasters.size())
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

        // Setup shadow cameras & find shadow casters
        light->SetupShadowViews(camera);
        std::vector<ShadowView>& shadowViews = light->ShadowViews();

        lightData[i].shadowParameters = light->ShadowParameters();
        lightData[i].shadowMatrix = light->ShadowViews()[0].shadowMatrix;

        for (size_t j = 0; j < shadowViews.size(); ++j)
        {
            ShadowView& view = shadowViews[j];
            shadowMaps[1].shadowViews.push_back(&view);

            switch (light->GetLightType())
            {
            case LIGHT_POINT:
                // Check which lit geometries are shadow casters and inside each shadow frustum. First check whether the shadow frustum is inside the view at all
                /// \todo Could use a frustum-frustum test for more accuracy
                if (frustum.IsInsideFast(BoundingBox(view.shadowFrustum)))
                    CollectShadowBatches(shadowMaps[1], view, pointSpotShadowCasters, true);
                else
                {
                    // If not inside the view (and thus not rendered), cannot consider it cached for next frame. However shadow map render this frame is a no-op
                    view.lastViewport = IntRect::ZERO;
                    view.renderMode = RENDER_STATIC_LIGHT_CACHED;
                }
                break;

            case LIGHT_SPOT:
                // For spot light need no frustum check
                CollectShadowBatches(shadowMaps[1], view, pointSpotShadowCasters, false);
                break;
            }
        }
    }
}

void Renderer::ProcessShadowDirLightWork(Task*, unsigned)
{
    dirLight->SetupShadowViews(camera);
    std::vector<ShadowView>& shadowViews = dirLight->ShadowViews();

    for (size_t i = 0; i < shadowViews.size(); ++i)
    {
        ShadowView& view = shadowViews[i];
        shadowMaps[0].shadowViews.push_back(&view);

        // Directional light needs a new frustum query for each split, as the shadow cameras are typically far outside the main view
        directionalShadowCasters.clear();
        octree->FindNodesMasked(reinterpret_cast<std::vector<OctreeNode*>&>(directionalShadowCasters), view.shadowFrustum, NF_GEOMETRY | NF_CASTSHADOWS);

        CollectShadowBatches(shadowMaps[0], view, directionalShadowCasters, false);
    }
}

void Renderer::CullLightsToFrustumWork(Task*, unsigned)
{
    // Update cluster frustums and bounding boxes if camera changed
    DefineClusterFrustums();

    // Clear per-cluster light data
    memset(numClusterLights, 0, sizeof numClusterLights);
    memset(clusterData, 0, sizeof clusterData);

    // Cull lights against each cluster frustum.
    Matrix3x4 cameraView = camera->ViewMatrix();

    for (size_t i = 0; i < lights.size(); ++i)
    {
        Light* light = lights[i];

        if (light->GetLightType() == LIGHT_POINT)
        {
            Sphere bounds(cameraView * light->WorldPosition(), light->Range());
            float minViewZ = bounds.center.z - light->Range();
            float maxViewZ = bounds.center.z + light->Range();

            for (size_t z = 0; z < NUM_CLUSTER_Z; ++z)
            {
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
        }
        else if (light->GetLightType() == LIGHT_SPOT)
        {
            Frustum bounds(light->WorldFrustum().Transformed(cameraView));
            BoundingBox boundsBox(bounds);
            float minViewZ = boundsBox.min.z;
            float maxViewZ = boundsBox.max.z;

            for (size_t z = 0; z < NUM_CLUSTER_Z; ++z)
            {
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
}

void Renderer::CollectNodeBatchesWork(Task*, unsigned)
{
    ++sortViewNumber;
    if (!sortViewNumber)
        ++sortViewNumber;

    float farClipMul = 32767.0f / camera->FarClip();

    Batch newBatch;

    for (size_t i = 0; i < geometries.size(); ++i)
    {
        for (auto it = geometries[i].begin(); it != geometries[i].end(); ++it)
        {
            GeometryNode* node = *it;
            unsigned short distance = (unsigned short)(node->Distance() * farClipMul);
            const SourceBatches& batches = node->Batches();
            size_t numGeometries = batches.NumGeometries();

            for (size_t j = 0; j < numGeometries; ++j)
            {
                Material* material = batches.GetMaterial(j);

                // Assume opaque first
                newBatch.pass = material->GetPass(PASS_OPAQUE);
                newBatch.programBits = (unsigned char)node->GetGeometryType();
                newBatch.geometry = batches.GetGeometry(j);

                if (!newBatch.programBits)
                    newBatch.worldTransform = &node->WorldTransform();
                else
                    newBatch.node = node;

                if (newBatch.pass)
                {
                    // Perform distance sort in addition to state sort
                    if (newBatch.pass->lastSortKey.first != sortViewNumber || newBatch.pass->lastSortKey.second > distance)
                    {
                        newBatch.pass->lastSortKey.first = sortViewNumber;
                        newBatch.pass->lastSortKey.second = distance;
                    }
                    if (newBatch.geometry->lastSortKey.first != sortViewNumber || newBatch.geometry->lastSortKey.second > distance)
                    {
                        newBatch.geometry->lastSortKey.first = sortViewNumber;
                        newBatch.geometry->lastSortKey.second = distance;
                    }

                    opaqueBatches.batches.push_back(newBatch);
                }
                else
                {
                    // If not opaque, try transparent
                    newBatch.pass = material->GetPass(PASS_ALPHA);
                    if (!newBatch.pass)
                        continue;

                    newBatch.distance = node->Distance();
                    alphaBatches.batches.push_back(newBatch);
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
