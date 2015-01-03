#version 150

#include "CommonCode.frag"

uniform sampler2D diffuseTex0;

in vec4 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
#ifdef NUMSHADOWCOORDS
in vec4 vShadowPos[NUMSHADOWCOORDS];
#endif
out vec4 fragColor;

void main()
{
    #ifdef NUMSHADOWCOORDS
    vec4 totalLight = CalculateLighting(vWorldPos, normalize(vNormal), vShadowPos);
    #else
    vec4 totalLight = CalculateLighting(vWorldPos, normalize(vNormal));
    #endif

    fragColor = totalLight * texture(diffuseTex0, vTexCoord);
}
