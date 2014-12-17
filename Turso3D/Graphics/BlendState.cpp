// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../IO/JSONValue.h"
#include "BlendState.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

bool BlendState::LoadJSON(const JSONValue& source)
{
    bool blendEnable_ = false;
    BlendFactor srcBlend_ = BLEND_ONE;
    BlendFactor destBlend_ = BLEND_ONE;
    BlendOp blendOp_ = BLEND_OP_ADD;
    BlendFactor srcBlendAlpha_ = BLEND_ONE;
    BlendFactor destBlendAlpha_ = BLEND_ONE;
    BlendOp blendOpAlpha_ = BLEND_OP_ADD;
    unsigned char colorWriteMask_ = COLORMASK_ALL;
    bool alphaToCoverage_ = false;

    if (source.Contains("blendEnable"))
        blendEnable_ = source["blendEnable"].GetBool();
    if (source.Contains("srcBlend"))
        srcBlend_ = (BlendFactor)String::ListIndex(source["srcBlend"].GetString(), blendFactorNames, BLEND_ONE);
    if (source.Contains("destBlend"))
        destBlend_ = (BlendFactor)String::ListIndex(source["destBlend"].GetString(), blendFactorNames, BLEND_ONE);
    if (source.Contains("blendOp"))
        blendOp_ = (BlendOp)String::ListIndex(source["blendOp"].GetString(), blendOpNames, BLEND_OP_ADD);
    if (source.Contains("srcBlendAlpha"))
        srcBlendAlpha_ = (BlendFactor)String::ListIndex(source["srcBlendAlpha"].GetString(), blendFactorNames, BLEND_ONE);
    if (source.Contains("destBlendAlpha"))
        destBlendAlpha_ = (BlendFactor)String::ListIndex(source["destBlendAlpha"].GetString(), blendFactorNames, BLEND_ONE);
    if (source.Contains("blendOpAlpha"))
        blendOpAlpha_ = (BlendOp)String::ListIndex(source["blendOpAlpha"].GetString(), blendOpNames, BLEND_OP_ADD);
    if (source.Contains("colorWriteMask"))
        colorWriteMask_ = (unsigned char)source["colorWriteMask"].GetNumber();
    if (source.Contains("alphaToCoverage"))
        alphaToCoverage_ = source["alphaToCoverage"].GetBool();

    return Define(blendEnable_, srcBlend_, destBlend_, blendOp_, srcBlendAlpha_, destBlendAlpha_, blendOpAlpha_, colorWriteMask_, alphaToCoverage_);
}

void BlendState::SaveJSON(JSONValue& dest)
{
    dest.SetEmptyObject();
    dest["blendEnable"] = blendEnable;
    dest["srcBlend"] = blendFactorNames[srcBlend];
    dest["destBlend"] = blendFactorNames[destBlend];
    dest["blendOp"] = blendOpNames[blendOp];
    dest["srcBlendAlpha"] = blendFactorNames[srcBlendAlpha];
    dest["destBlendAlpha"] = blendFactorNames[destBlendAlpha];
    dest["blendOpAlpha"] = blendOpNames[blendOpAlpha];
    dest["colorWriteMask"] = colorWriteMask;
    dest["alphaToCoverage"] = alphaToCoverage;
}

}
