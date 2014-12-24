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
        RegisterGraphicsLibrary();
        RegisterResourceLibrary();
        RegisterRendererLibrary();

        cache = new ResourceCache();
        cache->AddResourceDir(ExecutableDir() + "Data");

        log = new Log();
        input = new Input();
        profiler = new Profiler();

        graphics = new Graphics();
        graphics->RenderWindow()->SetTitle("Renderer test");
        graphics->SetMode(IntVector2(640, 480), false, true);

        renderer = new Renderer();

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &RendererTest::HandleCloseRequest);

        SharedPtr<Material> mat = Object::Create<Material>();
        mat->SetTexture(0, cache->LoadResource<Texture>("Test.png"));
        Pass* pass = mat->CreatePass("opaque");
        pass->SetShaders("Diffuse", "Diffuse");

        float vertexData[] = {
            // Position         // Texcoord
            -0.5f, 0.5f, 0.0f,  0.0f, 0.0f,
            0.5f, 0.5f, 0.0f,   1.0f, 0.0f,
            0.5f, -0.5f, 0.0f,  1.0f, 1.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 1.0f
        };

        Vector<VertexElement> vertexDeclaration;
        vertexDeclaration.Push(VertexElement(ELEM_VECTOR3, SEM_POSITION));
        vertexDeclaration.Push(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
        SharedPtr<VertexBuffer> vb = new VertexBuffer();
        vb->Define(USAGE_IMMUTABLE, 4, vertexDeclaration, true, vertexData);

        unsigned short indexData[] = {
            0,
            1,
            3,
            1,
            2,
            3,
        };

        SharedPtr<IndexBuffer> ib = new IndexBuffer();
        ib->Define(USAGE_IMMUTABLE, 6, sizeof(unsigned short), true, indexData);

        SharedPtr<Scene> scene = new Scene();
        scene->CreateChild<Octree>();
        Camera* camera = scene->CreateChild<Camera>();
        camera->SetPosition(Vector3(0.0f, 0.0f, -750.0f));

        SharedPtr<Geometry> geom = new Geometry();
        geom->vertexBuffer = vb;
        geom->indexBuffer = ib;
        geom->drawStart = 0;
        geom->drawCount = 6;

        Vector<GeometryNode*> nodes;

        for (int x = -125; x <= 125; ++x)
        {
            for (int y = -125; y <= 125; ++y)
            {
                GeometryNode* node = scene->CreateChild<GeometryNode>();
                node->SetPosition(Vector3(2.0f * x, 2.0f * y, 0.0f));
                node->SetNumGeometries(1);
                node->SetGeometry(0, geom);
                node->SetMaterial(0, mat);
                node->SetLocalBoundingBox(BoundingBox(Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.5f, 0.5f, 0.0f)));
                nodes.Push(node);
            }
        }

        float yaw = 0.0f, pitch = 0.0f;
        HiresTimer frameTimer;
        float dt = 0.0f;
        bool animate = false;
        Quaternion nodeRot = Quaternion::IDENTITY;

        for (;;)
        {
            frameTimer.Reset();
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
                nodeRot = Quaternion(0.0f, 0.0f, 100.0f * dt) * nodeRot;
                nodeRot.Normalize();

                for (auto it = nodes.Begin(); it != nodes.End(); ++it)
                    (*it)->SetRotation(nodeRot);
            }

            // Drawing and state setting functions will not check Graphics initialization state, check now
            if (!graphics->IsInitialized())
                break;

            renderer->CollectObjects(scene, camera);
            renderer->CollectBatches("opaque");

            graphics->Clear(CLEAR_COLOR | CLEAR_DEPTH, Color(0.0f, 0.0f, 0.5f));
            renderer->RenderBatches("opaque");
            graphics->Present();

            profiler->EndFrame();
            dt = frameTimer.ElapsedUSec() * 0.000001f;
        }

        profiler->BeginFrame();
        {
            PROFILE(DestroyScene);
            scene.Reset();
        }
        profiler->EndFrame();

        LOGRAW(profiler->OutputResults());
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