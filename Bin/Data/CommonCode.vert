layout(std140) uniform PerFrameVS0
{
    mat3x4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjMatrix;
    vec4 depthParameters;
};

layout(std140) uniform PerObjectVS1
{
    mat3x4 worldMatrix;
};

layout(std140) uniform LightsVS3
{
    mat4 shadowMatrices[4];
};

float CalculateDepth(vec4 clipPos)
{
    return dot(depthParameters.zw, clipPos.zw);
}