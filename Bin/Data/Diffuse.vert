#version 150

#include "CommonCode.vert"

in vec3 position;

void main()
{
    vec3 worldPos = vec4(position, 1.0) * worldMatrix;
    gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
}
