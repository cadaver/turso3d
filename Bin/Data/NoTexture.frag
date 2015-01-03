#version 150

#include "CommonCode.frag"

in vec4 vWorldPos;
in vec3 vNormal;
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
    
    fragColor = totalLight;
}
