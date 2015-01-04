#version 150

#include "CommonCode.vert"

in vec3 position;
#ifdef INSTANCED
in vec4 texCoord4;
in vec4 texCoord5;
in vec4 texCoord6;
#endif

void main()
{
    #ifdef INSTANCED
    mat3x4 instanceWorldMatrix = mat3x4(texCoord4, texCoord5, texCoord6);
    vec3 worldPos = vec4(position, 1.0) * instanceWorldMatrix;
    #else
    vec3 worldPos = vec4(position, 1.0) * worldMatrix;
    #endif

    gl_Position = vec4(worldPos.xyz, 1.0) * viewProjMatrix;
}
