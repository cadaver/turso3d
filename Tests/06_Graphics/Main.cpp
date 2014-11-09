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
        RegisterGraphicsLibrary();
        RegisterResourceLibrary();

        cache = new ResourceCache();
        cache->AddResourceDir(ExecutableDir() + "Data");

        log = new Log();
        input = new Input();

        graphics = new Graphics();
        graphics->RenderWindow()->SetTitle("Graphics test");
        graphics->SetMode(640, 480, false, true);

        SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &GraphicsTest::HandleCloseRequest);
        
        float vertexData[] = {
            // Position             // Texcoord
            0.0f, 0.05f, 0.0f,      0.5f, 0.0f,
            0.05f, -0.05f, 0.0f,    1.0f, 1.0f,
            -0.05f, -0.05f, 0.0f,   0.0f, 1.0f
        };

        unsigned short indexData[] = {
            0,
            1,
            2
        };

        Constant vc(C_VECTOR3, "Position");
        Constant pc(C_COLOR, "Color");
        
        String vsCode = 
            "cbuffer ConstantBuffer : register(b0)"
            "{"
            "    float3 ObjectPosition;"
            "}"
            ""
            "struct VOut"
            "{"
            "    float4 position : SV_POSITION;"
            "    float2 texCoord : TEXCOORD0;"
            "};"
            ""
            "VOut main(float3 position : POSITION, float2 texCoord : TEXCOORD0)"
            "{"
            "    VOut output;"
            "    output.position = float4(position + ObjectPosition, 1);"
            "    output.texCoord = texCoord;"
            "    return output;"
            "}";

        String psCode =
            "cbuffer ConstantBuffer : register(b0)"
            "{"
            "   float4 Color;"
            "}"
            ""
            "Texture2D Texture : register(t0);"
            "SamplerState Sampler : register(s0);"
            ""
            "float4 main(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_TARGET"
            "{"
            "    return Color * Texture.Sample(Sampler, texCoord);"
            "}";
        
        AutoPtr<VertexBuffer> vb = new VertexBuffer();
        vb->Define(3, MASK_POSITION | MASK_TEXCOORD1, false, true, vertexData);
        
        AutoPtr<IndexBuffer> ib = new IndexBuffer();
        ib->Define(3, sizeof(unsigned short), false, true, indexData);
        
        AutoPtr<ConstantBuffer> vcb = new ConstantBuffer();
        vcb->Define(1, &vc);

        AutoPtr<ConstantBuffer> pcb = new ConstantBuffer();
        pcb->Define(1, &pc);

        AutoPtr<Shader> vs = new Shader();
        AutoPtr<Shader> ps = new Shader();
        vs->SetName("Test.vs");
        ps->SetName("Test.ps");
        vs->Define(SHADER_VS, vsCode);
        ps->Define(SHADER_PS, psCode);
        ShaderVariation* vsv = vs->CreateVariation();
        ShaderVariation* psv = ps->CreateVariation();

        pcb->SetConstant("Color", Color::WHITE);
        pcb->Apply();

        AutoPtr<BlendState> bs = new BlendState();
        bs->Define(false);
        AutoPtr<DepthState> ds = new DepthState();
        ds->Define(true);
        AutoPtr<RasterizerState> rs = new RasterizerState();
        rs->Define();

        Texture* tex = cache->LoadResource<Texture>("Test.png");
        
        for (;;)
        {
            input->Update();
            if (input->KeyPressed('F'))
                graphics->SetFullscreen(!graphics->IsFullscreen());
            if (input->KeyPressed(27))
                graphics->Close();

            // Drawing and state setting functions will not check Graphics initialization state, check now
            if (!graphics->IsInitialized())
                break;

            graphics->Clear(CLEAR_COLOR | CLEAR_DEPTH, Color(0.0f, 0.0f, 0.5f));
            graphics->SetVertexBuffer(0, vb);
            graphics->SetIndexBuffer(ib);
            graphics->SetConstantBuffer(SHADER_VS, 0, vcb);
            graphics->SetConstantBuffer(SHADER_PS, 0, pcb);
            graphics->SetShaders(vsv, psv);
            graphics->SetBlendState(bs);
            graphics->SetDepthState(ds);
            graphics->SetRasterizerState(rs);
            graphics->SetTexture(0, tex);

            for (int i = 0; i < 1000; ++i)
            {
                vcb->SetConstant("Position", Vector3(Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, 0.0f));
                vcb->Apply();
                graphics->DrawIndexed(TRIANGLE_LIST, 0, 3);
            }

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
    
    GraphicsTest test;
    test.Run();

    return 0;
}