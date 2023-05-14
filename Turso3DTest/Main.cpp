#include "Graphics/FrameBuffer.h"
#include "Graphics/Graphics.h"
#include "Graphics/Texture.h"
#include "Input/Input.h"
#include "IO/Arguments.h"
#include "IO/FileSystem.h"
#include "IO/Log.h"
#include "IO/StringUtils.h"
#include "Math/Math.h"
#include "Math/Random.h"
#include "Renderer/AnimatedModel.h"
#include "Renderer/Animation.h"
#include "Renderer/AnimationState.h"
#include "Renderer/Camera.h"
#include "Renderer/DebugRenderer.h"
#include "Renderer/Light.h"
#include "Renderer/LightEnvironment.h"
#include "Renderer/Material.h"
#include "Renderer/Model.h"
#include "Renderer/Octree.h"
#include "Renderer/Renderer.h"
#include "Resource/ResourceCache.h"
#include "Renderer/StaticModel.h"
#include "Scene/Scene.h"
#include "Time/Timer.h"
#include "Time/Profiler.h"
#include "Thread/ThreadUtils.h"

#include <SDL.h>
#include <tracy/Tracy.hpp>

std::vector<StaticModel*> rotatingObjects;
std::vector<AnimatedModel*> animatingObjects;

void CreateScene(Scene* scene, Camera* camera, int preset)
{
    rotatingObjects.clear();
    animatingObjects.clear();

    ResourceCache* cache = Object::Subsystem<ResourceCache>();

    scene->Clear();
    scene->CreateChild<Octree>();
    LightEnvironment* lightEnvironment = scene->CreateChild<LightEnvironment>();

    SetRandomSeed(1);

    // Preset 0: occluders, static meshes and many local shadowcasting lights in addition to ambient light
    if (preset == 0)
    {
        lightEnvironment->SetAmbientColor(Color(0.3f, 0.3f, 0.3f));
        camera->SetFarClip(1000.0f);

        for (int y = -55; y <= 55; ++y)
        {
            for (int x = -55; x <= 55; ++x)
            {
                StaticModel* object = scene->CreateChild<StaticModel>();
                object->SetStatic(true);
                object->SetPosition(Vector3(10.5f * x, -0.05f, 10.5f * y));
                object->SetScale(Vector3(10.0f, 0.1f, 10.0f));
                object->SetModel(cache->LoadResource<Model>("Box.mdl"));
                object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
            }
        }

        for (unsigned i = 0; i < 10000; ++i)
        {
            StaticModel* object = scene->CreateChild<StaticModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(Random() * 1000.0f - 500.0f, 0.0f, Random() * 1000.0f - 500.0f));
            object->SetScale(1.5f);
            object->SetModel(cache->LoadResource<Model>("Mushroom.mdl"));
            object->SetMaterial(cache->LoadResource<Material>("Mushroom.json"));
            object->SetCastShadows(true);
            object->SetLodBias(2.0f);
            object->SetMaxDistance(600.0f);
        }

        Vector3 quadrantCenters[] = 
        {
            Vector3(-290.0f, 0.0f, -290.0f),
            Vector3(290.0f, 0.0f, -290.0f),
            Vector3(-290.0f, 0.0f, 290.0f),
            Vector3(290.0f, 0.0f, 290.0f),
        };

        std::vector<Light*> lights;

        for (unsigned i = 0; i < 100; ++i)
        {
            Light* light = scene->CreateChild<Light>();
            light->SetStatic(true);
            light->SetLightType(LIGHT_POINT);
            light->SetCastShadows(true);
            Vector3 colorVec = 2.0f * Vector3(Random(), Random(), Random()).Normalized();
            light->SetColor(Color(colorVec.x, colorVec.y, colorVec.z, 0.5f));
            light->SetRange(40.0f);
            light->SetShadowMapSize(256);
            light->SetShadowMaxDistance(200.0f);
            light->SetMaxDistance(900.0f);

            for (;;)
            {
                Vector3 newPos = quadrantCenters[i % 4] + Vector3(Random() * 500.0f - 250.0f, 10.0f, Random() * 500.0f - 250.0f);
                bool posOk = true;

                for (unsigned j = 0; j < lights.size(); ++j)
                {
                    if ((newPos - lights[j]->Position()).Length() < 80.0f)
                    {
                        posOk = false;
                        break;
                    }
                }

                if (posOk)
                {
                    light->SetPosition(newPos);
                    break;
                }
            }

            lights.push_back(light);
        }

        {
            StaticModel* object = scene->CreateChild<StaticModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(0.0f, 25.0f, 0.0f));
            object->SetScale(Vector3(1165.0f, 50.0f, 1.0f));
            object->SetModel(cache->LoadResource<Model>("Box.mdl"));
            object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
            object->SetCastShadows(true);
        }

        {
            StaticModel* object = scene->CreateChild<StaticModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(0.0f, 25.0f, 0.0f));
            object->SetScale(Vector3(1.0f, 50.0f, 1165.0f));
            object->SetModel(cache->LoadResource<Model>("Box.mdl"));
            object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
            object->SetCastShadows(true);
        }
    }
    // Preset 1: high number of animating cubes
    else if (preset == 1)
    {
        lightEnvironment->SetFogColor(Color(0.3f, 0.3f, 0.3f));
        lightEnvironment->SetFogStart(300.0f);
        lightEnvironment->SetFogEnd(500.0f);
        camera->SetFarClip(500.0f);

        SharedPtr<Material> customMat = Material::DefaultMaterial()->Clone();
        customMat->SetUniform("matDiffColor", Vector4(0.75f, 0.35f, 0.0f, 1.0f));
        customMat->SetUniform("matSpecColor", Vector4(0.75f / 3.0f, 0.35f / 3.0f, 0.0f, 1.0f));

        for (int y = -125; y <= 125; ++y)
        {
            for (int x = -125; x <= 125; ++x)
            {
                StaticModel* object = scene->CreateChild<StaticModel>();
                //object->SetStatic(true);
                object->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
                object->SetScale(0.25f);
                object->SetModel(cache->LoadResource<Model>("Box.mdl"));
                object->SetMaterial(customMat);
                rotatingObjects.push_back(object);
            }
        }

        Light* light = scene->CreateChild<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetColor(Color(1.0f, 1.0f, 1.0f, 0.5f));
        light->SetRotation(Quaternion(45.0f, 45.0f, 0.0f));
    }
    // Preset 2: skinned characters with directional light shadows
    else if (preset == 2)
    {
        lightEnvironment->SetFogColor(Color(0.5f, 0.5f, 0.75f));
        lightEnvironment->SetFogStart(300.0f);
        lightEnvironment->SetFogEnd(500.0f);
        camera->SetFarClip(500.0f);

        {
            StaticModel* object = scene->CreateChild<StaticModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(0, -0.05f, 0));
            object->SetScale(Vector3(100.0f, 0.1f, 100.0f));
            object->SetModel(cache->LoadResource<Model>("Box.mdl"));
            object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
        }

        for (int i = 0; i < 500; ++i)
        {
            AnimatedModel* object = scene->CreateChild<AnimatedModel>();
            object->SetStatic(true);
            object->SetPosition(Vector3(Random() * 90.0f - 45.0f, 0.0f, Random() * 90.0f - 45.0f));
            object->SetRotation(Quaternion(Random(360.0f), Vector3::UP));
            object->SetModel(cache->LoadResource<Model>("Jack.mdl"));
            object->SetCastShadows(true);
            object->SetMaxDistance(600.0f);
            AnimationState* state = object->AddAnimationState(cache->LoadResource<Animation>("Jack_Walk.ani"));
            state->SetWeight(1.0f);
            state->SetLooped(true);
            animatingObjects.push_back(object);
        }

        Light* light = scene->CreateChild<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetCastShadows(true);
        light->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        light->SetRotation(Quaternion(45.0f, 45.0f, 0.0f));
        light->SetShadowMapSize(2048);
        light->SetShadowMaxDistance(100.0f);
    }
}

int ApplicationMain(const std::vector<std::string>& arguments)
{
    bool useThreads = true;

    if (arguments.size() > 1 && arguments[1].find("nothreads") != std::string::npos)
        useThreads = false;

    // Create subsystems that don't depend on the application window / OpenGL context
    AutoPtr<WorkQueue> workQueue = new WorkQueue(useThreads ? 0 : 1);
    AutoPtr<Profiler> profiler = new Profiler();
    AutoPtr<Log> log = new Log();
    AutoPtr<ResourceCache> cache = new ResourceCache();
    cache->AddResourceDir(ExecutableDir() + "Data");

    // Create the Graphics subsystem to open the application window and initialize OpenGL
    AutoPtr<Graphics> graphics = new Graphics("Turso3D renderer test", IntVector2(1920, 1080));
    if (!graphics->Initialize())
        return 1;

    // Create subsystems that depend on the application window / OpenGL
    AutoPtr<Input> input = new Input(graphics->Window());
    AutoPtr<Renderer> renderer = new Renderer();
    AutoPtr<DebugRenderer> debugRenderer = new DebugRenderer();

    renderer->SetupShadowMaps(1024, 2048, FMT_D16);
    
    // Rendertarget textures
    AutoPtr<FrameBuffer> viewFbo = new FrameBuffer();
    AutoPtr<FrameBuffer> viewMRTFbo = new FrameBuffer();
    AutoPtr<FrameBuffer> ssaoFbo = new FrameBuffer();
    AutoPtr<Texture> colorBuffer = new Texture();
    AutoPtr<Texture> normalBuffer = new Texture();
    AutoPtr<Texture> depthStencilBuffer = new Texture();
    AutoPtr<Texture> ssaoTexture = new Texture();
    AutoPtr<Texture> occlusionDebugTexture = new Texture();

    // Random noise texture for SSAO
    unsigned char noiseData[4 * 4 * 4];
    for (int i = 0; i < 4 * 4; ++i)
    {
        Vector3 noiseVec(Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f);
        noiseVec.Normalize();

        noiseData[i * 4 + 0] = (unsigned char)(noiseVec.x * 127.0f + 128.0f);
        noiseData[i * 4 + 1] = (unsigned char)(noiseVec.y * 127.0f + 128.0f);
        noiseData[i * 4 + 2] = (unsigned char)(noiseVec.z * 127.0f + 128.0f);
        noiseData[i * 4 + 3] = 0;
    }

    ImageLevel noiseDataLevel(IntVector2(4, 4), FMT_RGBA8, &noiseData[0]);
    AutoPtr<Texture> noiseTexture = new Texture();
    noiseTexture->Define(TEX_2D, IntVector2(4, 4), FMT_RGBA8, 1, 1, &noiseDataLevel);
    noiseTexture->DefineSampler(FILTER_POINT);

    // Create the scene and camera. Camera is created outside scene so it's not disturbed by scene clears
    SharedPtr<Scene> scene = Object::Create<Scene>();
    SharedPtr<Camera> camera = Object::Create<Camera>();
    CreateScene(scene, camera, 0);

    camera->SetPosition(Vector3(0.0f, 20.0f, -75.0f));

    float yaw = 0.0f, pitch = 20.0f;
    HiresTimer frameTimer;
    Timer profilerTimer;
    float dt = 0.0f;
    float angle = 0.0f;
    int shadowMode = 1;
    bool drawSSAO = false;
    bool useOcclusion = true;
    bool animate = true;
    bool drawDebug = false;
    bool drawShadowDebug = false;
    bool drawOcclusionDebug = false;

    std::string profilerOutput;

    // Main loop
    while (!input->ShouldExit() && !input->KeyPressed(27))
    {
        ZoneScoped;
        frameTimer.Reset();

        if (profilerTimer.ElapsedMSec() >= 1000)
        {
            profilerOutput = profiler->OutputResults();
            profiler->BeginInterval();
            profilerTimer.Reset();
        }

        profiler->BeginFrame();

        // Check for input and scene switch / debug render options
        input->Update();

        if (input->KeyPressed(SDLK_F1))
            CreateScene(scene, camera, 0);
        if (input->KeyPressed(SDLK_F2))
            CreateScene(scene, camera, 1);
        if (input->KeyPressed(SDLK_F3))
            CreateScene(scene, camera, 2);

        if (input->KeyPressed(SDLK_1))
        {
            ++shadowMode;
            if (shadowMode > 2)
                shadowMode = 0;
            
            float biasMul = shadowMode > 1 ? 1.25f : 1.0f;
            Material::SetGlobalShaderDefines("", shadowMode > 1 ? "HQSHADOW" : "");
            renderer->SetShadowDepthBiasMul(biasMul, biasMul);
        }
        
        if (input->KeyPressed(SDLK_2))
            drawSSAO = !drawSSAO;
        if (input->KeyPressed(SDLK_3))
            useOcclusion = !useOcclusion;
        if (input->KeyPressed(SDLK_4))
            drawDebug = !drawDebug;
        if (input->KeyPressed(SDLK_5))
            drawShadowDebug = !drawShadowDebug;
        if (input->KeyPressed(SDLK_6))
            drawOcclusionDebug = !drawOcclusionDebug;
        if (input->KeyPressed(SDLK_SPACE))
            animate = !animate;

        if (input->KeyPressed(SDLK_f))
            graphics->SetFullscreen(!graphics->IsFullscreen());
        if (input->KeyPressed(SDLK_v))
            graphics->SetVSync(!graphics->VSync());

        // Camera movement
        IntVector2 mouseMove = input->MouseMove();
        yaw += mouseMove.x * 0.1f;
        pitch += mouseMove.y * 0.1f;
        pitch = Clamp(pitch, -90.0f, 90.0f);
        camera->SetRotation(Quaternion(pitch, yaw, 0.0f));

        float moveSpeed = (input->KeyDown(SDLK_LSHIFT) || input->KeyDown(SDLK_RSHIFT)) ? 50.0f : 5.0f;

        if (input->KeyDown(SDLK_w))
            camera->Translate(Vector3::FORWARD * dt * moveSpeed);
        if (input->KeyDown(SDLK_s))
            camera->Translate(Vector3::BACK * dt * moveSpeed);
        if (input->KeyDown(SDLK_a))
            camera->Translate(Vector3::LEFT * dt * moveSpeed);
        if (input->KeyDown(SDLK_d))
            camera->Translate(Vector3::RIGHT * dt * moveSpeed);

        // Scene animation
        if (animate)
        {
            ZoneScopedN("MoveObjects");

            PROFILE(MoveObjects);
        
            if (rotatingObjects.size())
            {
                angle += 100.0f * dt;
                Quaternion rotQuat(angle, Vector3::ONE);
                for (auto it = rotatingObjects.begin(); it != rotatingObjects.end(); ++it)
                    (*it)->SetRotation(rotQuat);
            }
            else if (animatingObjects.size())
            {
                for (auto it = animatingObjects.begin(); it != animatingObjects.end(); ++it)
                {
                    AnimatedModel* object = *it;
                    AnimationState* state = object->AnimationStates()[0];
                    state->AddTime(dt);
                    object->Translate(Vector3::FORWARD * 2.0f * dt);

                    // Rotate to avoid going outside the plane
                    Vector3 pos = object->Position();
                    if (pos.x < -45.0f || pos.x > 45.0f || pos.z < -45.0f || pos.z > 45.0f)
                        object->Yaw(45.0f * dt);
                }
            }
        }

        // Recreate rendertarget textures if window resolution changed
        int width = graphics->RenderWidth();
        int height = graphics->RenderHeight();

        if (colorBuffer->Width() != width || colorBuffer->Height() != height)
        {
            colorBuffer->Define(TEX_2D, IntVector2(width, height), FMT_RGBA8);
            colorBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            depthStencilBuffer->Define(TEX_2D, IntVector2(width, height), FMT_D32);
            depthStencilBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            normalBuffer->Define(TEX_2D, IntVector2(width, height), FMT_RGBA8);
            normalBuffer->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            viewFbo->Define(colorBuffer, depthStencilBuffer);

            std::vector<Texture*> mrt;
            mrt.push_back(colorBuffer.Get());
            mrt.push_back(normalBuffer.Get());
            viewMRTFbo->Define(mrt, depthStencilBuffer);
        }

        // Similarly recreate SSAO texture if needed
        if (drawSSAO && (ssaoTexture->Width() != colorBuffer->Width() / 2 || ssaoTexture->Height() != colorBuffer->Height() / 2))
        {
            ssaoTexture->Define(TEX_2D, IntVector2(colorBuffer->Width() / 2, colorBuffer->Height() / 2), FMT_R32F, 1, 1);
            ssaoTexture->DefineSampler(FILTER_BILINEAR, ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
            ssaoFbo->Define(ssaoTexture, nullptr);
        }

        camera->SetAspectRatio((float)width / (float)height);

        // Collect geometries and lights in frustum. Also set debug renderer to use the correct camera view
        {
            PROFILE(PrepareView);
            renderer->PrepareView(scene, camera, shadowMode > 0, useOcclusion);
            debugRenderer->SetView(camera);
        }
        
        // Raycast into the scene using the camera forward vector. If has a hit, draw a small debug sphere at the hit location
        {
            PROFILE(Raycast);

            Ray cameraRay(camera->WorldPosition(), camera->WorldDirection());
            RaycastResult res = scene->FindChild<Octree>()->RaycastSingle(cameraRay, DF_GEOMETRY);
            if (res.drawable)
                debugRenderer->AddSphere(Sphere(res.position, 0.05f), Color::WHITE, true);
        }

        // Now render the scene, starting with shadowmaps and opaque geometries
        {
            PROFILE(RenderView);

            renderer->RenderShadowMaps();

            // The default opaque shaders can write both color (first RT) and view-space normals (second RT).
            // If going to render SSAO, bind both rendertargets, else just the color RT
            if (drawSSAO)
                graphics->SetFrameBuffer(viewMRTFbo);
            else
                graphics->SetFrameBuffer(viewFbo);

            graphics->SetViewport(IntRect(0, 0, width, height));
            renderer->RenderOpaque();

            // Optional SSAO effect. First sample the normals and depth buffer, then apply a blurred SSAO result that darkens the opaque geometry
            if (drawSSAO)
            {
                float farClip = camera->FarClip();
                float nearClip = camera->NearClip();
                Vector3 nearVec, farVec;
                camera->FrustumSize(nearVec, farVec);

                ShaderProgram* program = graphics->SetProgram("Shaders/SSAO.glsl");
                graphics->SetFrameBuffer(ssaoFbo);
                graphics->SetViewport(IntRect(0, 0, ssaoTexture->Width(), ssaoTexture->Height()));
                graphics->SetUniform(program, "noiseInvSize", Vector2(ssaoTexture->Width() / 4.0f, ssaoTexture->Height() / 4.0f));
                graphics->SetUniform(program, "screenInvSize", Vector2(1.0f / colorBuffer->Width(), 1.0f / colorBuffer->Height()));
                graphics->SetUniform(program, "frustumSize", Vector4(farVec, (float)height / (float)width));
                graphics->SetUniform(program, "aoParameters", Vector4(0.15f, 1.0f, 0.025f, 0.15f));
                graphics->SetUniform(program, "depthReconstruct", Vector2(farClip / (farClip - nearClip), -nearClip / (farClip - nearClip)));
                graphics->SetTexture(0, depthStencilBuffer);
                graphics->SetTexture(1, normalBuffer);
                graphics->SetTexture(2, noiseTexture);
                graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
                graphics->DrawQuad();
                graphics->SetTexture(1, nullptr);
                graphics->SetTexture(2, nullptr);

                program = graphics->SetProgram("Shaders/SSAOBlur.glsl");
                graphics->SetFrameBuffer(viewFbo);
                graphics->SetViewport(IntRect(0, 0, width, height));
                graphics->SetUniform(program, "blurInvSize", Vector2(1.0f / ssaoTexture->Width(), 1.0f / ssaoTexture->Height()));
                graphics->SetTexture(0, ssaoTexture);
                graphics->SetRenderState(BLEND_SUBTRACT, CULL_NONE, CMP_ALWAYS, true, false);
                graphics->DrawQuad();
                graphics->SetTexture(0, nullptr);
            }

            // Render alpha geometry. Now only the color rendertarget is needed
            graphics->SetFrameBuffer(viewFbo);
            graphics->SetViewport(IntRect(0, 0, width, height));
            renderer->RenderAlpha();
        
            // Optional render of debug geometry
            if (drawDebug)
                renderer->RenderDebug();

            debugRenderer->Render();
            
            // Optional debug render of shadowmap. Draw both dir light cascades and the shadow atlas
            if (drawShadowDebug)
            {
                Matrix4 quadMatrix = Matrix4::IDENTITY;
                quadMatrix.m00 = 0.33f * 2.0f * (9.0f / 16.0f);
                quadMatrix.m11 = 0.33f;
                quadMatrix.m03 = -1.0f + quadMatrix.m00;
                quadMatrix.m13 = -1.0f + quadMatrix.m11;

                ShaderProgram* program = graphics->SetProgram("Shaders/DebugShadow.glsl");
                graphics->SetUniform(program, "worldViewProjMatrix", quadMatrix);
                graphics->SetTexture(0, renderer->ShadowMapTexture(0));
                graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
                graphics->DrawQuad();

                quadMatrix.m03 += 1.5f * quadMatrix.m00;
                quadMatrix.m00 = 0.33f * (9.0f / 16.0f);

                graphics->SetUniform(program, "worldViewProjMatrix", quadMatrix);
                graphics->SetTexture(0, renderer->ShadowMapTexture(1));
                graphics->DrawQuad();

                graphics->SetTexture(0, nullptr);
            }

            // Blit rendered contents to backbuffer now before presenting
            graphics->Blit(nullptr, IntRect(0, 0, width, height), viewFbo, IntRect(0, 0, width, height), true, false, FILTER_POINT);
        }

        {
            PROFILE(Present);
            graphics->Present();
        }

        profiler->EndFrame();
        dt = frameTimer.ElapsedUSec() * 0.000001f;

        FrameMark;
    }

    printf("%s", profilerOutput.c_str());

    return 0;
}

int main(int argc, char** argv)
{
    return ApplicationMain(ParseArguments(argc, argv));
}