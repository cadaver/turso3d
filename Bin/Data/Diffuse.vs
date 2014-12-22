#include "CommonCode.vs"

struct VSInput
{
    float4 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float3 worldPos = mul(input.position, worldMatrix);
    output.position = mul(float4(worldPos, 1.0), viewProjMatrix);
    output.texCoord = input.texCoord;
    return output;
}