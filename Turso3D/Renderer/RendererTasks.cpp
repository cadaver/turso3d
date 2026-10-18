// For conditions of distribution and use, seecopyright notice in License.txt

#include "Camera.h"
#include "GeometryNode.h"
#include "Light.h"
#include "Material.h"
#include "Octree.h"
#include "Renderer.h"
#include "RendererTasks.h"

#include <algorithm>
#include <tracy/Tracy.hpp>

static inline bool CompareDrawableDistances(Drawable* lhs, Drawable* rhs)
{
    return lhs->Distance() < rhs->Distance();
}

void CollectOctantsTask::Complete(unsigned /*threadIndex*/)
{
    ZoneScoped;

    // Go through octants in this task's octree branch
    Octant* octant = startOctant;
    ThreadOctantResult& result = renderer->octantResults[resultIdx];

    renderer->CollectOctantsAndLights(octant, result);

    // Queue final batch task for leftover nodes if needed
    if (result.drawableAcc)
    {
        if (result.collectBatchesTasks.size() <= result.batchTaskIdx)
            result.collectBatchesTasks.push_back(new CollectBatchesTask(renderer));

        CollectBatchesTask* batchTask = result.collectBatchesTasks[result.batchTaskIdx];
        batchTask->renderer = renderer;
        batchTask->octants.clear();
        batchTask->octants.insert(batchTask->octants.end(), result.octants.begin() + result.taskOctantIdx, result.octants.end());
        renderer->numPendingBatchTasks.fetch_add(1);
        renderer->workQueue->QueueTask(batchTask);
    }

    if (renderer->numPendingBatchTasks.fetch_sub(1) == 1)
        renderer->SortMainBatches();
}

void CollectBatchesTask::Complete(unsigned threadIndex)
{
    ZoneScoped;

    ThreadBatchResult& result = renderer->batchResults[threadIndex];
    const Matrix3x4& viewMatrix = renderer->camera->ViewMatrix();
    Vector3 viewZ = Vector3(viewMatrix.m20, viewMatrix.m21, viewMatrix.m22);
    Vector3 absViewZ = viewZ.Abs();
    // Use last scene depth range for coarse depth slicing this frame
    // Note: this is not the same as depth slicing of the light clusters, but it doesn't have to be
    float invLastZRange = 1.0f / (renderer->lastMaxZ - renderer->lastMinZ) * NUM_OPAQUE_Z_SPLITS;

    // Scan octants for geometries
    for (auto it = octants.begin(); it != octants.end(); ++it)
    {
        Octant* octant = it->first;
        unsigned char planeMask = it->second;
        const std::vector<Drawable*>& drawables = octant->Drawables();

        for (auto dIt = drawables.begin(); dIt != drawables.end(); ++dIt)
        {
            Drawable* drawable = *dIt;

            if (drawable->TestFlag(DF_GEOMETRY) && (drawable->ViewLayer() & renderer->viewMask))
            {
                const BoundingBox& geometryBox = drawable->WorldBoundingBox();

                // Note: to strike a balance between performance and occlusion accuracy, per-geometry occlusion tests are skipped for now,
                // as octants are already tested with combined actual drawable bounds
                if ((!planeMask || renderer->frustum.IsInsideMaskedFast(geometryBox, planeMask)) && drawable->OnPrepareRender(renderer->frameNumber, renderer->camera))
                {
                    // Determine visible geometries' min & max depth value and combined bounding box
                    Vector3 center = geometryBox.Center();
                    Vector3 edge = geometryBox.Size() * 0.5f;
                    float viewCenterZ = viewZ.DotProduct(center) + viewMatrix.m23;
                    float viewEdgeZ = absViewZ.DotProduct(edge);
                    float viewMinZ = viewCenterZ - viewEdgeZ;
                    float viewMaxZ = viewCenterZ + viewEdgeZ;

                    result.minZ = Min(result.minZ, viewMinZ);
                    result.maxZ = Max(result.maxZ, viewMaxZ);
                    result.geometryBounds.Merge(geometryBox);

                    Batch newBatch;
                    newBatch.programBits = (unsigned char)(drawable->Flags() & DF_GEOMETRY_TYPE_BITS);

                    if (!newBatch.programBits)
                        newBatch.worldTransform = &drawable->WorldTransform();
                    else
                    {
                        newBatch.geomIndex = 0;
                        newBatch.drawable = static_cast<GeometryDrawable*>(drawable);
                    }

                    const SourceBatches& batches = static_cast<GeometryDrawable*>(drawable)->Batches();
                    size_t numGeometries = batches.NumGeometries();
                    int zIndex = Clamp((int)((viewMinZ - renderer->lastMinZ) * invLastZRange), 0, NUM_OPAQUE_Z_SPLITS - 1);

                    if (numGeometries == 1)
                    {
                        Material* material = batches.GetMaterial(0);

                        // Assume opaque first
                        newBatch.pass = material->GetPass(PASS_OPAQUE);
                        newBatch.geometry = batches.GetGeometry(0);

                        if (newBatch.pass)
                        {
                            newBatch.SetStateSortKey();

                            // Put opaque batch into its coarse depth slice
                            result.opaqueBatches[zIndex].push_back(newBatch);
                        }
                        else
                        {
                            // If not opaque, try transparent
                            newBatch.pass = material->GetPass(PASS_ALPHA);
                            if (newBatch.pass)
                            {
                                newBatch.distance = drawable->Distance();
                                result.alphaBatches.push_back(newBatch);
                            }
                        }
                    }
                    else
                    {
                        for (size_t j = 0; j < numGeometries; ++j)
                        {
                            Material* material = batches.GetMaterial(j);

                            // Assume opaque first
                            newBatch.pass = material->GetPass(PASS_OPAQUE);
                            newBatch.geometry = batches.GetGeometry(j);

                            // Update geometry index as necessary
                            if (newBatch.programBits)
                                newBatch.geomIndex = (unsigned char)j;

                            if (newBatch.pass)
                            {
                                newBatch.SetStateSortKey();

                                // Put opaque batch into its coarse depth slice
                                result.opaqueBatches[zIndex].push_back(newBatch);
                            }
                            else
                            {
                                // If not opaque, try transparent
                                newBatch.pass = material->GetPass(PASS_ALPHA);
                                if (newBatch.pass)
                                {
                                    newBatch.distance = drawable->Distance();
                                    result.alphaBatches.push_back(newBatch);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (renderer->numPendingBatchTasks.fetch_sub(1) == 1)
        renderer->SortMainBatches();
}

void ProcessLightsTask::Complete(unsigned /*threadIndex*/)
{
    ZoneScoped;

    std::vector<LightDrawable*>& lights = renderer->lights;

    // Merge the light collection per-thread results; check that each light is valid. Also find the directional light
    for (size_t i = 0; i < renderer->rootLevelOctants.size(); ++i)
    {
        const ThreadOctantResult& result = renderer->octantResults[i];

        for (auto it = result.lights.begin(); it != result.lights.end(); ++it)
        {
            LightDrawable* light = *it;
            if (light->OnPrepareRender(renderer->frameNumber, renderer->camera))
            {
                if (light->GetLightType() == LIGHT_DIRECTIONAL)
                {
                    if (!renderer->dirLight || light->GetColor().Average() > renderer->dirLight->GetColor().Average())
                        renderer->dirLight = light;
                }
                else
                    lights.push_back(light);
            }
        }
    }

    // Sort localized lights by increasing distance
    std::sort(lights.begin(), lights.end(), CompareDrawableDistances);

    // Clamp to maximum supported
    if (lights.size() > MAX_LIGHTS)
        lights.resize(MAX_LIGHTS);

    // Clear per-cluster light data from previous frame, update cluster frustums and bounding boxes if camera changed, then queue light culling tasks
    renderer->DefineClusterFrustums();
    memset(renderer->clusterData, 0, MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z);
    renderer->workQueue->QueueTasks(NUM_CLUSTER_Z, reinterpret_cast<Task**>(&renderer->cullLightsTasks[0]));

    // Pre-step for shadow map caching: reallocate all lights' shadow map rectangles which are non-zero at this point.
    // If shadow maps were dirtied (size or bias change) reset all allocations instead
    for (auto it = lights.begin(); it != lights.end(); ++it)
    {
        LightDrawable* light = *it;
        if (renderer->shadowMapsDirty)
            light->SetShadowMap(nullptr);
        else if (renderer->drawShadows && light->ShadowStrength() < 1.0f && light->ShadowRect() != IntRect::ZERO)
            renderer->AllocateShadowMap(light);
    }

    // Check if directional light needs shadows
    if (renderer->dirLight)
    {
        if (renderer->shadowMapsDirty)
            renderer->dirLight->SetShadowMap(nullptr);

        if (!renderer->drawShadows || renderer->dirLight->ShadowStrength() >= 1.0f || !renderer->AllocateShadowMap(renderer->dirLight))
            renderer->dirLight->SetShadowMap(nullptr);
    }

    renderer->shadowMapsDirty = false;

    size_t lightTaskIdx = 0;

    // Go through lights and setup shadowcaster collection tasks
    for (size_t i = 0; i < lights.size(); ++i)
    {
        LightDrawable* light = lights[i];
        float cutoff = light->GetLightType() == LIGHT_SPOT ? cosf(light->Fov() * 0.5f * M_DEGTORAD) : 0.0f;

        // Light indexing starts at 1 as 0 is "no light"
        LightData& lightData = renderer->lightData[i + 1];
        lightData.position = Vector4(light->WorldPosition(), 1.0f);
        lightData.direction = Vector4(-light->WorldDirection(), 0.0f);
        lightData.attenuation = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f / (1.0f - cutoff), 1.0f);
        lightData.color = light->EffectiveColor();
        lightData.shadowParameters = Vector4::ONE; // Assume unshadowed

        // Check if not shadowcasting or beyond shadow range
        if (!renderer->drawShadows || light->ShadowStrength() >= 1.0f)
        {
            light->SetShadowMap(nullptr);
            continue;
        }

        // Now retry shadow map allocation if necessary. If it's a new allocation, must rerender the shadow map
        if (!light->ShadowMap())
        {
            if (!renderer->AllocateShadowMap(light))
                continue;
        }

        light->InitShadowViews();
        std::vector<ShadowView>& shadowViews = light->ShadowViews();

        // Preallocate shadowcaster list
        ShadowMap& shadowMap = renderer->shadowMaps[1];
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

        std::vector<AutoPtr<CollectShadowCastersTask> >& lightTasks = renderer->collectShadowCastersTasks;
        if (lightTasks.size() <= lightTaskIdx)
            lightTasks.push_back(new CollectShadowCastersTask(renderer));

        lightTasks[lightTaskIdx]->light = light;
        renderer->workQueue->AddDependency(renderer->processShadowCastersTask, lightTasks[lightTaskIdx]);
        ++lightTaskIdx;
    }

    if (renderer->dirLight && renderer->dirLight->ShadowMap())
    {
        ShadowMap& shadowMap = renderer->shadowMaps[0];

        renderer->dirLight->InitShadowViews();
        std::vector<ShadowView>& shadowViews = renderer->dirLight->ShadowViews();

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
        renderer->workQueue->QueueTasks(lightTaskIdx, reinterpret_cast<Task**>(&renderer->collectShadowCastersTasks[0]));
}

void CullLightsTask::Complete(unsigned /*threadIndex*/)
{
    ZoneScoped;

    // Cull lights against each cluster frustum on the given Z-level
    const Matrix3x4& cameraView = renderer->camera->ViewMatrix();
    std::vector<LightDrawable*>& lights = renderer->lights;

    // Clear old light data first
    size_t idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
    ClusterCullData* cullData = &renderer->clusterCullData[idx];
    for (size_t i = 0; i < NUM_CLUSTER_X * NUM_CLUSTER_Y; ++i)
    {
        cullData->numLights = 0;
        ++cullData;
    }

    // Go through lights and add to each affected cluster. Do culling checks both ways to reduce false positives
    for (size_t i = 0; i < lights.size(); ++i)
    {
        LightDrawable* light = lights[i];
        LightType lightType = light->GetLightType();

        if (lightType == LIGHT_POINT)
        {
            Sphere bounds(cameraView * light->WorldPosition(), light->Range());
            float minViewZ = bounds.center.z - light->Range();
            float maxViewZ = bounds.center.z + light->Range();

            idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
            cullData = &renderer->clusterCullData[idx];
            if (minViewZ > cullData->frustum.vertices[4].z || maxViewZ < cullData->frustum.vertices[0].z)
                continue;

            for (size_t y = 0; y < NUM_CLUSTER_Y; ++y)
            {
                for (size_t x = 0; x < NUM_CLUSTER_X; ++x)
                {
                    if (bounds.IsInsideFast(cullData->boundingBox) && cullData->frustum.IsInsideFast(bounds))
                    {
                        Vector3 cameraViewPos = cameraView * light->WorldPosition();
                        float distance = cullData->frustum.Distance(cameraViewPos);

                        if (cullData->numLights < MAX_LIGHTS_CLUSTER)
                        {
                            cullData->distances[cullData->numLights] = distance;
                            renderer->clusterData[idx * MAX_LIGHTS_CLUSTER + cullData->numLights++] = (unsigned char)(i + 1);
                        }
                        else
                        {
                            // If maximum lights exceeded, overwrite a light that is further away
                            for (size_t j = 0; j < MAX_LIGHTS_CLUSTER; ++j)
                            {
                                if (cullData->distances[j] > distance)
                                {
                                    cullData->distances[j] = distance;
                                    renderer->clusterData[idx * MAX_LIGHTS_CLUSTER + j] = (unsigned char)(i + 1);
                                    break;
                                }
                            }
                        }
                    }

                    ++idx;
                    ++cullData;
                }
            }
        }
        else if (lightType == LIGHT_SPOT)
        {
            Frustum bounds(light->WorldFrustum().Transformed(cameraView));
            BoundingBox boundsBox(bounds);
            float minViewZ = boundsBox.min.z;
            float maxViewZ = boundsBox.max.z;

            idx = z * NUM_CLUSTER_X * NUM_CLUSTER_Y;
            cullData = &renderer->clusterCullData[idx];
            if (minViewZ > cullData->frustum.vertices[4].z || maxViewZ < cullData->frustum.vertices[0].z)
                continue;

            for (size_t y = 0; y < NUM_CLUSTER_Y; ++y)
            {
                for (size_t x = 0; x < NUM_CLUSTER_X; ++x)
                {
                    if (bounds.IsInsideFast(cullData->boundingBox) && cullData->frustum.IsInsideFast(boundsBox))
                    {
                        Vector3 cameraViewPos = cameraView * light->WorldPosition();
                        float distance = cullData->frustum.Distance(cameraViewPos);

                        if (cullData->numLights < MAX_LIGHTS_CLUSTER)
                        {
                            cullData->distances[cullData->numLights] = distance;
                            renderer->clusterData[idx * MAX_LIGHTS_CLUSTER + cullData->numLights++] = (unsigned char)(i + 1);
                        }
                        else
                        {
                            // If maximum lights exceeded, overwrite a light that is further away
                            for (size_t j = 0; j < MAX_LIGHTS_CLUSTER; ++j)
                            {
                                if (cullData->distances[j] > distance)
                                {
                                    cullData->distances[j] = distance;
                                    renderer->clusterData[idx * MAX_LIGHTS_CLUSTER + j] = (unsigned char)(i + 1);
                                    break;
                                }
                            }
                        }
                    }

                    ++idx;
                    ++cullData;
                }
            }
        }
    }
}

void CollectShadowCastersTask::Complete(unsigned /*threadIndex*/)
{
    ZoneScoped;

    LightType lightType = light->GetLightType();
    std::vector<ShadowView>& shadowViews = light->ShadowViews();

    // Directional lights perform queries later, here only point & spot lights (in shadow atlas) are considered
    ShadowMap& shadowMap = renderer->shadowMaps[1];

    if (lightType == LIGHT_POINT)
    {
        // Point light: perform only one sphere query, then check which of the point light sides are visible
        for (size_t i = 0; i < shadowViews.size(); ++i)
        {
            // Check if each of the sides is in view. Do not process if isn't. Rendering will be no-op this frame, but cached contents are discarded once comes into view again
            light->SetupShadowView(i, renderer->camera);
            ShadowView& view = shadowViews[i];

            if (!renderer->frustum.IsInsideFast(BoundingBox(view.shadowFrustum)))
            {
                view.renderMode = RENDER_STATIC_LIGHT_CACHED;
                view.viewport = IntRect::ZERO;
                view.lastViewport = IntRect::ZERO;
            }
        }

        std::vector<Drawable*>& shadowCasters = shadowMap.shadowCasters[shadowViews[0].casterListIdx];
        renderer->octree->FindDrawables(shadowCasters, light->WorldSphere(), DF_GEOMETRY | DF_CAST_SHADOWS);
    }
    else if (lightType == LIGHT_SPOT)
    {
        // Spot light: perform query for the spot frustum
        light->SetupShadowView(0, renderer->camera);
        ShadowView& view = shadowViews[0];

        std::vector<Drawable*>& shadowCasters = shadowMap.shadowCasters[view.casterListIdx];
        renderer->octree->FindDrawablesMasked(shadowCasters, view.shadowFrustum, DF_GEOMETRY | DF_CAST_SHADOWS);
    }
}

void ProcessShadowCastersTask::Complete(unsigned /*threadIndex*/)
{
    ZoneScoped;

    // Queue shadow batch collection tasks. These will also perform shadow batch sorting tasks when done
    if (renderer->drawShadows)
    {
        size_t shadowTaskIdx = 0;
        LightDrawable* lastLight = nullptr;
        std::vector<AutoPtr<CollectShadowBatchesTask> >& tasks = renderer->collectShadowBatchesTasks;

        for (size_t i = 0; i < NUM_SHADOW_MAPS; ++i)
        {
            ShadowMap& shadowMap = renderer->shadowMaps[i];
            for (size_t j = 0; j < shadowMap.shadowViews.size(); ++j)
            {
                LightDrawable* light = shadowMap.shadowViews[j]->light;
                // For a point light, make only one task that will handle all of the views and skip rest
                if (light->GetLightType() == LIGHT_POINT && light == lastLight)
                    continue;

                lastLight = light;

                if (tasks.size() <= shadowTaskIdx)
                    tasks.push_back(new CollectShadowBatchesTask(renderer));
                tasks[shadowTaskIdx]->shadowMapIdx = i;
                tasks[shadowTaskIdx]->viewIdx = j;
                renderer->numPendingShadowViewTasks[i].fetch_add(1);
                ++shadowTaskIdx;
            }
        }

        if (shadowTaskIdx > 0)
            renderer->workQueue->QueueTasks(shadowTaskIdx, reinterpret_cast<Task**>(&renderer->collectShadowBatchesTasks[0]));
    }

    // Finally copy correct shadow matrices for the localized light data
    // Note: directional light shadow matrices may still be pending, but they are not included here
    std::vector<LightDrawable*>& lights = renderer->lights;
    for (size_t i = 0; i < lights.size(); ++i)
    {
        LightDrawable* light = lights[i];

        if (light->ShadowMap())
        {
            LightData& lightData = renderer->lightData[i + 1];
            lightData.shadowParameters = light->ShadowParameters();
            lightData.shadowMatrix = light->ShadowViews()[0].shadowMatrix;
        }
    }
}

void CollectShadowBatchesTask::Complete(unsigned /*threadIndex*/)
{
    ZoneScoped;

    ShadowMap& shadowMap = renderer->shadowMaps[shadowMapIdx];

    for (;;)
    {
        ShadowView& view = *shadowMap.shadowViews[viewIdx];

        LightDrawable* light = view.light;
        LightType lightType = light->GetLightType();

        float splitMinZ = renderer->minZ, splitMaxZ = renderer->maxZ;

        // Focus directional light shadow camera to the visible geometry combined bounds, and query for shadowcasters late
        if (lightType == LIGHT_DIRECTIONAL)
        {
            if (!light->SetupShadowView(viewIdx, renderer->camera, &renderer->geometryBounds))
                view.viewport = IntRect::ZERO;
            else
            {
                splitMinZ = Max(splitMinZ, view.splitMinZ);
                splitMaxZ = Min(splitMaxZ, view.splitMaxZ);

                // Before querying (which is potentially expensive), check for degenerate depth range or frustum outside split
                if (splitMinZ >= splitMaxZ || splitMinZ > view.splitMaxZ || splitMaxZ < view.splitMinZ)
                    view.viewport = IntRect::ZERO;
                else
                    renderer->octree->FindDrawablesMasked(shadowMap.shadowCasters[view.casterListIdx], view.shadowFrustum, DF_GEOMETRY | DF_CAST_SHADOWS);
            }
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
            const std::vector<Drawable*>& initialShadowCasters = shadowMap.shadowCasters[view.casterListIdx];

            bool dynamicOrDirLight = lightType == LIGHT_DIRECTIONAL || !light->IsStatic();
            bool dynamicCastersMoved = false;
            bool staticCastersMoved = false;

            size_t totalShadowCasters = 0;
            size_t staticShadowCasters = 0;

            Frustum lightViewFrustum = renderer->camera->WorldSplitFrustum(splitMinZ, splitMaxZ).Transformed(lightView);
            BoundingBox lightViewFrustumBox(lightViewFrustum);

            BatchQueue* destStatic = !dynamicOrDirLight ? &shadowMap.shadowBatches[view.staticQueueIdx] : nullptr;
            BatchQueue* destDynamic = &shadowMap.shadowBatches[view.dynamicQueueIdx];

            for (auto it = initialShadowCasters.begin(); it != initialShadowCasters.end(); ++it)
            {
                Drawable* drawable = *it;
                const BoundingBox& geometryBox = drawable->WorldBoundingBox();

                bool inView = drawable->InView(renderer->frameNumber);
                bool staticNode = drawable->IsStatic();

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
                    if (!drawable->OnPrepareRender(renderer->frameNumber, renderer->camera))
                        continue;
                }

                ++totalShadowCasters;

                if (staticNode)
                {
                    ++staticShadowCasters;
                    if (drawable->LastUpdateFrameNumber() == renderer->frameNumber)
                        staticCastersMoved = true;
                }
                else
                {
                    if (drawable->LastUpdateFrameNumber() == renderer->frameNumber)
                        dynamicCastersMoved = true;
                }

                // If did not allocate a static queue, just put everything to dynamic
                BatchQueue& dest = destStatic ? (staticNode ? *destStatic : *destDynamic) : *destDynamic;

                Batch newBatch;
                newBatch.programBits = (unsigned char)(drawable->Flags() & DF_GEOMETRY_TYPE_BITS);

                if (!newBatch.programBits)
                    newBatch.worldTransform = &drawable->WorldTransform();
                else
                {
                    newBatch.geomIndex = 0;
                    newBatch.drawable = static_cast<GeometryDrawable*>(drawable);
                }

                const SourceBatches& batches = static_cast<GeometryDrawable*>(drawable)->Batches();
                size_t numGeometries = batches.NumGeometries();

                if (numGeometries == 1)
                {
                    Material* material = batches.GetMaterial(0);
                    newBatch.pass = material->GetPass(PASS_SHADOW);
                    if (!newBatch.pass)
                        continue;

                    newBatch.geometry = batches.GetGeometry(0);

                    newBatch.SetStateSortKey();
                    dest.batches.push_back(newBatch);
                }
                else
                {
                    for (size_t j = 0; j < numGeometries; ++j)
                    {
                        Material* material = batches.GetMaterial(j);
                        newBatch.pass = material->GetPass(PASS_SHADOW);
                        if (!newBatch.pass)
                            continue;

                        newBatch.geometry = batches.GetGeometry(j);

                        // Update geometry index as necessary
                        if (newBatch.programBits)
                            newBatch.geomIndex = (unsigned char)j;

                        newBatch.SetStateSortKey();
                        dest.batches.push_back(newBatch);
                    }
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
        if (lightType == LIGHT_POINT && viewIdx < shadowMap.shadowViews.size() - 1 && shadowMap.shadowViews[viewIdx + 1]->light == light)
            ++viewIdx;
        else
            break;
    }

    // Sort shadow batches if was the last task
    if (renderer->numPendingShadowViewTasks[shadowMapIdx].fetch_sub(1) == 1)
        renderer->SortShadowBatches(shadowMap);
}

void SortOpaqueBatchesTask::Complete(unsigned /*threadIndex*/)
{
    ZoneScoped;

    for (size_t i = 0; i < renderer->workQueue->NumThreads(); ++i)
    {
        const ThreadBatchResult& result = renderer->batchResults[i];
        if (result.opaqueBatches[z].size())
            renderer->opaqueBatches[z].batches.insert(renderer->opaqueBatches[z].batches.end(), result.opaqueBatches[z].begin(), result.opaqueBatches[z].end());
    }

    renderer->opaqueBatches[z].Sort(renderer->opaqueInstanceTransforms[z], SORT_STATE, renderer->hasInstancing);
}
