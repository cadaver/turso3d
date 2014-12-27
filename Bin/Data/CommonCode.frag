layout(std140) uniform PerFramePS0
{
    vec3 ambientColor;
};

layout(std140) uniform LightsPS3
{
    vec4 lightPositions[4];
    vec4 lightDirections[4];
    vec4 lightColors[4];
};

vec4 CalculateLighting(vec3 normal)
{
    vec4 totalLight = vec4(0, 0, 0, 1);
    #ifdef AMBIENT
    totalLight.rgb += ambientColor;
    #endif
    #ifdef DIRLIGHT0
    totalLight.rgb += lightColors[0].rgb * clamp(dot(lightDirections[0].xyz, normal), 0, 1);
    #endif
    #ifdef DIRLIGHT1
    totalLight.rgb += lightColors[1].rgb * clamp(dot(lightDirections[1].xyz, normal), 0, 1);
    #endif
    #ifdef DIRLIGHT2
    totalLight.rgb += lightColors[2].rgb * clamp(dot(lightDirections[2].xyz, normal), 0, 1);
    #endif
    #ifdef DIRLIGHT3
    totalLight.rgb += lightColors[3].rgb * clamp(dot(lightDirections[3].xyz, normal), 0, 1);
    #endif
    return totalLight;
}