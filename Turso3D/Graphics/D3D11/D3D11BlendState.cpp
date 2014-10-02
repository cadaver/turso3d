// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "D3D11BlendState.h"
#include "D3D11Graphics.h"

#include <d3d11.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

BlendState::BlendState() :
    stateObject(0)
{
}

BlendState::~BlendState()
{
    Release();
}

void BlendState::Release()
{
    if (graphics)
    {
        if (graphics->GetBlendState() == this)
            graphics->SetBlendState(0);
    }
    
    if (stateObject)
    {
        ID3D11BlendState* d3dBlendState = (ID3D11BlendState*)stateObject;
        d3dBlendState->Release();
        stateObject = 0;
    }
}

bool BlendState::Define(bool blendEnable_, BlendFactor srcBlend_, BlendFactor destBlend_, BlendOperation blendOp_,
    BlendFactor srcBlendAlpha_, BlendFactor destBlendAlpha_, BlendOperation blendOpAlpha_, unsigned char colorWriteMask_,
    bool alphaToCoverage_)
{
    Release();

    blendEnable = blendEnable_;
    srcBlend = srcBlend_;
    destBlend = destBlend_;
    blendOp = blendOp_;
    srcBlendAlpha = srcBlendAlpha_;
    destBlendAlpha = destBlendAlpha_;
    blendOpAlpha = blendOpAlpha_;
    colorWriteMask = colorWriteMask_;
    alphaToCoverage = alphaToCoverage_;

    if (graphics && graphics->IsInitialized())
    {
        D3D11_BLEND_DESC blendStateDesc;
        memset(&blendStateDesc, 0, sizeof blendStateDesc);

        blendStateDesc.AlphaToCoverageEnable = alphaToCoverage;
        blendStateDesc.IndependentBlendEnable = false;
        blendStateDesc.RenderTarget[0].BlendEnable = blendEnable;
        blendStateDesc.RenderTarget[0].SrcBlend = (D3D11_BLEND)srcBlend;
        blendStateDesc.RenderTarget[0].DestBlend = (D3D11_BLEND)destBlend;
        blendStateDesc.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)blendOp;
        blendStateDesc.RenderTarget[0].SrcBlendAlpha =  (D3D11_BLEND)srcBlendAlpha;
        blendStateDesc.RenderTarget[0].DestBlendAlpha =  (D3D11_BLEND)destBlendAlpha;
        blendStateDesc.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP)blendOpAlpha;
        blendStateDesc.RenderTarget[0].RenderTargetWriteMask = colorWriteMask & COLORMASK_ALL;

        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();
        d3dDevice->CreateBlendState(&blendStateDesc, (ID3D11BlendState**)&stateObject);

        if (!stateObject)
        {
            LOGERROR("Failed to create blend state");
            return false;
        }
        else
            LOGDEBUG("Created blend state");
    }

    return true;
}

}
