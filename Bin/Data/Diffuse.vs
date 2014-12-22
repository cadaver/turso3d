#include "CommonCode.vs"

struct VSInput
{
    float4 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float3 worldPos = mul(input.position, worldMatrix);
    output.position = mul(float4(worldPos, 1.0), viewProjMatrix);
    return output;
}