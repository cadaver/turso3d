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

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &RendererTest::HandleCloseRequest);

        for (;;)
        {
            input->Update();
            if (input->IsKeyPressed(27))
                graphics->Close();

            // Drawing and state setting functions will not check Graphics initialization state, check now
            if (!graphics->IsInitialized())
                break;

            graphics->Present();
        }
    }

    void HandleCloseRequest(Event& /* event */)
    {
        graphics->Close();
    }

    AutoPtr<ResourceCache> cache;
    AutoPtr<Graphics> graphics;
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