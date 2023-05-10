#include "Uniforms.glsl"

#ifdef COMPILEVS

#include "Transform.glsl"

in vec3 position;
in vec3 normal;
in vec2 texCoord;

out vec4 vWorldPos;
out vec3 vNormal;
out vec3 vViewNormal;
out vec2 vTexCoord;
noperspective out vec2 vScreenPos;

#else

#include "Lighting.glsl"

in vec4 vWorldPos;
in vec3 vNormal;
in vec3 vViewNormal;
in vec2 vTexCoord;
noperspective in vec2 vScreenPos;
out vec4 fragColor[2];

layout(std140) uniform PerMaterialData3
{
    vec4 matDiffColor;
    vec4 matSpecColor;
};

uniform sampler2D diffuseTex0;

#endif

void vert()
{
    mat3x4 world = GetWorldMatrix();
    
    vWorldPos.xyz = vec4(position, 1.0) * world;
    vNormal = normalize((vec4(normal, 0.0) * world));
    vViewNormal = (vec4(vNormal, 0.0) * viewMatrix) * 0.5 + 0.5;
    vTexCoord = texCoord;
    gl_Position = vec4(vWorldPos.xyz, 1.0) * viewProjMatrix;
    vWorldPos.w = CalculateDepth(gl_Position);
    vScreenPos = CalculateScreenPos(gl_Position);
}

void frag()
{
    vec3 diffuseLight;
    vec3 specularLight;
    CalculateLighting(vWorldPos, vNormal, vScreenPos, matDiffColor, matSpecColor, diffuseLight, specularLight);

    vec3 finalColor = texture(diffuseTex0, vTexCoord).rgb * diffuseLight + specularLight;

    fragColor[0] = vec4(mix(fogColor, finalColor, GetFogFactor(vWorldPos.w)), matDiffColor.a);
    fragColor[1] = vec4(vViewNormal, 1.0);
}
