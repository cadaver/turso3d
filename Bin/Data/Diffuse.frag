#version 150

#include "CommonCode.frag"

uniform sampler2D diffuseTex0;

in vec2 vTexCoord;
in vec3 vNormal;
out vec4 fragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    vec4 totalLight = CalculateLighting(normal);
    fragColor = totalLight * texture(diffuseTex0, vTexCoord);
}
