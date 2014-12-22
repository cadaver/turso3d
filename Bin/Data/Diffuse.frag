#version 150

uniform sampler2D diffuseTex0;

in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    fragColor = texture(diffuseTex0, vTexCoord);
}
