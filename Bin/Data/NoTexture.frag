#version 150

#include "CommonCode.frag"

in vec3 vWorldPos;
in vec3 vNormal;
out vec4 fragColor;

void main()
{
    return CalculateLighting(vWorldPos, normalize(vNormal));
}
