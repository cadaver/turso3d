#include "CommonCode.vs"

struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
    #ifdef INSTANCED
    float4x3 instanceWorldMatrix : TEXCOORD4;
    #endif
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
    float4 worldPos : TEXCOORD1;
    #ifdef NUMSHADOWCOORDS
    float4 shadowPos[NUMSHADOWCOORDS] : TEXCOORD4;
    #endif
};

VSOutput main(VSInput input)
{
    VSOutput output;

    #ifdef INSTANCED
    output.worldPos.xyz = mul(input.position, input.instanceWorldMatrix);
    output.normal = normalize(mul(input.normal, (float3x3)input.instanceWorldMatrix));
    output.tangent = float4(normalize(mul(input.tangent.xyz, (float3x3)input.instanceWorldMatrix)), input.tangent.w);
    #else
    output.worldPos.xyz = mul(input.position, worldMatrix);
    output.normal = normalize(mul(input.normal, (float3x3)worldMatrix));
    output.tangent = float4(normalize(mul(input.normal, (float3x3)worldMatrix)), input.tangent.w);
    #endif

    output.position = mul(float4(output.worldPos.xyz, 1), viewProjMatrix);
    output.worldPos.w = CalculateDepth(output.position);
    output.texCoord = input.texCoord;
    #ifdef NUMSHADOWCOORDS
    for (int i = 0; i < NUMSHADOWCOORDS; ++i)
        output.shadowPos[i] = mul(float4(output.worldPos.xyz, 1.0), shadowMatrices[i]);
    #endif

    return output;
}