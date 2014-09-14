// For conditions of distribution and use, see copyright notice in License.txt

#include "Turso3D.h"
#include "Debug/DebugNew.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <cstdio>
#include <cstdlib>

using namespace Turso3D;

class GraphicsTest : public Object
{
    OBJECT(GraphicsTest);

public:
    void Run()
    {
        input = new Input();
        graphics = new Graphics();
        graphics->RenderWindow()->SetTitle("Graphics test");
        graphics->SetMode(640, 480, false);

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &GraphicsTest::HandleCloseRequest);
        
        while (graphics->RenderWindow()->IsOpen())
        {
            input->Update();
            graphics->Clear(CLEAR_COLOR, Color(Random(), Random(), Random()));
            graphics->Present();
        }
    }

    void HandleCloseRequest(Event& /* event */)
    {
        graphics->Close();
    }

    AutoPtr<Graphics> graphics;
    AutoPtr<Input> input;
};

int main()
{
    #ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
    GraphicsTest test;
    test.Run();

    return 0;
}