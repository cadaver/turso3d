// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLRasterizerState.h"

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

RasterizerState::RasterizerState()
{
}

RasterizerState::~RasterizerState()
{
    Release();
}

void RasterizerState::Release()
{
    if (graphics)
    {
        if (graphics->GetRasterizerState() == this)
            graphics->SetRasterizerState(nullptr);
    }
}

bool RasterizerState::Define(FillMode fillMode_, CullMode cullMode_, int depthBias_, float depthBiasClamp_,
    float slopeScaledDepthBias_, bool depthClipEnable_, bool scissorEnable_, bool multisampleEnable_,
    bool antialiasedLineEnable_)

{
    PROFILE(DefineRasterizerState);

    Release();

    fillMode = fillMode_;
    cullMode = cullMode_;
    depthBias = depthBias_;
    depthBiasClamp = depthBiasClamp_;
    slopeScaledDepthBias = slopeScaledDepthBias_;
    depthClipEnable = depthClipEnable_;
    scissorEnable = scissorEnable_;
    multisampleEnable = multisampleEnable_;
    antialiasedLineEnable = antialiasedLineEnable_;

    return true;
}

}
