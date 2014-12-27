#version 150

#include "CommonCode.frag"

uniform sampler2D diffuseTex0;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    vec4 totalLight = CalculateLighting(vWorldPos, normalize(vNormal));
    fragColor = totalLight * texture(diffuseTex0, vTexCoord);
}
