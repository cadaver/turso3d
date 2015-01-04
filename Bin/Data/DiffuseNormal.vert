#version 150

#include "CommonCode.vert"

in vec3 position;
in vec3 normal;
in vec4 tangent;
in vec2 texCoord;
#ifdef INSTANCED
in vec4 texCoord4;
in vec4 texCoord5;
in vec4 texCoord6;
#endif
out vec4 vWorldPos;
out vec3 vNormal;
out vec4 vTangent;
out vec2 vTexCoord;
#ifdef NUMSHADOWCOORDS
out vec4 vShadowPos[NUMSHADOWCOORDS];
#endif

void main()
{
    #ifdef INSTANCED
    mat3x4 instanceWorldMatrix = mat3x4(texCoord4, texCoord5, texCoord6);
    vWorldPos.xyz = vec4(position, 1.0) * instanceWorldMatrix;
    vNormal = normalize(vec4(normal, 0.0) * instanceWorldMatrix);
    vTangent = vec4(normalize(vec4(tangent.xyz, 0.0) * instanceWorldMatrix), tangent.w);
    #else
    vWorldPos.xyz = vec4(position, 1.0) * worldMatrix;
    vNormal = normalize(vec4(normal, 0.0) * worldMatrix);
    vTangent = vec4(normalize(vec4(tangent.xyz, 0.0) * worldMatrix), tangent.w);
    #endif

    gl_Position = vec4(vWorldPos.xyz, 1.0) * viewProjMatrix;
    vWorldPos.w = CalculateDepth(gl_Position);
    vTexCoord = texCoord;
    #ifdef NUMSHADOWCOORDS
    for (int i = 0; i < NUMSHADOWCOORDS; ++i)
        vShadowPos[i] = vec4(vWorldPos, 1.0) * shadowMatrices[i];
    #endif
}
