layout(std140) uniform PerViewData0
{
    uniform mat3x4 viewMatrix;
    uniform mat4x4 projectionMatrix;
    uniform mat4x4 viewProjMatrix;
    uniform vec4 depthParameters;
    uniform vec3 cameraPosition;
    uniform vec4 ambientColor;
    uniform vec3 fogColor;
    uniform vec2 fogParameters;
    uniform vec3 dirLightDirection;
    uniform vec4 dirLightColor;
    uniform vec4 dirLightShadowSplits;
    uniform vec4 dirLightShadowParameters;
    uniform mat4x4 dirLightShadowMatrices[2];
};

struct Light
{
    vec4 position;
    vec4 direction;
    vec4 attenuation;
    vec4 color;
    vec4 shadowParameters;
    mat4 shadowMatrix;
};

layout(std140) uniform LightData1
{
    Light lights[255];
};

#ifdef SKINNED
layout(std140) uniform PerObjectData2
{
    mat3x4 skinMatrices[96];
};
#endif

float GetFogFactor(float depth)
{
    return clamp((fogParameters.x - depth) * fogParameters.y, 0.0, 1.0);
}
