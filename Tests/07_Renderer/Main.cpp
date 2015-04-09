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
        graphics->RenderWindow()->SetMouseVisible(false);
        graphics->SetMode(IntVector2(800, 600), false, true);

        renderer->SetupShadowMaps(1, 2048, FMT_D16);

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &RendererTest::HandleCloseRequest);

        SharedPtr<Scene> scene = new Scene();
        scene->CreateChild<Octree>();
        Camera* camera = scene->CreateChild<Camera>();
        camera->SetPosition(Vector3(0.0f, 20.0f, -75.0f));
        camera->SetAmbientColor(Color(0.1f, 0.1f, 0.1f));

        for (int y = -5; y <= 5; ++y)
        {
            for (int x = -5; x <= 5; ++x)
            {
                StaticModel* object = scene->CreateChild<StaticModel>();
                object->SetPosition(Vector3(10.5f * x, -0.1f, 10.5f * y));
                object->SetScale(Vector3(10.0f, 0.1f, 10.0f));
                object->SetModel(cache->LoadResource<Model>("Box.mdl"));
                object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
            }
        }

        for (unsigned i = 0; i < 435; ++i)
        {
            StaticModel* object = scene->CreateChild<StaticModel>();
            object->SetPosition(Vector3(Random() * 100.0f - 50.0f, 1.0f, Random() * 100.0f - 50.0f));
            object->SetScale(1.5f);
            object->SetModel(cache->LoadResource<Model>("Mushroom.mdl"));
            object->SetMaterial(cache->LoadResource<Material>("Mushroom.json"));
            object->SetCastShadows(true);
            object->SetLodBias(2.0f);
        }

        for (unsigned i = 0; i < 10; ++i)
        {
            Light* light = scene->CreateChild<Light>();
            light->SetLightType(LIGHT_POINT);
            light->SetCastShadows(true);
            Vector3 colorVec = 2.0f * Vector3(Random(), Random(), Random()).Normalized();
            light->SetColor(Color(colorVec.x, colorVec.y, colorVec.z));
            light->SetFov(90.0f);
            light->SetRange(20.0f);
            light->SetPosition(Vector3(Random() * 120.0f - 60.0f, 7.0f, Random() * 120.0f - 60.0f));
            light->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
            light->SetShadowMapSize(256);
        }

        float yaw = 0.0f, pitch = 20.0f;
        HiresTimer frameTimer;
        Timer profilerTimer;
        float dt = 0.0f;
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

            pitch += input->MouseMove().y * 0.25f;
            yaw += input->MouseMove().x * 0.25f;
            pitch = Clamp(pitch, -90.0f, 90.0f);

            float moveSpeed = input->IsKeyDown(VK_SHIFT) ? 50.0f : 10.0f;

            camera->SetRotation(Quaternion(pitch, yaw, 0.0f));
            if (input->IsKeyDown('W'))
                camera->Translate(Vector3::FORWARD * dt * moveSpeed);
            if (input->IsKeyDown('S'))
                camera->Translate(Vector3::BACK * dt * moveSpeed);
            if (input->IsKeyDown('A'))
                camera->Translate(Vector3::LEFT * dt * moveSpeed);
            if (input->IsKeyDown('D'))
                camera->Translate(Vector3::RIGHT * dt * moveSpeed);

            // Drawing and state setting functions will not check Graphics initialization state, check now
            if (!graphics->IsInitialized())
                break;

            // Update camera aspect ratio based on window size
            camera->SetAspectRatio((float)graphics->Width() / (float)graphics->Height());

            {
                PROFILE(RenderScene);
                Vector<PassDesc> passes;
                passes.Push(PassDesc("opaque", SORT_STATE, true));
                passes.Push(PassDesc("alpha", SORT_BACK_TO_FRONT, true));
                renderer->PrepareView(scene, camera, passes);

                renderer->RenderShadowMaps();
                graphics->ResetRenderTargets();
                graphics->ResetViewport();
                graphics->Clear(CLEAR_COLOR | CLEAR_DEPTH, Color::BLACK);
                renderer->RenderBatches(passes);
            }
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