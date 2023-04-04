#include "Uniforms.glsl"

#ifdef COMPILEVS

#include "Transform.glsl"

in vec3 position;

#else

out vec4 fragColor;

#endif

void vert()
{
    mat3x4 world = GetWorldMatrix();
    
    vec3 worldPos = vec4(position, 1.0) * world;
    gl_Position = vec4(worldPos, 1.0) * viewProjMatrix;
}

void frag()
{
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
