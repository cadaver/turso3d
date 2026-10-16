// For conditions of distribution and use, seecopyright notice in License.txt

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
#include "DebugRenderer.h"
#include "Light.h"
#include "LightEnvironment.h"
#include "Material.h"
#include "Model.h"
#include "Octree.h"
#include "Renderer.h"
#include "RendererTasks.h"
#include "StaticModel.h"

#include <cstring>
#include <tracy/Tracy.hpp>

static const size_t DRAWABLES_PER_BATCH_TASK = 256;
static const size_t NUM_BOX_INDICES = 36;
static const float OCCLUSION_MARGIN = 0.1f;

void ThreadOctantResult::Clear()
{
    drawableAcc = 0;
    taskOctantIdx = 0;
    batchTaskIdx = 0;
    lights.clear();
    octants.clear();
    occlusionQueries.clear();
}

void ThreadBatchResult::Clear()
{
    minZ = M_MAX_FLOAT;
    maxZ = 0.0f;
    geometryBounds.Destroy();
    for (size_t i = 0; i < NUM_OPAQUE_Z_SPLITS; ++i)
        opaqueBatches[i].clear();
    alphaBatches.clear();
}

ShadowMap::ShadowMap()
{
    // Construct texture but do not define its size yet
    texture = Object::Create<Texture>();
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
    graphics(Subsystem<Graphics>()),
    workQueue(Subsystem<WorkQueue>()),
    frameNumber(0),
    clusterFrustumsDirty(true),
    depthBiasMul(1.0f),
    slopeScaleBiasMul(1.0f),
    minZ(0.0f),
    maxZ(0.0f),
    lastMinZ(0.0f),
    lastMaxZ(0.0f)
{
    assert(graphics && graphics->IsInitialized());
    assert(workQueue);

    RegisterSubsystem(this);
    RegisterRendererLibrary();

    hasInstancing = graphics->HasInstancing();
    if (hasInstancing)
    {
        instanceVertexBuffer = new VertexBuffer();
        instanceVertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, 3));
        instanceVertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, 4));
        instanceVertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TEXCOORD, 5));
    }

    clusterTexture = new Texture();
#ifndef GL_ES_VERSION_3_0
    clusterTexture->Define(TEX_3D, IntVector3(NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), FMT_RGBA32U, 1);
#else
    clusterTexture->Define(TEX_3D, IntVector3(NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), FMT_RGBA8, 1);
#endif
    clusterTexture->DefineSampler(FILTER_POINT, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

    clusterCullData = new ClusterCullData[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    clusterData = new unsigned char[MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    lightData = new LightData[MAX_LIGHTS + 1];
    memset(lightData, 0, (MAX_LIGHTS + 1) * sizeof(LightData));

    perViewDataBuffer = new UniformBuffer();
    perViewDataBuffer->Define(USAGE_DYNAMIC, sizeof(PerViewUniforms));

    lightDataBuffer = new UniformBuffer();
    lightDataBuffer->Define(USAGE_DYNAMIC, (MAX_LIGHTS + 1) * sizeof(LightData));

    octantResults = new ThreadOctantResult[NUM_OCTANT_TASKS];
    batchResults = new ThreadBatchResult[workQueue->NumThreads()];

    for (size_t i = 0; i < NUM_OCTANTS + 1; ++i)
    {
        collectOctantsTasks[i] = new CollectOctantsTask(this);
        collectOctantsTasks[i]->resultIdx = i;
    }

    for (size_t z = 0; z < NUM_OPAQUE_Z_SPLITS; ++z)
    {
        sortOpaqueBatchesTasks[z] = new SortOpaqueBatchesTask(this);
        sortOpaqueBatchesTasks[z]->z = z;
    }

    for (size_t z = 0; z < NUM_CLUSTER_Z; ++z)
    {
        cullLightsTasks[z] = new CullLightsTask(this);
        cullLightsTasks[z]->z = z;
    }

    processLightsTask = new ProcessLightsTask(this);
    batchesReadyTask = new Task();
    processShadowCastersTask = new ProcessShadowCastersTask(this);

    DefineBoundingBoxGeometry();
}

Renderer::~Renderer()
{
    RemoveSubsystem(this);
}

void Renderer::SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format)
{
    if (!shadowMaps)
        shadowMaps = new ShadowMap[NUM_SHADOW_MAPS];

    for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i)
    {
        ShadowMap& shadowMap = shadowMaps[i];

        shadowMap.texture->Define(TEX_2D, i == 0 ? IntVector2(dirLightSize * 2, dirLightSize) : IntVector2(lightAtlasSize, lightAtlasSize), format);
        shadowMap.texture->DefineSampler(COMPARE_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP, 1);
        shadowMap.fbo->Define(nullptr, shadowMap.texture);
    }

    if (!staticObjectShadowBuffer)
        staticObjectShadowBuffer = new RenderBuffer();
    if (!staticObjectShadowFbo)
        staticObjectShadowFbo = new FrameBuffer();

    staticObjectShadowBuffer->Define(IntVector2(lightAtlasSize, lightAtlasSize), format);
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

void Renderer::PrepareView(Scene* scene_, Camera* camera_, bool drawShadows_, bool useOcclusion_)
{
    ZoneScoped;

    if (!scene_ || !camera_)
        return;

    scene = scene_;
    camera = camera_;
    octree = scene->FindChild<Octree>();
    lightEnvironment = scene->FindChild<LightEnvironment>();
    if (!octree)
        return;

    // Framenumber is never 0
    ++frameNumber;
    if (!frameNumber)
        ++frameNumber;

    drawShadows = shadowMaps ? drawShadows_ : false;
    useOcclusion = useOcclusion_;
    frustum = camera->WorldFrustum();
    viewMask = camera->ViewMask();

    // Clear results from last frame
    dirLight = nullptr;
    lastCamera = nullptr;
    rootLevelOctants.clear();
    for (size_t i = 0; i < NUM_OPAQUE_Z_SPLITS; ++i)
    {
        opaqueBatches[i].Clear();
        opaqueInstanceTransforms[i].clear();
    }
    alphaBatches.Clear();
    alphaInstanceTransforms.clear();
    lights.clear();
    
    lastMinZ = minZ < M_MAX_FLOAT ? minZ : 0.0f;
    lastMaxZ = maxZ > 0.0f ? maxZ : camera->FarClip();
    minZ = M_MAX_FLOAT;
    maxZ = 0.0f;
    geometryBounds.Destroy();

    // Stagger for occlusion queries based on last frametime
    lastFrameTime = graphics->LastFrameTime();

    for (size_t i = 0; i < NUM_OCTANT_TASKS; ++i)
        octantResults[i].Clear();
    for (size_t i = 0; i < workQueue->NumThreads(); ++i)
        batchResults[i].Clear();
    
    if (shadowMaps)
    {
        for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i)
            shadowMaps[i].Clear();
    }

    // Process moved / animated objects' octree reinsertions
    octree->Update(frameNumber);

    // Precalculate SAT test parameters for accurate frustum test (verify what octants to occlusion query)
    if (useOcclusion)
        frustumSATData.Calculate(frustum);

    // Check arrived occlusion query results while octree update goes on, then finish octree update
    CheckOcclusionQueries();
    octree->FinishUpdate();

    // Find the starting points for octree traversal. Include the root if it contains drawables that didn't fit elsewhere
    Octant* rootOctant = octree->Root();
    if (rootOctant->Drawables().size())
        rootLevelOctants.push_back(rootOctant);

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (rootOctant->Child(i))
            rootLevelOctants.push_back(rootOctant->Child(i));
    }

    // If no root level octants, must early-out the view preparation; there is nothing to render and task dependencies would not complete
    if (rootLevelOctants.empty())
        return;

    // Keep track of both batch + octant task progress before main batches can be sorted (batch tasks will add to the counter when queued)
    numPendingBatchTasks.store((int)rootLevelOctants.size());
    numPendingShadowViewTasks[0].store(0);
    numPendingShadowViewTasks[1].store(0);

    // Ensure shadowcaster processing doesn't happen before lights have been found and processed, and geometry bounds are known
    workQueue->AddDependency(processShadowCastersTask, processLightsTask);
    workQueue->AddDependency(processShadowCastersTask, batchesReadyTask);

    // Find octants in view and their plane masks for node frustum culling. At the same time, find lights and process them
    // When octant collection tasks complete, they queue tasks for collecting batches from those octants.
    for (size_t i = 0; i < rootLevelOctants.size(); ++i)
    {
        collectOctantsTasks[i]->startOctant = rootLevelOctants[i];
        workQueue->AddDependency(processLightsTask, collectOctantsTasks[i]);
    }

    // Start threaded work and wait for it to finish
    workQueue->QueueTasks(rootLevelOctants.size(), reinterpret_cast<Task**>(&collectOctantsTasks[0]));
    workQueue->Complete();
}

void Renderer::RenderShadowMaps()
{
    if (!shadowMaps)
        return;

    ZoneScoped;

    // Unbind shadow textures before rendering to
    Texture::Unbind(TU_DIRLIGHTSHADOW);
    Texture::Unbind(TU_SHADOWATLAS);

    for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i)
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
            LightDrawable* light = view->light;

            if (view->renderMode == RENDER_STATIC_LIGHT_STORE_STATIC)
            {
                graphics->Clear(false, true, view->viewport);

                BatchQueue& batchQueue = shadowMap.shadowBatches[view->staticQueueIdx];
                if (batchQueue.HasBatches())
                {
                    graphics->SetViewport(view->viewport);
                    graphics->SetDepthBias(light->DepthBias() * depthBiasMul, light->SlopeScaleBias() * slopeScaleBiasMul);
                    RenderBatches(view->shadowCamera, batchQueue);
                }
            }
        }

        // Now do the shadowmap -> static shadowmap storage blits as necessary
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            ShadowView* view = shadowMap.shadowViews[j];

            if (view->renderMode == RENDER_STATIC_LIGHT_STORE_STATIC)
                graphics->Blit(staticObjectShadowFbo, view->viewport, shadowMap.fbo, view->viewport, false, true, FILTER_POINT);
        }

        // Rebind shadowmap
        shadowMap.fbo->Bind();

        // First do all the clears or static shadowmap -> shadowmap blits
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            ShadowView* view = shadowMap.shadowViews[j];

            if (view->renderMode == RENDER_DYNAMIC_LIGHT)
                graphics->Clear(false, true, view->viewport);
            else if (view->renderMode == RENDER_STATIC_LIGHT_RESTORE_STATIC)
                graphics->Blit(shadowMap.fbo, view->viewport, staticObjectShadowFbo, view->viewport, false, true, FILTER_POINT);
        }

        // Finally render the dynamic objects
        for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
        {
            ShadowView* view = shadowMap.shadowViews[j];
            LightDrawable* light = view->light;

            if (view->renderMode != RENDER_STATIC_LIGHT_CACHED)
            {
                BatchQueue& batchQueue = shadowMap.shadowBatches[view->dynamicQueueIdx];
                if (batchQueue.HasBatches())
                {
                    graphics->SetViewport(view->viewport);
                    graphics->SetDepthBias(light->DepthBias() * depthBiasMul, light->SlopeScaleBias() * slopeScaleBiasMul);
                    RenderBatches(view->shadowCamera, batchQueue);
                }
            }
        }
    }

    graphics->SetDepthBias(0.0f, 0.0f);
}

void Renderer::RenderOpaque(bool clear)
{
    ZoneScoped;

    // Update light data before rendering
    UpdateLightData();

    if (shadowMaps)
    {
        shadowMaps[0].texture->Bind(TU_DIRLIGHTSHADOW);
        shadowMaps[1].texture->Bind(TU_SHADOWATLAS);
        faceSelectionTexture1->Bind(TU_FACESELECTION1);
        faceSelectionTexture2->Bind(TU_FACESELECTION2);
    }

    clusterTexture->Bind(TU_LIGHTCLUSTERDATA);
    lightDataBuffer->Bind(UB_LIGHTDATA);

    if (clear)
        graphics->Clear(true, true, IntRect::ZERO, lightEnvironment ? lightEnvironment->FogColor() : DEFAULT_FOG_COLOR);

    // Render opaque batches in front to back order (coarse slicing)
    UpdateInstanceTransforms(NUM_OPAQUE_Z_SPLITS, &opaqueInstanceTransforms[0]);
    unsigned baseInstanceIndex = 0;
    for (size_t i = 0; i < NUM_OPAQUE_Z_SPLITS; ++i)
    {
        if (opaqueBatches[i].HasBatches())
        {
            RenderBatches(camera, opaqueBatches[i], baseInstanceIndex);
            baseInstanceIndex += (unsigned)opaqueInstanceTransforms[i].size();
        }
    }

    // Render occlusion now after opaques
    if (useOcclusion)
        RenderOcclusionQueries();
}

void Renderer::RenderAlpha()
{
    ZoneScoped;

    if (shadowMaps)
    {
        shadowMaps[0].texture->Bind(TU_DIRLIGHTSHADOW);
        shadowMaps[1].texture->Bind(TU_SHADOWATLAS);
        faceSelectionTexture1->Bind(TU_FACESELECTION1);
        faceSelectionTexture2->Bind(TU_FACESELECTION2);
    }

    clusterTexture->Bind(TU_LIGHTCLUSTERDATA);
    lightDataBuffer->Bind(UB_LIGHTDATA);

    UpdateInstanceTransforms(alphaInstanceTransforms);
    RenderBatches(camera, alphaBatches);
}

void Renderer::RenderDebug()
{
    ZoneScoped;

    DebugRenderer* debug = Subsystem<DebugRenderer>();
    if (!debug)
        return;

    for (auto it = lights.begin(); it != lights.end(); ++it)
        (*it)->OnRenderDebug(debug);

    for (size_t i = 0; i < rootLevelOctants.size(); ++i)
    {
        const ThreadOctantResult& result = octantResults[i];

        for (auto oIt = result.octants.begin(); oIt != result.octants.end(); ++oIt)
        {
            Octant* octant = oIt->first;
            octant->OnRenderDebug(debug);
            const std::vector<Drawable*>& drawables = octant->Drawables();

            for (auto dIt = drawables.begin(); dIt != drawables.end(); ++dIt)
            {
                Drawable* drawable = *dIt;
                if (drawable->TestFlag(DF_GEOMETRY) && drawable->LastFrameNumber() == frameNumber)
                    drawable->OnRenderDebug(debug);
            }
        }
    }
}

Texture* Renderer::ShadowMapTexture(size_t index) const
{
    return (shadowMaps && index < NUM_SHADOW_MAPS) ? shadowMaps[index].texture : nullptr;
}

void Renderer::CollectOctantsAndLights(Octant* octant, ThreadOctantResult& result, unsigned char planeMask)
{
    const BoundingBox& octantBox = octant->CullingBox();

    if (planeMask)
    {
        // If not already inside all frustum planes, do frustum test and terminate if completely outside
        planeMask = frustum.IsInsideMasked(octantBox, planeMask);
        if (planeMask == 0xff)
        {
            // If octant becomes frustum culled, reset its visibility for when it comes back to view, including its children
            if (useOcclusion && octant->Visibility() != VIS_OUTSIDE_FRUSTUM)
                octant->SetVisibility(VIS_OUTSIDE_FRUSTUM, true);
            return;
        }
    }

    // Process occlusion now before going further
    if (useOcclusion)
    {
        switch (octant->Visibility())
        {
            // If was previously outside frustum, reset to visible-unknown
        case VIS_OUTSIDE_FRUSTUM:
            octant->SetVisibility(VIS_VISIBLE_UNKNOWN, false);
            // If octant has unknown visibility, issue query if not pending, but collect child octants and drawables
        case VIS_VISIBLE_UNKNOWN:
            AddOcclusionQuery(octant, result, planeMask);
            break;

            // If octant is occluded, issue query if not pending, and do not process further this frame
        case VIS_OCCLUDED:
            AddOcclusionQuery(octant, result, planeMask);
            return;

            // If octant was occluded previously, but its parent came into view, issue tests along the hierarchy but do not render on this frame
        case VIS_OCCLUDED_UNKNOWN:
            AddOcclusionQuery(octant, result, planeMask);
            if (octant != octree->Root() && octant->HasChildren())
            {
                for (size_t i = 0; i < NUM_OCTANTS; ++i)
                {
                    if (octant->Child(i))
                        CollectOctantsAndLights(octant->Child(i), result, planeMask);
                }
            }
            return;

            // If the octant's parent is already visible too, only test the octant if it is a "leaf octant" with drawables
            // Note: visible octants will also add a time-based staggering to reduce queries
        case VIS_VISIBLE:
            Octant* parent = octant->Parent();
            if (octant->Drawables().size() > 0 || (parent && parent->Visibility() != VIS_VISIBLE))
                AddOcclusionQuery(octant, result, planeMask);
            break;
        }
    }
    else
    {
        // When occlusion not in use, reset all traversed octants to visible-unknown
        octant->SetVisibility(VIS_VISIBLE_UNKNOWN, false);
    }

    const std::vector<Drawable*>& drawables = octant->Drawables();

    for (auto it = drawables.begin(); it != drawables.end(); ++it)
    {
        Drawable* drawable = *it;

        if (drawable->TestFlag(DF_LIGHT))
        {
            const BoundingBox& lightBox = drawable->WorldBoundingBox();
            if ((drawable->LayerMask() & viewMask) && (!planeMask || frustum.IsInsideMaskedFast(lightBox, planeMask)))
                result.lights.push_back(static_cast<LightDrawable*>(drawable));
        }
        // Lights are sorted first in octants, so break when first geometry encountered. Store the octant for batch collecting
        else
        {
            result.octants.push_back(std::make_pair(octant, planeMask));
            result.drawableAcc += drawables.end() - it;

            // Setup and queue batches collection task if over the drawable limit now
            if (result.drawableAcc >= DRAWABLES_PER_BATCH_TASK)
            {
                if (result.collectBatchesTasks.size() <= result.batchTaskIdx)
                    result.collectBatchesTasks.push_back(new CollectBatchesTask(this));

                CollectBatchesTask* batchTask = result.collectBatchesTasks[result.batchTaskIdx];
                batchTask->octants.clear();
                batchTask->octants.insert(batchTask->octants.end(), result.octants.begin() + result.taskOctantIdx, result.octants.end());
                numPendingBatchTasks.fetch_add(1);
                workQueue->QueueTask(batchTask);

                result.drawableAcc = 0;
                result.taskOctantIdx = result.octants.size();
                ++result.batchTaskIdx;
            }

            break;
        }
    }

    // Root octant is handled separately. Otherwise recurse into child octants
    if (octant != octree->Root() && octant->HasChildren())
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->Child(i))
                CollectOctantsAndLights(octant->Child(i), result, planeMask);
        }
    }
}

void Renderer::AddOcclusionQuery(Octant* octant, ThreadOctantResult& result, unsigned char planeMask)
{
    // No-op if previous query still ongoing. Also If the octant intersects the frustum, verify with SAT test that it actually covers some screen area
    // Otherwise the occlusion test will produce a false negative
    if (octant->CheckNewOcclusionQuery(lastFrameTime) && (!planeMask || frustum.IsInsideSAT(octant->CullingBox(), frustumSATData)))
        result.occlusionQueries.push_back(octant);
}

bool Renderer::AllocateShadowMap(LightDrawable* light)
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

    // Shadowcaster processing needs accurate scene min / max Z results, combine them from per-thread data
    for (size_t i = 0; i < workQueue->NumThreads(); ++i)
    {
        const ThreadBatchResult& result = batchResults[i];
        minZ = Min(minZ, result.minZ);
        maxZ = Max(maxZ, result.maxZ);
        if (result.geometryBounds.IsDefined())
            geometryBounds.Merge(result.geometryBounds);
    }

    minZ = Max(minZ, camera->NearClip());

    // Signal that shadowcaster processing is OK to happen
    workQueue->QueueTask(batchesReadyTask);

    // Join and sort opaque batches in own task per Z-slice
    workQueue->QueueTasks(NUM_OPAQUE_Z_SPLITS, reinterpret_cast<Task**>(&sortOpaqueBatchesTasks[0]));

    // Join per-thread alpha batches and sort
    for (size_t i = 0; i < workQueue->NumThreads(); ++i)
    {
        const ThreadBatchResult& result = batchResults[i];
        if (result.alphaBatches.size())
            alphaBatches.batches.insert(alphaBatches.batches.end(), result.alphaBatches.begin(), result.alphaBatches.end());
    }

    alphaBatches.Sort(alphaInstanceTransforms, SORT_DISTANCE, hasInstancing);
}

void Renderer::SortShadowBatches(ShadowMap& shadowMap)
{
    ZoneScoped;

    for (size_t i = 0; i < shadowMap.shadowViews.size(); ++i)
    {
        ShadowView& view = *shadowMap.shadowViews[i];
        LightDrawable* light = view.light;

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
    ZoneScoped;

    if (hasInstancing && transforms.size())
    {
        if (instanceVertexBuffer->NumVertices() < transforms.size())
            instanceVertexBuffer->Define(USAGE_DYNAMIC, transforms.size(), instanceVertexElements, &transforms[0]);
        else
            instanceVertexBuffer->SetData(0, transforms.size(), &transforms[0]);
    }
}

void Renderer::UpdateInstanceTransforms(size_t numVectors, const std::vector<Matrix3x4>* transforms)
{
    ZoneScoped;

    if (hasInstancing)
    {
        size_t totalTransformSize = 0;
        for (size_t i = 0; i < numVectors; ++i)
            totalTransformSize += transforms[i].size();

        if (totalTransformSize)
        { 
            if (instanceVertexBuffer->NumVertices() < totalTransformSize)
                instanceVertexBuffer->Define(USAGE_DYNAMIC, totalTransformSize, instanceVertexElements);

            size_t baseIndex = 0;
            for (size_t i = 0; i < numVectors; ++i)
            {
                if (transforms[i].size())
                {
                    instanceVertexBuffer->SetData(baseIndex, transforms[i].size(), &transforms[i][0]);
                    baseIndex += transforms[i].size();
                }
            }
        }
    }
}

void Renderer::UpdateLightData()
{
    ZoneScoped;

#ifndef GL_ES_VERSION_3_0
    ImageLevel clusterLevel(IntVector3(NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), FMT_RGBA32U, clusterData);
#else
    ImageLevel clusterLevel(IntVector3(NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), FMT_RGBA8, clusterData);
#endif
    clusterTexture->SetData(0, IntBox(0, 0, 0, NUM_CLUSTER_X, NUM_CLUSTER_Y, NUM_CLUSTER_Z), clusterLevel);
    lightDataBuffer->SetData(0, (lights.size() + 1) * sizeof(LightData), lightData);
}

void Renderer::RenderBatches(Camera* camera_, const BatchQueue& queue, unsigned baseInstanceIndex)
{
    ZoneScoped;

    lastMaterial = nullptr;
    lastPass = nullptr;

    if (camera_ != lastCamera)
    {
        float nearClip = camera->NearClip();
        float farClip = camera->FarClip();

        perViewData.projectionMatrix = camera_->ProjectionMatrix();
        perViewData.viewMatrix = camera_->ViewMatrix();
        perViewData.viewProjMatrix = perViewData.projectionMatrix * perViewData.viewMatrix;
        perViewData.depthParameters = Vector4(nearClip, farClip, camera_->IsOrthographic() ? 0.5f : 0.0f, camera_->IsOrthographic() ? 0.5f : 1.0f / farClip);
        perViewData.cameraPosition = Vector4(camera_->WorldPosition(), 1.0f);

        size_t dataSize = sizeof(PerViewUniforms);

        // Set global lighting settings if is the main view
        if (camera_ == camera)
        {
            perViewData.ambientColor = lightEnvironment ? lightEnvironment->AmbientColor() : DEFAULT_AMBIENT_COLOR;
            perViewData.fogColor = lightEnvironment ? lightEnvironment->FogColor() : DEFAULT_FOG_COLOR;

            float fogStart = lightEnvironment ? lightEnvironment->FogStart() : DEFAULT_FOG_START;
            float fogEnd = lightEnvironment ? lightEnvironment->FogEnd() : DEFAULT_FOG_END;
            float fogRange = Max(fogEnd - fogStart, M_EPSILON);
            perViewData.fogParameters = Vector4(fogEnd / farClip, farClip / fogRange, 0.0f, 0.0f);
        }

        // Set directional light data if exists and is the main view
        if (!dirLight || camera_ != camera)
        {
            perViewData.dirLightDirection = Vector4::ZERO;
            perViewData.dirLightColor = Color::BLACK;
            perViewData.dirLightShadowParameters = Vector4::ONE;
            dataSize -= 2 * sizeof(Matrix4); // Leave out shadow matrices
        }
        else
        {
            perViewData.dirLightDirection = Vector4(-dirLight->WorldDirection(), 0.0f);
            perViewData.dirLightColor = dirLight->GetColor();

            if (dirLight->ShadowMap())
            {
                Vector2 cascadeSplits = dirLight->ShadowCascadeSplits();
                float firstSplit = cascadeSplits.x / farClip;
                float secondSplit = cascadeSplits.y / farClip;

                perViewData.dirLightShadowSplits = Vector4(firstSplit, secondSplit, dirLight->ShadowFadeStart() * secondSplit, 1.0f / (secondSplit - dirLight->ShadowFadeStart() * secondSplit));
                perViewData.dirLightShadowParameters = dirLight->ShadowParameters();
                if (dirLight->ShadowViews().size() >= 2)
                {
                    perViewData.dirLightShadowMatrices[0] = dirLight->ShadowViews()[0].shadowMatrix;
                    perViewData.dirLightShadowMatrices[1]  = dirLight->ShadowViews()[1].shadowMatrix;
                }
            }
            else
            {
                perViewData.dirLightShadowParameters = Vector4::ONE;
                dataSize -= 2 * sizeof(Matrix4); // Leave out shadow matrices
            }
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

                UniformBuffer* materialUniforms = material->GetUniformBuffer();
                if (materialUniforms)
                    materialUniforms->Bind(UB_MATERIALDATA);

                lastMaterial = material;
            }

            CullMode cullMode = material->GetCullMode();
            if (camera_->UseReverseCulling())
            {
                if (cullMode == CULL_BACK)
                    cullMode = CULL_FRONT;
                else if (cullMode == CULL_FRONT)
                    cullMode = CULL_BACK;
            }

            graphics->SetRenderState(batch.pass->GetBlendMode(), cullMode, batch.pass->GetDepthTest(), batch.pass->GetColorWrite(), batch.pass->GetDepthWrite());

            lastPass = batch.pass;
        }

        Geometry* geometry = batch.geometry;
        VertexBuffer* vb = geometry->vertexBuffer;
        IndexBuffer* ib = geometry->indexBuffer;
        vb->Bind(program->Attributes());
        if (ib)
            ib->Bind();

        if (geometryBits == GEOM_INSTANCED)
        {
            if (ib)
                graphics->DrawIndexedInstanced(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount, instanceVertexBuffer, batch.instanceStart + baseInstanceIndex, batch.instanceCount);
            else
                graphics->DrawInstanced(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount, instanceVertexBuffer, batch.instanceStart + baseInstanceIndex, batch.instanceCount);

            it += batch.instanceCount - 1;
        }
        else
        {
            if (!geometryBits)
                graphics->SetUniform(program, U_WORLDMATRIX, *batch.worldTransform);
            else
                batch.drawable->OnRender(program, batch.geomIndex);

            if (ib)
                graphics->DrawIndexed(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount);
            else
                graphics->Draw(PT_TRIANGLE_LIST, geometry->drawStart, geometry->drawCount);
        }
    }
}

void Renderer::CheckOcclusionQueries()
{
    static std::vector<OcclusionQueryResult> results;
    results.clear();
    graphics->CheckOcclusionQueryResults(results);

    {
        ZoneScopedN("PropagateVisibility");

        for (auto it = results.begin(); it != results.end(); ++it)
        {
            Octant* octant = static_cast<Octant*>(it->object);
            // If the octant was destroyed in the meanwhile, it would be nulled in the pending query
            if (octant)
                octant->OnOcclusionQueryResult(it->visible);
        }
    }
}

void Renderer::RenderOcclusionQueries()
{
    ZoneScoped;

    if (!boundingBoxShaderProgram)
        return;

    Matrix3x4 boxMatrix(Matrix3x4::IDENTITY);
    float nearClip = camera->NearClip();

    // Use camera's motion since last frame to enlarge the bounding boxes. Use multiplied movement speed to account for latency in query results
    Vector3 cameraPosition = camera->WorldPosition();
    Vector3 cameraMove = cameraPosition - previousCameraPosition;
    Vector3 enlargement = (OCCLUSION_MARGIN + 4.0f * cameraMove.Length()) * Vector3::ONE;

    boundingBoxVertexBuffer->Bind(MASK_POSITION);
    boundingBoxIndexBuffer->Bind();
    boundingBoxShaderProgram->Bind();
    graphics->SetRenderState(BLEND_REPLACE, CULL_BACK, CMP_LESS_EQUAL, false, false);

    for (size_t i = 0; i < NUM_OCTANT_TASKS; ++i)
    {
        for (auto it = octantResults[i].occlusionQueries.begin(); it != octantResults[i].occlusionQueries.end(); ++it)
        {
            Octant* octant = *it;

            const BoundingBox& octantBox = octant->CullingBox();
            BoundingBox box(octantBox.min - enlargement, octantBox.max + enlargement);

            // If bounding box could be clipped by near plane, assume visible without performing query
            if (box.Distance(cameraPosition) < 2.0f * nearClip)
            {
                octant->OnOcclusionQueryResult(true);
                continue;
            }

            Vector3 size = box.HalfSize();
            Vector3 center = box.Center();

            boxMatrix.m00 = size.x;
            boxMatrix.m11 = size.y;
            boxMatrix.m22 = size.z;
            boxMatrix.m03 = center.x;
            boxMatrix.m13 = center.y;
            boxMatrix.m23 = center.z;

            graphics->SetUniform(boundingBoxShaderProgram, U_WORLDMATRIX, boxMatrix);

            unsigned queryId = graphics->BeginOcclusionQuery(octant);
            graphics->DrawIndexed(PT_TRIANGLE_LIST, 0, NUM_BOX_INDICES);
            graphics->EndOcclusionQuery();

            // Remember query in octant to not re-test it until result arrives
            octant->OnOcclusionQuery(queryId);
        }
    }

    previousCameraPosition = cameraPosition;
}

void Renderer::DefineFaceSelectionTextures()
{
    // Face selection textures do not depend on shadow map size. No-op if already defined
    if (faceSelectionTexture1 && faceSelectionTexture2)
        return;

    faceSelectionTexture1 = new Texture();
    faceSelectionTexture2 = new Texture();

    static const float faceSelectionData1[] = 
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    static const float faceSelectionData2[] = 
    {
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

void Renderer::DefineBoundingBoxGeometry()
{
    static const float boxVertexData[] = {
        -1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f
    };

    std::vector<VertexElement> vertexDeclaration;
    vertexDeclaration.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
    boundingBoxVertexBuffer = new VertexBuffer();
    boundingBoxVertexBuffer->Define(USAGE_DEFAULT, 8, vertexDeclaration, boxVertexData);

    static const unsigned short boxIndexData[] = {
        0, 2, 1,
        2, 3, 1,
        2, 4, 3,
        4, 5, 3,
        4, 6, 5,
        6, 7, 5,
        6, 0, 7,
        0, 1, 7,
        4, 2, 0,
        6, 4, 0,
        1, 3, 5,
        1, 5, 7
    };

    boundingBoxIndexBuffer = new IndexBuffer();
    boundingBoxIndexBuffer->Define(USAGE_DEFAULT, NUM_BOX_INDICES, sizeof(unsigned short), boxIndexData);

    boundingBoxShaderProgram = graphics->CreateProgram("Shaders/BoundingBox.glsl");
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
                    Frustum& clusterFrustum = clusterCullData[idx].frustum;
                    BoundingBox& clusterBox = clusterCullData[idx].boundingBox;

                    clusterFrustum.vertices[0] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * y, near);
                    clusterFrustum.vertices[1] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * (y + 1), near);
                    clusterFrustum.vertices[2] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * (y + 1), near);
                    clusterFrustum.vertices[3] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * y, near);
                    clusterFrustum.vertices[4] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * y, far);
                    clusterFrustum.vertices[5] = cameraProjInverse * Vector3(-1.0f + xStep * (x + 1), 1.0f - yStep * (y + 1), far);
                    clusterFrustum.vertices[6] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * (y + 1), far);
                    clusterFrustum.vertices[7] = cameraProjInverse * Vector3(-1.0f + xStep * x, 1.0f - yStep * y, far);
                    clusterFrustum.UpdatePlanes();
                    clusterBox.Define(clusterFrustum);
                    ++idx;
                }
            }
        }

        lastClusterFrustumProj = cameraProj;
        clusterFrustumsDirty = false;
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
    Bone::RegisterObject();
    Camera::RegisterObject();
    OctreeNode::RegisterObject();
    GeometryNode::RegisterObject();
    StaticModel::RegisterObject();
    AnimatedModel::RegisterObject();
    Light::RegisterObject();
    LightEnvironment::RegisterObject();
    Material::RegisterObject();
    Model::RegisterObject();
    Animation::RegisterObject();

    registered = true;
}
