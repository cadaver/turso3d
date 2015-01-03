#version 150

#include "CommonCode.frag"

in vec3 vWorldPos;
in vec3 vNormal;
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
    
    fragColor = totalLight;
}
