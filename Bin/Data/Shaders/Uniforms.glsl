layout(std140) uniform PerViewData0
{
    uniform mat3x4 viewMatrix;
    uniform mat4x4 projectionMatrix;
    uniform mat4x4 viewProjMatrix;
    uniform vec4 depthParameters;
    uniform vec4 dirLightData[12];
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
layout(std140) uniform SkinMatrixData2
{
    mat3x4 skinMatrices[96];
};
#endif