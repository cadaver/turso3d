#version 150

#include "CommonCode.frag"

uniform sampler2D diffuseTex0;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
#ifdef SHADOW
in vec4 vShadowPos[4];
#endif
out vec4 fragColor;

void main()
{
    #ifdef SHADOW
    vec4 totalLight = CalculateLighting(vWorldPos, normalize(vNormal), vShadowPos);
    #else
    vec4 totalLight = CalculateLighting(vWorldPos, normalize(vNormal));
    #endif

    fragColor = totalLight * texture(diffuseTex0, vTexCoord);
}
