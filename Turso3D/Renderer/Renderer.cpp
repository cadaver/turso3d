// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/FrameBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/RenderBuffer.h"
#include "../Graphics/Shader.h"
#include "../Graphics/ShaderProgram.h"
#include "../Graphics/Texture.h"
#include "../Graphics/VertexBuffer.h"
#include "../IO/Log.h"
#include "../Math/Random.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "../Thread/Profiler.h"
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
    frameNumber(0),
    sortViewNumber(0),
    lastPerViewUniforms(0),
    lastPerLightUniforms(0),
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

    shadowMapsDirty = true;
}

void Renderer::PrepareView(Scene* scene_, Camera* camera_, bool drawShadows)
{
    PROFILE(PrepareView);

    if (!scene_ || !camera_)
        return;

    scene = scene_;
    camera = camera_;
    octree = scene->FindChild<Octree>();
    if (!octree)
        return;

    if (!shadowMaps.size())
        drawShadows = false;

    // Framenumber is never 0
    ++frameNumber;
    if (!frameNumber)
        ++frameNumber;

    frustum = camera->WorldFrustum();
    viewMask = camera->ViewMask();
    geometries.clear();
    lights.clear();
    lightLists.clear();
    dirLight = nullptr;

    opaqueBatches.Clear();
    opaqueAdditiveBatches.Clear();
    alphaBatches.Clear();
    instanceTransforms.clear();
    instanceTransformsDirty = true;

    for (auto it = shadowMaps.begin(); it != shadowMaps.end(); ++it)
        it->Clear();
    usedLightLists = 0;

    CollectVisibleNodes();
    CollectLightInteractions(drawShadows);
    CollectNodeBatches();
    SortNodeBatches();
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
                    RenderBatches(view->shadowCamera, batchQueue.batches);
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
                    SetDepthBias(view->light->DepthBias(), view->light->SlopeScaleBias());
                    RenderBatches(view->shadowCamera, batchQueue.batches);
                }
            }
        }
    }

    SetDepthBias(0.0f, 0.0f);
}

void Renderer::RenderOpaque(FrameBuffer* additiveFbo)
{
    PROFILE(RenderOpaque);

    if (shadowMaps.size())
    {
        shadowMaps[0].texture->Bind(8);
        shadowMaps[1].texture->Bind(9);
        faceSelectionTexture1->Bind(10);
        faceSelectionTexture2->Bind(11);
    }

    RenderBatches(camera, opaqueBatches.batches);
    if (additiveFbo)
        additiveFbo->Bind();
    RenderBatches(camera, opaqueAdditiveBatches.batches);
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

    RenderBatches(camera, alphaBatches.batches);
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
    octree->FindNodesMasked(frustum, this, &Renderer::CollectGeometriesAndLights);
}

void Renderer::CollectLightInteractions(bool drawShadows)
{
    PROFILE(CollectLightInteractions);

    // Sort localized lights by increasing distance
    std::sort(lights.begin(), lights.end(), CompareLights);

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

    for (auto it = lights.begin(); it != lights.end(); ++it)
    {
        Light* light = *it;

        litGeometries.clear();
        bool hasReceivers = false;

        // Create a light list that contains only this light. It will be used for nodes that have no light interactions so far
        unsigned long long key = (unsigned long long)light;
        if (lightListPool.size() <= usedLightLists)
            lightListPool.push_back(new LightList());
        LightList* lightList = lightListPool[usedLightLists];
        lightLists[key] = lightList;
        ++usedLightLists;

        lightList->key = key;
        lightList->useCount = 0;
        lightList->lights.resize(1);
        lightList->lights[0] = light;

        switch (light->GetLightType())
        {
        case LIGHT_POINT:
            octree->FindNodes(reinterpret_cast<std::vector<OctreeNode*>&>(litGeometries), light->WorldSphere(), NF_ENABLED | NF_GEOMETRY);
            for (auto gIt = litGeometries.begin(), gEnd = litGeometries.end(); gIt != gEnd; ++gIt)
            {
                GeometryNode* node = *gIt;
                // Add light only to nodes which are actually inside the frustum this frame
                if (node->LastFrameNumber() == frameNumber)
                {
                    AddLightToNode(node, light, lightList);
                    hasReceivers = true;
                }
            }
            break;

        case LIGHT_SPOT:
            octree->FindNodesMasked(reinterpret_cast<std::vector<OctreeNode*>&>(litGeometries), light->WorldFrustum(), NF_ENABLED | NF_GEOMETRY);
            for (auto gIt = litGeometries.begin(), gEnd = litGeometries.end(); gIt != gEnd; ++gIt)
            {
                GeometryNode* node = *gIt;
                if (node->LastFrameNumber() == frameNumber)
                {
                    AddLightToNode(node, light, lightList);
                    hasReceivers = true;
                }
            }
            break;
        }

        if (!drawShadows || light->ShadowStrength() >= 1.0f || !hasReceivers)
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

        for (size_t i = 0; i < shadowViews.size(); ++i)
        {
            ShadowView& view = shadowViews[i];

            shadowMaps[1].shadowViews.push_back(&view);

            switch (light->GetLightType())
            {
            case LIGHT_POINT:
                // Check which lit geometries are shadow casters and inside each shadow frustum. First check whether the shadow frustum is inside the view at all
                /// \todo Could use a frustum-frustum test for more accuracy
                if (frustum.IsInsideFast(BoundingBox(view.shadowFrustum)))
                    CollectShadowBatches(shadowMaps[1], view, litGeometries, true, true);
                else
                {
                    // If not inside the view (and thus not rendered), cannot consider it cached for next frame. However shadow map render this frame is a no-op
                    view.lastViewport = IntRect::ZERO;
                    view.renderMode = RENDER_STATIC_LIGHT_CACHED;
                }
                break;

            case LIGHT_SPOT:
                // For spot light only need to check which lit geometries are shadow casters
                CollectShadowBatches(shadowMaps[1], view, litGeometries, false, true);
                break;
            }
        }
    }

    // Pre-build data for used lightlists
    for (auto it = lightLists.begin(); it != lightLists.end(); ++it)
    {
        LightList* list = it->second;
        if (!list->useCount)
            continue;

        list->lightPasses.resize((list->lights.size() + (MAX_LIGHTS_PER_PASS - 1)) / MAX_LIGHTS_PER_PASS);

        for (size_t pIdx = 0; pIdx < list->lightPasses.size(); ++pIdx)
        {
            LightPass& pass = list->lightPasses[pIdx];
            size_t lightStart = pIdx * MAX_LIGHTS_PER_PASS;
            pass.numLights = (unsigned char)Min((int)(list->lights.size() - lightStart), MAX_LIGHTS_PER_PASS);

            const unsigned char baseLightBits[] = {
                0,
                1 * SP_LIGHT,
                3 * SP_LIGHT,
                6 * SP_LIGHT,
                10 * SP_LIGHT
            };

            pass.lightBits = baseLightBits[pass.numLights];

            size_t idx = 0;

            for (size_t i = 0; i < pass.numLights; ++i)
            {
                Light* light = list->lights[lightStart + i];

                // Put shadowcasting lights first in the light pass
                if (light->ShadowMap())
                {
                    pass.lightBits += SP_LIGHT;

                    float cutoff = light->GetLightType() == LIGHT_SPOT ? cosf(light->Fov() * 0.5f * M_DEGTORAD) : 0.0f;
                    pass.lightData[idx * 9] = Vector4(light->WorldPosition(), 1.0f);
                    pass.lightData[idx * 9 + 1] = Vector4(-light->WorldDirection(), 1.0f);
                    pass.lightData[idx * 9 + 2] = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f / (1.0f - cutoff), 0.0f);
                    pass.lightData[idx * 9 + 3] = light->EffectiveColor().Data();
                    pass.lightData[idx * 9 + 4] = light->ShadowParameters();
                    *reinterpret_cast<Matrix4*>(&pass.lightData[idx * 9 + 5]) = light->ShadowViews()[0].shadowMatrix;
                    ++idx;
                }
            }

            for (size_t i = 0; i < pass.numLights; ++i)
            {
                Light* light = list->lights[lightStart + i];

                if (!light->ShadowMap())
                {
                    float cutoff = light->GetLightType() == LIGHT_SPOT ? cosf(light->Fov() * 0.5f * M_DEGTORAD) : 0.0f;
                    pass.lightData[idx * 9] = Vector4(light->WorldPosition(), 1.0f);
                    pass.lightData[idx * 9 + 1] = Vector4(-light->WorldDirection(), 1.0f);
                    pass.lightData[idx * 9 + 2] = Vector4(1.0f / Max(light->Range(), M_EPSILON), cutoff, 1.0f / (1.0f - cutoff), 0.0f);
                    pass.lightData[idx * 9 + 3] = light->EffectiveColor().Data();
                    ++idx;
                }
            }
        }
    }

    // Directional light
    if (dirLight)
    {
        if (!drawShadows || dirLight->ShadowStrength() >= 1.0f || !AllocateShadowMap(dirLight))
            dirLight->SetShadowMap(nullptr);
        else
        {
            dirLight->SetupShadowViews(camera);

            std::vector<ShadowView>& shadowViews = dirLight->ShadowViews();

            for (size_t i = 0; i < shadowViews.size(); ++i)
            {
                ShadowView& view = shadowViews[i];

                // Directional light needs a new frustum query for each split, as the shadow cameras are typically far outside the main view
                litGeometries.clear();
                octree->FindNodesMasked(reinterpret_cast<std::vector<OctreeNode*>&>(litGeometries), view.shadowFrustum, NF_ENABLED | NF_GEOMETRY | NF_CASTSHADOWS);

                shadowMaps[0].shadowViews.push_back(&view);

                CollectShadowBatches(shadowMaps[0], view, litGeometries, false, false);
            }
        }
    }

}

void Renderer::CollectShadowBatches(ShadowMap& shadowMap, ShadowView& view, const std::vector<GeometryNode*>& potentialShadowCasters, bool checkFrustum, bool checkShadowCaster)
{
    Light* light = view.light;
    Camera* shadowCamera = view.shadowCamera;
    const Frustum& shadowFrustum = view.shadowFrustum;
    BoundingBox lightViewFrustumBox(view.lightViewFrustum);
    const Matrix3x4& lightView = view.shadowCamera->ViewMatrix();

    bool dynamicOrDirLight = light->GetLightType() == LIGHT_DIRECTIONAL || !light->Static();
    bool hasDynamicCasters = false;

    shadowCasters.clear();

    for (auto it = potentialShadowCasters.begin(); it != potentialShadowCasters.end(); ++it)
    {
        GeometryNode* node = *it;
        bool dynamicNode = !node->Static();
        
        if (checkShadowCaster && !node->CastShadows())
            continue;
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

    ++sortViewNumber;
    if (!sortViewNumber)
        ++sortViewNumber;

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

    float farClipMul = 65535.0f / shadowCamera->FarClip();

    for (auto it = shadowCasters.begin(); it != shadowCasters.end(); ++it)
    {
        GeometryNode* node = *it;
        bool dynamicNode = !node->Static();

        // Skip static casters unless is a dynamic light render, or going to store them to the static map
        if (!dynamicNode && view.renderMode > RENDER_STATIC_LIGHT_STORE_STATIC)
            continue;

        // Avoid unnecessary splitting into dynamic and static objects
        BatchQueue& dest = destStatic ? (node->Static() ? *destStatic : *destDynamic) : *destDynamic;

        bool staticGeom = node->GetGeometryType() == GEOM_STATIC;
        unsigned short distance = (unsigned short)(shadowCamera->Distance(node->WorldPosition()) * farClipMul);
        const SourceBatches& batches = node->Batches();
        size_t numGeometries = batches.NumGeometries();

        for (size_t i = 0; i < numGeometries; ++i)
        {
            Geometry* geometry = batches.GetGeometry(i);
            Material* material = batches.GetMaterial(i);
            Pass* pass = material->GetPass(PASS_SHADOW);
            if (!pass)
                continue;

            Batch newBatch;

            if (staticGeom)
                newBatch.worldTransform = &node->WorldTransform();
            else
                newBatch.node = node;

            newBatch.lightPass = nullptr;
            newBatch.pass = pass;
            newBatch.geometry = geometry;
            newBatch.programBits = (unsigned char)node->GetGeometryType();

            // Perform distance sort for pass / geometry in addition to state sort
            if (pass->lastSortKey.first != sortViewNumber || pass->lastSortKey.second > distance)
            {
                pass->lastSortKey.first = sortViewNumber;
                pass->lastSortKey.second = distance;
            }
            if (geometry->lastSortKey.first != sortViewNumber || geometry->lastSortKey.second > distance)
            {
                geometry->lastSortKey.first = sortViewNumber;
                geometry->lastSortKey.second = distance;
            }
            if (geometry->useCombined)
            {
                VertexBuffer* vb = geometry->vertexBuffer.Get();
                if (vb->lastSortKey.first != sortViewNumber || vb->lastSortKey.second > distance)
                {
                    vb->lastSortKey.first = sortViewNumber;
                    vb->lastSortKey.second = distance;
                }
            }

            dest.batches.push_back(newBatch);
        }
    }

    // Sort immediately so that another view can rewrite the combined distance / state sort keys
    if (destStatic)
        destStatic->Sort(instanceTransforms, true, hasInstancing);
    
    destDynamic->Sort(instanceTransforms, true, hasInstancing);
}

void Renderer::CollectNodeBatches()
{
    PROFILE(CollectNodeBatches);

    ++sortViewNumber;
    if (!sortViewNumber)
        ++sortViewNumber;

    float farClipMul = 65535.0f / camera->FarClip();

    bool hasShadowDirLight = dirLight && dirLight->ShadowMap();

    for (auto it = geometries.begin(); it != geometries.end(); ++it)
    {
        GeometryNode* node = *it;
        unsigned char geomType = (unsigned char)node->GetGeometryType();
        unsigned short distance = (unsigned short)(node->Distance() * farClipMul);
        const SourceBatches& batches = node->Batches();
        size_t numGeometries = batches.NumGeometries();

        for (size_t i = 0; i < numGeometries; ++i)
        {
            Geometry* geometry = batches.GetGeometry(i);
            Material* material = batches.GetMaterial(i);
            bool lit = material->Lit();
            LightList* lightList = lit ? node->GetLightList() : nullptr;

            PassType type = PASS_OPAQUE;
            Pass* pass = material->GetPass(type);

            if (!pass)
            {
                type = PASS_ALPHA;
                pass = material->GetPass(type);
                if (!pass)
                    continue;
            }

            Batch newBatch;

            if (!geomType)
                newBatch.worldTransform = &node->WorldTransform();
            else
                newBatch.node = node;

            newBatch.lightPass = lightList ? &lightList->lightPasses[0] : nullptr;
            newBatch.programBits = geomType | (newBatch.lightPass ? newBatch.lightPass->lightBits : 0);
            if (lit && hasShadowDirLight)
                newBatch.programBits += SP_SHADOWDIRLIGHT;
            newBatch.pass = pass;
            newBatch.geometry = geometry;

            if (geometry->useCombined)
            {
                VertexBuffer* vb = geometry->vertexBuffer.Get();
                if (vb->lastSortKey.first != sortViewNumber || vb->lastSortKey.second > distance)
                {
                    vb->lastSortKey.first = sortViewNumber;
                    vb->lastSortKey.second = distance;
                }
            }

            if (type == PASS_OPAQUE)
            {
                // Perform distance sort for pass / geometry / lightpass in addition to state sort
                if (newBatch.lightPass && (newBatch.lightPass->lastSortKey.first != sortViewNumber || newBatch.lightPass->lastSortKey.second > distance))
                {
                    newBatch.lightPass->lastSortKey.first = sortViewNumber;
                    newBatch.lightPass->lastSortKey.second = distance;
                }
                if (pass->lastSortKey.first != sortViewNumber || pass->lastSortKey.second > distance)
                {
                    pass->lastSortKey.first = sortViewNumber;
                    pass->lastSortKey.second = distance;
                }
                if (geometry->lastSortKey.first != sortViewNumber || geometry->lastSortKey.second > distance)
                {
                    geometry->lastSortKey.first = sortViewNumber;
                    geometry->lastSortKey.second = distance;
                }

                opaqueBatches.batches.push_back(newBatch);

                // Add additive passes if necessary
                if (lightList && lightList->lightPasses.size() > 1)
                {
                    pass = material->GetPass(PASS_OPAQUEADD);

                    if (pass)
                    {
                        if (pass->lastSortKey.first != sortViewNumber || pass->lastSortKey.second > distance)
                        {
                            pass->lastSortKey.first = sortViewNumber;
                            pass->lastSortKey.second = distance;
                        }

                        for (size_t idx = 1; idx < lightList->lightPasses.size(); ++idx)
                        {
                            newBatch.lightPass = &lightList->lightPasses[idx];
                            newBatch.programBits = geomType | newBatch.lightPass->lightBits;
                            newBatch.pass = pass;

                            if (newBatch.lightPass->lastSortKey.first != sortViewNumber || newBatch.lightPass->lastSortKey.second > distance)
                            {
                                newBatch.lightPass->lastSortKey.first = sortViewNumber;
                                newBatch.lightPass->lastSortKey.second = distance;
                            }

                            opaqueAdditiveBatches.batches.push_back(newBatch);
                        }
                    }
                }
            }
            else
            {
                newBatch.distance = node->Distance();
                alphaBatches.batches.push_back(newBatch);

                // Add additive passes if necessary
                if (lightList && lightList->lightPasses.size() > 1)
                {
                    pass = material->GetPass(PASS_ALPHAADD);

                    if (pass)
                    {
                        for (size_t idx = 1; idx < lightList->lightPasses.size(); ++idx)
                        {
                            newBatch.programBits = geomType | newBatch.lightPass->lightBits;
                            newBatch.lightPass = &lightList->lightPasses[idx];
                            newBatch.pass = pass;
                            // Each successive additive batch a little closer for correct order
                            newBatch.distance -= 0.00001f;

                            alphaBatches.batches.push_back(newBatch);
                        }
                    }
                }
            }
        }
    }
}

void Renderer::SortNodeBatches()
{
    PROFILE(SortNodeBatches);

    opaqueBatches.Sort(instanceTransforms, true, hasInstancing);
    opaqueAdditiveBatches.Sort(instanceTransforms, true, hasInstancing);
    alphaBatches.Sort(instanceTransforms, false, hasInstancing);
}

void Renderer::RenderBatches(Camera* camera_, const std::vector<Batch>& batches)
{
    lastCamera = nullptr;
    lastLightPass = nullptr;
    lastMaterial = nullptr;
    lastPass = nullptr;

    if (hasInstancing && instanceTransformsDirty && instanceTransforms.size())
    {
        if (instanceVertexBuffer->NumVertices() < instanceTransforms.size())
            instanceVertexBuffer->Define(USAGE_DYNAMIC, instanceTransforms.size(), instanceVertexElements, &instanceTransforms[0]);
        else
            instanceVertexBuffer->SetData(0, instanceTransforms.size(), &instanceTransforms[0]);
        instanceTransformsDirty = false;
    }

    if (camera_ != lastCamera)
    {
        lastCamera = camera_;
        ++lastPerViewUniforms;
        if (!lastPerViewUniforms)
            ++lastPerViewUniforms;
    }

    for (auto it = batches.begin(); it != batches.end(); ++it)
    {
        const Batch& batch = *it;

        ShaderProgram* program = batch.pass->GetShaderProgram(batch.programBits);
        if (!program->Bind())
            continue;

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
                Vector4 depthParameters(camera->NearClip(), camera->FarClip(), 0.0f, 0.0f);
                if (camera_->IsOrthographic())
                {
                    depthParameters.z = 0.5f;
                    depthParameters.w = 0.5f;
                }
                else
                    depthParameters.w = 1.0f / camera->FarClip();

                glUniform4fv(location, 1, depthParameters.Data());
            }
            location = program->Uniform(U_DIRLIGHTDATA);
            if (location >= 0)
            {
                Vector4 dirLightData[12];

                if (!dirLight)
                {
                    dirLightData[0] = Vector4::ZERO;
                    dirLightData[1] = Vector4::ZERO;
                    glUniform4fv(location, 2, dirLightData[0].Data());
                }
                else
                {
                    dirLightData[0] = Vector4(-dirLight->WorldDirection(), 0.0f);
                    dirLightData[1] = dirLight->GetColor().Data();
                    
                    if (dirLight->ShadowMap())
                    {
                        float farClip = camera->FarClip();
                        float firstSplit = dirLight->ShadowSplit(0) / farClip;
                        float secondSplit = dirLight->ShadowSplit(1) / farClip;

                        dirLightData[2] = Vector4(firstSplit, secondSplit, dirLight->ShadowFadeStart() * secondSplit, 1.0f / (secondSplit - dirLight->ShadowFadeStart() * secondSplit));
                        dirLightData[3] = dirLight->ShadowParameters();
                        if (dirLight->ShadowViews().size() >= 2)
                        {
                            *reinterpret_cast<Matrix4*>(&dirLightData[4]) = dirLight->ShadowViews()[0].shadowMatrix;;
                            *reinterpret_cast<Matrix4*>(&dirLightData[8]) = dirLight->ShadowViews()[1].shadowMatrix;;
                        }
                        
                        glUniform4fv(location, 12, dirLightData[0].Data());
                    }
                    else
                        glUniform4fv(location, 2, dirLightData[0].Data());
                }
            }

            program->lastPerViewUniforms = lastPerViewUniforms;
        }

        if (batch.lightPass)
        {
            if (batch.lightPass != lastLightPass)
            {
                lastLightPass = batch.lightPass;
                ++lastPerLightUniforms;
                if (!lastPerLightUniforms)
                    ++lastPerLightUniforms;
            }

            if (program->lastPerLightUniforms != lastPerLightUniforms)
            {
                int location = program->Uniform(U_LIGHTDATA);
                if (location >= 0)
                    glUniform4fv(location, batch.lightPass->numLights * 9, batch.lightPass->lightData[0].Data());

                program->lastPerLightUniforms = lastPerLightUniforms;
            }
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
        unsigned char geometryBits = batch.programBits & SP_GEOMETRYBITS;

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
            glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(batch.instanceRange[0] * instanceVertexSize));
            glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(batch.instanceRange[0] * instanceVertexSize + sizeof(Vector4)));
            glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(batch.instanceRange[0] * instanceVertexSize + 2 * sizeof(Vector4)));

            VertexBuffer* vb = geometry->vertexBuffer;
            IndexBuffer* ib = geometry->indexBuffer;
            vb->Bind(program->Attributes());
            if (ib)
                ib->Bind();

            glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)geometry->drawCount, ib->IndexSize() == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, 
                (const void*)(geometry->drawStart* ib->IndexSize()), batch.instanceRange[1]);
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

void Renderer::CollectGeometriesAndLights(std::vector<OctreeNode*>::const_iterator begin, std::vector<OctreeNode*>::const_iterator end, unsigned char planeMask)
{
    for (auto it = begin; it != end; ++it)
    {
        OctreeNode* node = *it;
        unsigned short flags = node->Flags();
        if ((flags & NF_ENABLED) && (flags & (NF_GEOMETRY | NF_LIGHT)) && (node->LayerMask() & viewMask) && (planeMask == 0x3f || frustum.IsInsideMaskedFast(node->WorldBoundingBox(), planeMask)))
        {
            if (flags & NF_GEOMETRY)
            {
                GeometryNode* geometry = static_cast<GeometryNode*>(node);
                if (geometry->OnPrepareRender(frameNumber, camera))
                    geometries.push_back(geometry);
            }
            else
            {
                Light* light = static_cast<Light*>(node);
                if (light->OnPrepareRender(frameNumber, camera))
                {
                    if (light->GetLightType() != LIGHT_DIRECTIONAL)
                        lights.push_back(light);
                    else if (!dirLight || light->GetColor().Average() > dirLight->GetColor().Average())
                        dirLight = light;
                }
            }
        }
    }
}

void Renderer::AddLightToNode(GeometryNode* node, Light* light, LightList* lightList)
{
    LightList* oldList = node->GetLightList();

    if (!oldList)
    {
        // First light assigned on this frame
        node->SetLightList(lightList);
        ++lightList->useCount;
    }
    else
    {
        // Create new light list based on the node's existing one
        --oldList->useCount;
        unsigned long long newListKey = oldList->key;
        newListKey += (unsigned long long)light << ((oldList->lights.size() & 3) * 16);
        auto it = lightLists.find(newListKey);
        if (it != lightLists.end())
        {
            LightList* newList = it->second;
            ++newList->useCount;
            node->SetLightList(newList);
        }
        else
        {
            if (lightListPool.size() <= usedLightLists)
                lightListPool.push_back(new LightList());
            LightList* newList = lightListPool[usedLightLists];
            lightLists[newListKey] = newList;
            ++usedLightLists;

            newList->key = newListKey;
            newList->useCount = 1;
            newList->lights = oldList->lights;
            newList->lights.push_back(light);
            node->SetLightList(newList);
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
