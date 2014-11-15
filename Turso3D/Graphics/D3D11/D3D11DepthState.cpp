// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "D3D11DepthState.h"
#include "D3D11Graphics.h"

#include <d3d11.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

DepthState::DepthState() :
    stateObject(nullptr)
{
}

DepthState::~DepthState()
{
    Release();
}

void DepthState::Release()
{
    if (graphics)
    {
        if (graphics->GetDepthState() == this)
            graphics->SetDepthState(nullptr, 0);
    }
    
    if (stateObject)
    {
        ID3D11DepthStencilState* d3dDepthStencilState = (ID3D11DepthStencilState*)stateObject;
        d3dDepthStencilState->Release();
        stateObject = nullptr;
    }
}

bool DepthState::Define(bool depthEnable_, bool depthWrite_, CompareMode depthFunc_, bool stencilEnable_,
    unsigned char stencilReadMask_, unsigned char stencilWriteMask_, StencilOp frontFail_, StencilOp frontDepthFail_,
    StencilOp frontPass_, CompareMode frontFunc_, StencilOp backFail_, StencilOp backDepthFail_, StencilOp backPass_,
    CompareMode backFunc_)
{
    PROFILE(DefineDepthState);

    Release();

    depthEnable = depthEnable_;
    depthWrite = depthWrite_;
    depthFunc = depthFunc_;
    stencilEnable = stencilEnable_;
    stencilReadMask = stencilReadMask_;
    stencilWriteMask = stencilWriteMask_;
    frontFail = frontFail_;
    frontDepthFail = frontDepthFail_;
    frontPass = frontPass_;
    frontFunc = frontFunc_;
    backFail = backFail_;
    backDepthFail = backDepthFail_;
    backPass = backPass_;
    backFunc = backFunc_;

    if (graphics && graphics->IsInitialized())
    {
        D3D11_DEPTH_STENCIL_DESC stateDesc;
        memset(&stateDesc, 0, sizeof stateDesc);

        stateDesc.DepthEnable = depthEnable;
        stateDesc.DepthWriteMask = depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        stateDesc.DepthFunc = (D3D11_COMPARISON_FUNC)depthFunc;
        stateDesc.StencilEnable = stencilEnable;
        stateDesc.StencilReadMask = stencilReadMask;
        stateDesc.StencilWriteMask = stencilWriteMask;
        stateDesc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)frontFail;
        stateDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)frontDepthFail;
        stateDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)frontPass;
        stateDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)frontFunc;
        stateDesc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)backFail;
        stateDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)backDepthFail;
        stateDesc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)backPass;
        stateDesc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)backFunc;

        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();
        d3dDevice->CreateDepthStencilState(&stateDesc, (ID3D11DepthStencilState**)&stateObject);

        if (!stateObject)
        {
            LOGERROR("Failed to create depth state");
            return false;
        }
        else
            LOGDEBUG("Created depth state");
    }

    return true;
}

}
