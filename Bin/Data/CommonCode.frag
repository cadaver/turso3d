layout(std140) uniform PerFramePS0
{
    vec3 ambientColor;
};

layout(std140) uniform LightsPS3
{
    vec4 lightPositions[4];
    vec4 lightDirections[4];
    vec4 lightAttenuations[4];
    vec4 lightColors[4];
};

vec3 CalculateDirLight(int index, vec3 worldPos, vec3 normal)
{
    float NdotL = clamp(dot(normal, lightDirections[index].xyz), 0.0, 1.0);
    return NdotL * lightColors[index].rgb;
}

vec3 CalculatePointLight(int index, vec3 worldPos, vec3 normal)
{
    vec3 lightVec = (lightPositions[index].xyz - worldPos) * lightAttenuations[index].x;
    float lightDist = length(lightVec);
    vec3 localDir = lightVec / lightDist;
    float NdotL = clamp(dot(normal, localDir), 0.0, 1.0);
    return NdotL * clamp(1.0 - lightDist * lightDist, 0.0, 1.0) * lightColors[index].rgb;
}

vec3 CalculateSpotLight(int index, vec3 worldPos, vec3 normal)
{
    vec3 lightVec = (lightPositions[index].xyz - worldPos) * lightAttenuations[index].x;
    float lightDist = length(lightVec);
    vec3 localDir = lightVec / lightDist;
    float NdotL = clamp(dot(normal, localDir), 0.0, 1.0);
    float spotEffect = dot(localDir, lightDirections[index].xyz);
    float spotAtten = clamp((spotEffect - lightAttenuations[index].y) * lightAttenuations[index].z, 0.0, 1.0);
    return NdotL * spotAtten * clamp(1.0 - lightDist * lightDist, 0.0, 1.0) * lightColors[index].rgb;
}

vec4 CalculateLighting(vec3 worldPos, vec3 normal)
{
    vec4 totalLight = vec4(0, 0, 0, 1);
    #ifdef AMBIENT
    totalLight.rgb += ambientColor;
    #endif
    #ifdef DIRLIGHT0
    totalLight.rgb += CalculateDirLight(0, worldPos, normal);
    #endif
    #ifdef DIRLIGHT1
    totalLight.rgb += CalculateDirLight(1, worldPos, normal);
    #endif
    #ifdef DIRLIGHT2
    totalLight.rgb += CalculateDirLight(2, worldPos, normal);
    #endif
    #ifdef DIRLIGHT3
    totalLight.rgb += CalculateDirLight(3, worldPos, normal);
    #endif
    #ifdef POINTLIGHT0
    totalLight.rgb += CalculatePointLight(0, worldPos, normal);
    #endif
    #ifdef POINTLIGHT1
    totalLight.rgb += CalculatePointLight(1, worldPos, normal);
    #endif
    #ifdef POINTLIGHT2
    totalLight.rgb += CalculatePointLight(2, worldPos, normal);
    #endif
    #ifdef POINTLIGHT3
    totalLight.rgb += CalculatePointLight(3, worldPos, normal);
    #endif
    #ifdef SPOTLIGHT0
    totalLight.rgb += CalculateSpotLight(0, worldPos, normal);
    #endif
    #ifdef SPOTLIGHT1
    totalLight.rgb += CalculateSpotLight(1, worldPos, normal);
    #endif
    #ifdef SPOTLIGHT2
    totalLight.rgb += CalculateSpotLight(2, worldPos, normal);
    #endif
    #ifdef SPOTLIGHT3
    totalLight.rgb += CalculateSpotLight(3, worldPos, normal);
    #endif
    return totalLight;
}

