// For conditions of distribution and use, see copyright notice in License.txt

#include "Turso3D.h"
#include "Debug/DebugNew.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <cstdio>
#include <cstdlib>

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
            // Position
            -0.5f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,
            -0.5f, -0.5f, 0.0f
        };

        Vector<VertexElement> vertexDeclaration;
        vertexDeclaration.Push(VertexElement(ELEM_VECTOR3, SEM_POSITION));
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

        BoundingBox box(Vector3(-0.5f, -0.5f, 0.0f), Vector3(0.5f, 0.5f, 0.0f));

        SharedPtr<Scene> scene = new Scene();
        scene->CreateChild<Octree>();

        Camera* camera = scene->CreateChild<Camera>();
        camera->SetPosition(Vector3(0.0f, 0.0f, -50.0f));
        
        for (int x = 0; x <= 0; ++x)
        {
            for (int y = 0; y <= 0; ++y)
            {
                GeometryNode* geom = scene->CreateChild<GeometryNode>();
                geom->SetPosition(Vector3(x * 2.0f, y * 2.0f, 0.0f));
                geom->SetupBatches(GEOM_STATIC, 1);
                geom->SetBoundingBox(box);

                SourceBatch* batch = geom->GetBatch(0);
                batch->material = mat;
                batch->vertexBuffer = vb;
                batch->indexBuffer = ib;
                batch->drawStart = 0;
                batch->drawCount = 6;
            }
        }

        for (;;)
        {
            input->Update();
            if (input->IsKeyPressed(27))
                graphics->Close();

            // Drawing and state setting functions will not check Graphics initialization state, check now
            if (!graphics->IsInitialized())
                break;

            renderer->CollectObjects(scene, camera);
            renderer->CollectBatches("opaque");

            graphics->Clear(CLEAR_COLOR | CLEAR_DEPTH, Color(0.0f, 0.0f, 0.5f));
            renderer->DrawBatches("opaque");
            graphics->Present();
        }
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