#version 150

#include "CommonCode.vert"

in vec4 position;

void main()
{
    gl_Position = viewProjMatrix * worldMatrix * position;
}
