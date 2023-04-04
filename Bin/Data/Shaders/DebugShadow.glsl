#ifdef COMPILEVS

uniform mat4 worldViewProjMatrix;

in vec3 position;
in vec2 texCoord;

out vec2 vTexCoord;

#else

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D diffuseTex0;

#endif

void vert()
{
    vTexCoord = texCoord;
    gl_Position = vec4(position, 1.0) * worldViewProjMatrix;
}

void frag()
{
    float depth = texture(diffuseTex0, vTexCoord).r;
    // Raise to 2nd power to see differences better
    depth *= depth;
    fragColor = vec4(depth, depth, depth, 1.0);
}
