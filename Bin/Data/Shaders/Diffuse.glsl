#ifdef COMPILEVS

#include "Transform.glsl"

in vec3 position;
in vec3 normal;
in vec2 texCoord;

out vec4 vWorldPos;
out vec3 vNormal;
out vec3 vViewNormal;
out vec2 vTexCoord;

#else

#include "Lighting.glsl"

in vec4 vWorldPos;
in vec3 vNormal;
in vec3 vViewNormal;
in vec2 vTexCoord;
out vec4 fragColor[2];

uniform sampler2D diffuseTex0;
uniform vec4 matDiffColor;

#endif

void vert()
{
#ifdef INSTANCED
    mat3x4 worldMatrix = mat3x4(texCoord3, texCoord4, texCoord5);
#endif

    vWorldPos.xyz = vec4(position, 1.0) * worldMatrix;
    vNormal = normalize((vec4(normal, 0.0) * worldMatrix));
    vViewNormal = (vec4(vNormal, 0.0) * viewMatrix) * 0.5 + 0.5;
    vTexCoord = texCoord;
    gl_Position = vec4(vWorldPos.xyz, 1.0) * viewProjMatrix;
    vWorldPos.w = CalculateDepth(gl_Position);
    //vScreenUv = CalculateScreenPos(gl_Position) * 0.5 + 0.5;
}

void frag()
{
    fragColor[0] = vec4(matDiffColor.rgb * texture(diffuseTex0, vTexCoord).rgb * CalculateLighting(vWorldPos, vNormal), matDiffColor.a);
    fragColor[1] = vec4(vViewNormal, 1.0);
}
