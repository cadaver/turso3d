#version 150

#include "CommonCode.vert"

in vec3 position;
in vec3 normal;
#ifdef INSTANCED
in vec4 texCoord4;
in vec4 texCoord5;
in vec4 texCoord6;
#endif
out vec3 vWorldPos;
out vec3 vNormal;
#ifdef SHADOW
out vec4 vShadowPos[4];
#endif

void main()
{
    #ifdef INSTANCED
    mat3x4 instanceWorldMatrix = mat3x4(texCoord4, texCoord5, texCoord6);
    mat3 normalMatrix = mat3(instanceWorldMatrix[0].xyz, instanceWorldMatrix[1].xyz, instanceWorldMatrix[2].xyz);
    vWorldPos = vec4(position, 1.0) * instanceWorldMatrix;
    vNormal = normal * normalMatrix;
    #else
    mat3 normalMatrix = mat3(worldMatrix[0].xyz, worldMatrix[1].xyz, worldMatrix[2].xyz);
    vWorldPos = vec4(position, 1.0) * worldMatrix;
    vNormal = normal * normalMatrix;
    #endif

    gl_Position = vec4(vWorldPos, 1.0) * viewProjMatrix;
    #ifdef SHADOW
    for (int i = 0; i < 4; ++i)
        vShadowPos[i] = vec4(vWorldPos, 1.0) * shadowMatrices[i];
    #endif
}
