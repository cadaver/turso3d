// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLDepthState.h"
#include "GLGraphics.h"

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

DepthState::DepthState()
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
}

bool DepthState::Define(bool depthEnable_, bool depthWrite_, CompareFunc depthFunc_, bool stencilEnable_,
    unsigned char stencilReadMask_, unsigned char stencilWriteMask_, StencilOp frontFail_, StencilOp frontDepthFail_,
    StencilOp frontPass_, CompareFunc frontFunc_, StencilOp backFail_, StencilOp backDepthFail_, StencilOp backPass_,
    CompareFunc backFunc_)
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

    return true;
}

}
