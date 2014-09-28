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
        log = new Log();
        input = new Input();
        graphics = new Graphics();
        graphics->RenderWindow()->SetTitle("Graphics test");
        graphics->SetMode(640, 480, false, true);

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &GraphicsTest::HandleCloseRequest);
        
        float vertexData[] = {
            -1.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            1.0f, -1.0f, 0.0f
        };

        unsigned short indexData[] = {
            0,
            1,
            2
        };

        String vsCode = 
            "struct VOut"
            "{"
            "   float4 position : SV_POSITION;"
            "};"
            "VOut main(float3 position : POSITION)"
            "{"
            "   VOut output;"
            "   output.position = float4(position, 1);"
            "   return output;"
            "}";

        String psCode =
            "float4 main(float4 position : SV_POSITION) : SV_TARGET"
            "{"
            "   return float4(1, 1, 1, 1);"
            "}";

        AutoPtr<VertexBuffer> vb = new VertexBuffer();
        AutoPtr<IndexBuffer> ib = new IndexBuffer();
        vb->Define(3, MASK_POSITION, false, true, vertexData);
        ib->Define(3, sizeof(unsigned short), false, true, indexData);

        AutoPtr<Shader> vs = new Shader();
        AutoPtr<Shader> ps = new Shader();
        vs->SetName("Test.vs");
        ps->SetName("Test.ps");
        vs->Define(SHADER_VS, vsCode);
        ps->Define(SHADER_PS, psCode);
        ShaderVariation* vsv = vs->CreateVariation();
        ShaderVariation* psv = ps->CreateVariation();

        graphics->SetVertexBuffer(0, vb);
        graphics->SetIndexBuffer(ib);
        graphics->SetShaders(vsv, psv);

        while (graphics->RenderWindow()->IsOpen())
        {
            input->Update();
            // Switch fullscreen
            if (input->KeyPressed('F'))
                graphics->SwitchFullscreen();
            if (input->KeyPressed(27))
            {
                graphics->Close();
                break;
            }

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
    AutoPtr<Log> log;
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