// For conditions of distribution and use, see copyright notice in License.txt

#include "Turso3D.h"
#include "Debug/DebugNew.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <cstdio>
#include <cstdlib>

#include "Windows.h"

using namespace Turso3D;

class RendererTest : public Object
{
    OBJECT(RendererTest);

public:
    void Run()
    {
        printf("Size of SpatialNode: %d\n", sizeof(SpatialNode));
        printf("Size of OctreeNode: %d\n", sizeof(OctreeNode));
        printf("Size of GeometryNode: %d\n", sizeof(GeometryNode));
        printf("Size of StaticModel: %d\n", sizeof(StaticModel));
        printf("Size of Octant: %d\n", sizeof(Octant));
        printf("Size of Geometry: %d\n", sizeof(Geometry));

        RegisterGraphicsLibrary();
        RegisterResourceLibrary();
        RegisterRendererLibrary();

        cache = new ResourceCache();
        cache->AddResourceDir(ExecutableDir() + "Data");

        log = new Log();
        input = new Input();
        profiler = new Profiler();
        graphics = new Graphics();
        renderer = new Renderer();

        graphics->RenderWindow()->SetTitle("Renderer test");
        graphics->SetMode(IntVector2(640, 480), false, true);

        renderer->SetupShadowMaps(1, 2048, FMT_D16);

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &RendererTest::HandleCloseRequest);

        SharedPtr<Scene> scene = new Scene();
        scene->CreateChild<Octree>();
        Camera* camera = scene->CreateChild<Camera>();
        camera->SetPosition(Vector3(0.0f, 3.0f, -20.0f));
        camera->SetAmbientColor(Color::BLACK);

        {
            Light* light = scene->CreateChild<Light>();
            light->SetPosition(Vector3(0.0f, 20.0f, 0.0f));
            light->SetLightType(LIGHT_DIRECTIONAL);
            light->SetDirection(Vector3(-0.5f, -1.0f, -0.75f));
            light->SetCastShadows(true);
            light->SetDepthBias(50);
            light->SetShadowMapSize(1024);
        }

        Vector<GeometryNode*> nodes;

        for (int x = -10; x < 10; ++x)
        {
            for (int z = -10; z < 10; ++z)
            {
                StaticModel* modelNode = scene->CreateChild<StaticModel>();
                modelNode->SetPosition(Vector3(2.0f * x, 0.5f, 2.0f * z));
                modelNode->SetModel(cache->LoadResource<Model>("Box.mdl"));
                modelNode->SetCastShadows(true);
                //for (size_t i = 0; i < modelNode->NumGeometries(); ++i)
                //    modelNode->SetMaterial(i, cache->LoadResource<Material>("Test.json"));
                nodes.Push(modelNode);
            }
        }

        {
            StaticModel* modelNode = scene->CreateChild<StaticModel>();
            modelNode->SetModel(cache->LoadResource<Model>("Box.mdl"));
            modelNode->SetScale(Vector3(50.0f, 0.01f, 50.0f));
            //for (size_t i = 0; i < modelNode->NumGeometries(); ++i)
            //    modelNode->SetMaterial(i, cache->LoadResource<Material>("Test.json"));
        }

        float yaw = 0.0f, pitch = 0.0f;
        HiresTimer frameTimer;
        Timer profilerTimer;
        float dt = 0.0f;
        bool animate = false;
        Quaternion nodeRot = Quaternion::IDENTITY;
        String profilerOutput;

        for (;;)
        {
            frameTimer.Reset();
            if (profilerTimer.ElapsedMSec() >= 1000)
            {
                profilerOutput = profiler->OutputResults();
                profilerTimer.Reset();
                profiler->BeginInterval();
            }

            profiler->BeginFrame();

            input->Update();
            if (input->IsKeyPress(27))
                graphics->Close();
            if (input->IsKeyPress(32))
                animate = !animate;
            
            if (input->IsMouseButtonDown(MOUSEB_RIGHT))
            {
                pitch += input->MouseMove().y * 0.25f;
                yaw += input->MouseMove().x * 0.25f;
                pitch = Clamp(pitch, -90.0f, 90.0f);
            }

            float moveSpeed = input->IsKeyDown(VK_SHIFT) ? 20.0f : 1.0f;

            camera->SetRotation(Quaternion(pitch, yaw, 0.0f));
            if (input->IsKeyDown('W'))
                camera->Translate(Vector3::FORWARD * dt * moveSpeed);
            if (input->IsKeyDown('S'))
                camera->Translate(Vector3::BACK * dt * moveSpeed);
            if (input->IsKeyDown('A'))
                camera->Translate(Vector3::LEFT * dt * moveSpeed);
            if (input->IsKeyDown('D'))
                camera->Translate(Vector3::RIGHT * dt * moveSpeed);

            if (animate)
            {
                PROFILE(AnimateNodes);
                nodeRot = Quaternion(-100.0f * dt, 0.0f, 0.0f) * nodeRot;
                nodeRot.Normalize();

                for (auto it = nodes.Begin(), end = nodes.End(); it != end; ++it)
                    (*it)->SetRotation(nodeRot);
            }

            // Drawing and state setting functions will not check Graphics initialization state, check now
            if (!graphics->IsInitialized())
                break;

            // Update camera aspect ratio based on window size
            camera->SetAspectRatio((float)graphics->Width() / (float)graphics->Height());

            Vector<PassDesc> passes;
            passes.Push(PassDesc("opaque", SORT_STATE, true));
            passes.Push(PassDesc("alpha", SORT_BACK_TO_FRONT, true));
            renderer->PrepareView(scene, camera, passes);

            renderer->RenderShadowMaps();
            graphics->ResetRenderTargets();
            graphics->ResetViewport();
            graphics->Clear(CLEAR_COLOR | CLEAR_DEPTH, Color(0.0f, 0.0f, 0.5f));
            renderer->RenderBatches("opaque");
            renderer->RenderBatches("alpha");
            graphics->Present();

            profiler->EndFrame();
            dt = frameTimer.ElapsedUSec() * 0.000001f;
        }

        LOGRAW(profilerOutput);
    }

    void HandleCloseRequest(Event& /* event */)
    {
        graphics->Close();
    }

    AutoPtr<ResourceCache> cache;
    AutoPtr<Graphics> graphics;
    AutoPtr<Renderer> renderer;
    AutoPtr<Input> input;
    AutoPtr<Log> log;
    AutoPtr<Profiler> profiler;
};

int main()
{
    #ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
    RendererTest test;
    test.Run();

    return 0;
}