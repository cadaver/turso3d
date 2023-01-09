uniform mat3x4 viewMatrix;
uniform mat4x4 projectionMatrix;
uniform mat4x4 viewProjMatrix;
uniform vec4 depthParameters;

#ifdef INSTANCED
in vec4 texCoord3;
in vec4 texCoord4;
in vec4 texCoord5;
#else
uniform mat3x4 worldMatrix;
#endif

float CalculateDepth(vec4 outPos)
{
    return dot(depthParameters.zw, outPos.zw);
}

vec2 CalculateScreenPos(vec4 outPos)
{
    return outPos.xy / outPos.w;
}
