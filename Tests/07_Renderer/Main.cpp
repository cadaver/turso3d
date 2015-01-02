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

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &RendererTest::HandleCloseRequest);

        SharedPtr<Scene> scene = new Scene();
        scene->CreateChild<Octree>();
        Camera* camera = scene->CreateChild<Camera>();
        camera->SetPosition(Vector3(0.0f, 3.0f, -50.0f));

        Light* light = scene->CreateChild<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetDirection(Vector3::FORWARD);
        light->SetColor(Color(1.0f, 0.7f, 0.3f));

        Vector<GeometryNode*> nodes;

        for (int x = -125; x < 125; ++x)
        {
            for (int z = -125; z < 125; ++z)
            {
                StaticModel* modelNode = scene->CreateChild<StaticModel>();
                modelNode->SetPosition(Vector3(2.0f * x, 0.0f, 2.0f * z));
                modelNode->SetModel(cache->LoadResource<Model>("Box.mdl"));
                for (size_t i = 0; i < modelNode->NumGeometries(); ++i)
                    modelNode->SetMaterial(i, cache->LoadResource<Material>("Test.json"));
                nodes.Push(modelNode);
            }
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

            float moveSpeed = input->IsKeyDown(VK_SHIFT) ? 200.0f : 50.0f;

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

                for (auto it = nodes.Begin(); it != nodes.End(); ++it)
                    (*it)->SetRotation(nodeRot);
            }

            // Drawing and state setting functions will not check Graphics initialization state, check now
            if (!graphics->IsInitialized())
                break;

            renderer->CollectObjects(scene, camera);

            Vector<PassDesc> passes;
            passes.Push(PassDesc("opaque", SORT_STATE, true));
            passes.Push(PassDesc("alpha", SORT_BACK_TO_FRONT, true));
            renderer->CollectBatches(passes);

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