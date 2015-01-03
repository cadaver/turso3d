cbuffer PerFrameVS : register(b0)
{
    float4x3 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjMatrix;
}

cbuffer PerObjectVS : register(b1)
{
    float4x3 worldMatrix;
}

cbuffer LightsVS : register(b3)
{
    float4x4 shadowMatrices[4];
}
