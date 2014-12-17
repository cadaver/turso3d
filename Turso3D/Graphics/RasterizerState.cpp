// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../IO/JSONValue.h"
#include "RasterizerState.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

bool RasterizerState::LoadJSON(const JSONValue& source)
{
    FillMode fillMode_ = FILL_SOLID;
    CullMode cullMode_ = CULL_BACK;
    int depthBias_ = 0;
    float depthBiasClamp_ = M_INFINITY;
    float slopeScaledDepthBias_ = 0.0f;
    bool depthClipEnable_ = true;
    bool scissorEnable_ = false;

    if (source.Contains("fillMode"))
        fillMode_ = (FillMode)String::ListIndex(source["fillMode"].GetString(), fillModeNames, FILL_SOLID);
    if (source.Contains("cullMode"))
        cullMode_ = (CullMode)String::ListIndex(source["cullMode"].GetString(), cullModeNames, CULL_BACK);
    if (source.Contains("depthBias"))
        depthBias_ = (int)source["depthBias"].GetNumber();
    if (source.Contains("depthBiasClamp"))
        depthBiasClamp_ = (float)source["depthBiasClamp"].GetNumber();
    if (source.Contains("slopeScaledDepthBias"))
        slopeScaledDepthBias_ = (float)source["slopeScaledDepthBias"].GetNumber();
    if (source.Contains("depthClipEnable"))
        depthClipEnable_ = source["depthClipEnable"].GetBool();
    if (source.Contains("scissorEnable"))
        scissorEnable_ = source["scissorEnable"].GetBool();

    return Define(fillMode_, cullMode_, depthBias_, depthBiasClamp_, slopeScaledDepthBias_, depthClipEnable_, scissorEnable_);
}

void RasterizerState::SaveJSON(JSONValue& dest)
{
    dest.SetEmptyObject();
    dest["fillMode"] = fillModeNames[fillMode];
    dest["cullMode"] = cullModeNames[cullMode];
    dest["depthBias"] = depthBias;
    dest["depthBiasClamp"] = depthBiasClamp;
    dest["slopeScaledDepthBias"] = slopeScaledDepthBias;
    dest["depthClipEnable"] = depthClipEnable;
    dest["scissorEnable"] = scissorEnable;
}

}
