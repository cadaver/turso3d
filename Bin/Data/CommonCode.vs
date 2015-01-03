cbuffer PerFrameVS : register(b0)
{
    float4x3 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjMatrix;
    float4 depthParameters;
}

cbuffer PerObjectVS : register(b1)
{
    float4x3 worldMatrix;
}

cbuffer LightsVS : register(b3)
{
    float4x4 shadowMatrices[4];
}

float CalculateDepth(float4 clipPos)
{
    return dot(depthParameters.zw, clipPos.zw);
}