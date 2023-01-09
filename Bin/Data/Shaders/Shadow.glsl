#ifdef COMPILEVS

#include "Transform.glsl"

in vec3 position;

#else

out vec4 fragColor;

#endif

void vert()
{
#ifdef INSTANCED
    mat3x4 worldMatrix = mat3x4(texCoord3, texCoord4, texCoord5);
#endif

    vec3 worldPos = vec4(position, 1.0) * worldMatrix;
    gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
}

void frag()
{
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
