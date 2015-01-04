#version 150

#include "CommonCode.frag"

uniform sampler2D diffuseTex0;
uniform sampler2D normalTex1;

in vec4 vWorldPos;
in vec3 vNormal;
in vec4 vTangent;
in vec2 vTexCoord;
#ifdef NUMSHADOWCOORDS
in vec4 vShadowPos[NUMSHADOWCOORDS];
#endif
out vec4 fragColor;

vec3 DecodeNormal(vec4 normalInput)
{
    vec3 normal;
    normal.xy = normalInput.ag * 2.0 - 1.0;
    normal.z = sqrt(max(1.0 - dot(normal.xy, normal.xy), 0.0));
    return normal;
}

void main()
{
    mat3 tbn = mat3(vTangent.xyz, cross(vTangent.xyz, vNormal) * vTangent.w, vNormal);
    vec3 normal = normalize(DecodeNormal(texture(normalTex1, vTexCoord)) * tbn);

    #ifdef NUMSHADOWCOORDS
    vec4 totalLight = CalculateLighting(vWorldPos, normal, vShadowPos);
    #else
    vec4 totalLight = CalculateLighting(vWorldPos, normal);
    #endif

    fragColor = totalLight * texture(diffuseTex0, vTexCoord);
}
