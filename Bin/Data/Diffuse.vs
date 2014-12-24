#include "CommonCode.vs"

struct VSInput
{
    float4 position : POSITION;
    float2 texCoord : TEXCOORD0;
    #ifdef INSTANCED
    float4x3 instanceWorldMatrix : TEXCOORD4;
    #endif
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
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
    output.texCoord = input.texCoord;
    return output;
}