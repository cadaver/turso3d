#version 150

#include "CommonCode.vert"

in vec3 position;
in vec2 texCoord;
out vec2 vTexCoord;

void main()
{
    vec3 worldPos = vec4(position, 1.0) * worldMatrix;
    gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
    vTexCoord = texCoord;
}
