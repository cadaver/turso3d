#ifdef COMPILEVS

uniform mat4 viewProjMatrix;

in vec3 position;
in vec4 color;

out vec4 vColor;

#else

in vec4 vColor;
out vec4 fragColor;

#endif

void vert()
{
    vColor = color;
    gl_Position = vec4(position, 1.0) * viewProjMatrix;
}

void frag()
{
    fragColor = vec4(vColor.rgb, 1.0);
}
