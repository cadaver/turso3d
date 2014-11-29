// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLBlendState.h"
#include "GLGraphics.h"

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

BlendState::BlendState()
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
            graphics->SetBlendState(nullptr);
    }
}

bool BlendState::Define(bool blendEnable_, BlendFactor srcBlend_, BlendFactor destBlend_, BlendOp blendOp_,
    BlendFactor srcBlendAlpha_, BlendFactor destBlendAlpha_, BlendOp blendOpAlpha_, unsigned char colorWriteMask_,
    bool alphaToCoverage_)
{
    PROFILE(DefineBlendState);

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

    return true;
}

}
