#include "CommonCode.vs"

struct VSInput
{
    float4 position : POSITION;
    #ifdef INSTANCED
    float4x3 instanceWorldMatrix : TEXCOORD4;
    #endif
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    #ifdef INSTANCED
    float3 worldPos = mul(input.position, input.instanceWorldMatrix);
    #else
    float3 worldPos = mul(input.position, worldMatrix);
    #endif

    output.position = mul(float4(worldPos, 1.0), viewProjMatrix);

    return output;
}