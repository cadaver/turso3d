// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../IO/JSONValue.h"
#include "DepthState.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

bool DepthState::LoadJSON(const JSONValue& source)
{
    bool depthEnable_ = true;
    bool depthWrite_ = true;
    CompareFunc depthFunc_ = CMP_LESS;
    bool stencilEnable_ = false;
    unsigned char stencilReadMask_ = 0xff;
    unsigned char stencilWriteMask_ = 0xff;
    StencilOp frontFail_ = STENCIL_OP_KEEP;
    StencilOp frontDepthFail_ = STENCIL_OP_KEEP;
    StencilOp frontPass_ = STENCIL_OP_KEEP;
    CompareFunc frontFunc_ = CMP_ALWAYS;
    StencilOp backFail_ = STENCIL_OP_KEEP;
    StencilOp backDepthFail_ = STENCIL_OP_KEEP;
    StencilOp backPass_ = STENCIL_OP_KEEP;
    CompareFunc backFunc_ = CMP_ALWAYS;

    if (source.Contains("depthEnable"))
        depthEnable_ = source["depthEnable"].GetBool();
    if (source.Contains("depthWrite"))
        depthEnable_ = source["depthWrite"].GetBool();
    if (source.Contains("depthFunc"))
        depthFunc_ = (CompareFunc)String::ListIndex(source["depthFunc"].GetString(), compareFuncNames, CMP_LESS);
    if (source.Contains("stencilEnable"))
        stencilEnable_ = source["stencilEnable"].GetBool();
    if (source.Contains("stencilReadMask"))
        stencilReadMask_ = (unsigned char)source["stencilReadMask"].GetNumber();
    if (source.Contains("stencilWriteMask"))
        stencilWriteMask_ = (unsigned char)source["stencilWriteMask"].GetNumber();
    if (source.Contains("frontFail"))
        frontFail_ = (StencilOp)String::ListIndex(source["frontFail"].GetString(), stencilOpNames, STENCIL_OP_KEEP);
    if (source.Contains("frontDepthFail"))
        frontDepthFail_ = (StencilOp)String::ListIndex(source["frontDepthFail"].GetString(), stencilOpNames, STENCIL_OP_KEEP);
    if (source.Contains("frontPass"))
        frontPass_ = (StencilOp)String::ListIndex(source["frontPass"].GetString(), stencilOpNames, STENCIL_OP_KEEP);
    if (source.Contains("frontFunc"))
        frontFunc_ = (CompareFunc)String::ListIndex(source["frontFunc"].GetString(), compareFuncNames, CMP_ALWAYS);
    if (source.Contains("backFail"))
        backFail_ = (StencilOp)String::ListIndex(source["backFail"].GetString(), stencilOpNames, STENCIL_OP_KEEP);
    if (source.Contains("backDepthFail"))
        backDepthFail_ = (StencilOp)String::ListIndex(source["backDepthFail"].GetString(), stencilOpNames, STENCIL_OP_KEEP);
    if (source.Contains("backPass"))
        backPass_ = (StencilOp)String::ListIndex(source["backPass"].GetString(), stencilOpNames, STENCIL_OP_KEEP);
    if (source.Contains("backFunc"))
        backFunc_ = (CompareFunc)String::ListIndex(source["backFunc"].GetString(), compareFuncNames, CMP_ALWAYS);

    return Define(depthEnable_, depthWrite_, depthFunc_, stencilEnable_, stencilReadMask_, stencilWriteMask_, frontFail_,
        frontDepthFail_, frontPass_, frontFunc_, backFail_, backDepthFail_, backPass_, backFunc_);
}

void DepthState::SaveJSON(JSONValue& dest)
{
    dest.SetEmptyObject();
    dest["depthEnable"] = depthEnable;
    dest["depthWrite"] = depthWrite;
    dest["depthFunc"] = compareFuncNames[depthFunc];
    dest["stencilEnable"] = stencilEnable;
    dest["stencilReadMask"] = stencilReadMask;
    dest["stencilWriteMask"] = stencilWriteMask;
    dest["frontFail"] = stencilOpNames[frontFail];
    dest["frontDepthFail"] = stencilOpNames[frontDepthFail];
    dest["frontPass"] = stencilOpNames[frontPass];
    dest["frontFunc"] = compareFuncNames[frontFunc];
    dest["backFail"] = stencilOpNames[backFail];
    dest["backDepthFail"] = stencilOpNames[backDepthFail];
    dest["backPass"] = stencilOpNames[backPass];
    dest["backFunc"] = compareFuncNames[backFunc];
}

}
