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
    vec4 shadowParameters[4];
    vec4 pointShadowParameters[4];
    vec4 dirShadowSplits;
    vec4 dirShadowFade;
};

uniform sampler2DShadow shadowMap8[4];
uniform samplerCube faceSelectionTex12;
uniform samplerCube faceSelectionTex13;

float SampleShadowMap(int index, vec4 shadowPos)
{
    vec4 offsets1 = vec4(shadowParameters[index].xy * shadowPos.w, 0, 0);
    vec4 offsets2 = vec4(vec2(shadowParameters[index].x, -shadowParameters[index].y) * shadowPos.w, 0, 0);

    return (textureProj(shadowMap8[index], shadowPos + offsets1) +
        textureProj(shadowMap8[index], shadowPos - offsets1) +
        textureProj(shadowMap8[index], shadowPos + offsets2) +
        textureProj(shadowMap8[index], shadowPos - offsets2)) * 0.25;
}

vec4 GetPointShadowPos(int index, vec4 worldPos)
{
    vec3 lightVec = lightPositions[index].xyz - worldPos.xyz;
    vec3 axis = texture(faceSelectionTex12, lightVec).xyz;
    vec4 adjust = texture(faceSelectionTex13, lightVec);
    float depth = abs(dot(lightVec, axis));
    // Scale back to avoid cube edge artifacts
    /// \todo Should use shadow map texel size instead of a constant
    vec3 normLightVec = (lightVec / depth) * 0.99;
    vec2 coords = vec2(dot(normLightVec.zxx, axis), dot(normLightVec.yzy, axis)) * adjust.xy + adjust.zw;
    return vec4(coords * pointShadowParameters[index].xy + pointShadowParameters[index].zw, shadowParameters[index].z + shadowParameters[index].w / depth, 1);
}

float CalculatePointAtten(int index, vec4 worldPos, vec3 normal)
{
    vec3 lightVec = (lightPositions[index].xyz - worldPos.xyz) * lightAttenuations[index].x;
    float lightDist = length(lightVec);
    vec3 localDir = lightVec / lightDist;
    float NdotL = clamp(dot(normal, localDir), 0.0, 1.0);
    return NdotL * clamp(1.0 - lightDist * lightDist, 0.0, 1.0);
}

float CalculateSpotAtten(int index, vec4 worldPos, vec3 normal)
{
    vec3 lightVec = (lightPositions[index].xyz - worldPos.xyz) * lightAttenuations[index].x;
    float lightDist = length(lightVec);
    vec3 localDir = lightVec / lightDist;
    float NdotL = clamp(dot(normal, localDir), 0.0, 1.0);
    float spotEffect = dot(localDir, lightDirections[index].xyz);
    float spotAtten = clamp((spotEffect - lightAttenuations[index].y) * lightAttenuations[index].z, 0.0, 1.0);
    return NdotL * spotAtten * clamp(1.0 - lightDist * lightDist, 0.0, 1.0);
}

vec3 CalculateDirLight(int index, vec4 worldPos, vec3 normal)
{
    float NdotL = clamp(dot(normal, lightDirections[index].xyz), 0.0, 1.0);
    return NdotL * lightColors[index].rgb;
}

vec3 CalculatePointLight(int index, vec4 worldPos, vec3 normal)
{
    return CalculatePointAtten(index, worldPos, normal) * lightColors[index].rgb;
}

vec3 CalculateSpotLight(int index, vec4 worldPos, vec3 normal)
{
    return CalculateSpotAtten(index, worldPos, normal) * lightColors[index].rgb;
}

#ifdef NUMSHADOWCOORDS
vec4 GetDirShadowPos(float depth, vec4 shadowPos[NUMSHADOWCOORDS])
{
    #if NUMSHADOWCOORDS == 1
    return shadowPos[0];
    #elif NUMSHADOWCOORDS == 2
    if (depth < dirShadowSplits.x)
        return shadowPos[0];
    else
        return shadowPos[1];
    #elif NUMSHADOWCOORDS == 3
    if (depth < dirShadowSplits.x)
        return shadowPos[0];
    else if (depth < dirShadowSplits.y)
        return shadowPos[1];
    else
        return shadowPos[2];
    #elif NUMSHADOWCOORDS == 4
    if (depth < dirShadowSplits.x)
        return shadowPos[0];
    else if (depth < dirShadowSplits.y)
        return shadowPos[1];
    else if (depth < dirShadowSplits.z)
        return shadowPos[2];
    else
        return shadowPos[3];
    #endif
}

vec3 CalculateShadowDirLight(int index, vec4 worldPos, vec3 normal, vec4 shadowPos[NUMSHADOWCOORDS])
{
    float atten = clamp(dot(normal, lightDirections[index].xyz), 0, 1);
    float fade = clamp(worldPos.w * dirShadowFade.y - dirShadowFade.x, 0, 1);
    if (atten > 0 && fade < 1)
        atten *= clamp(fade + SampleShadowMap(index, GetDirShadowPos(worldPos.w, shadowPos)), 0, 1);
    return atten * lightColors[index].rgb;
}

vec3 CalculateShadowSpotLight(int index, vec4 worldPos, vec3 normal, vec4 shadowPos)
{
    float atten = CalculateSpotAtten(index, worldPos, normal);
    if (atten > 0)
        atten *= SampleShadowMap(index, shadowPos);
    return atten * lightColors[index].rgb;
}
#endif

vec3 CalculateShadowPointLight(int index, vec4 worldPos, vec3 normal)
{
    float atten = CalculatePointAtten(index, worldPos, normal);
    if (atten > 0)
        atten *= SampleShadowMap(index, GetPointShadowPos(index, worldPos));
    return atten * lightColors[index].rgb;
}

#ifdef NUMSHADOWCOORDS
vec4 CalculateLighting(vec4 worldPos, vec3 normal, vec4 shadowPos[NUMSHADOWCOORDS])
#else
vec4 CalculateLighting(vec4 worldPos, vec3 normal)
#endif
{
    #ifdef NUMSHADOWCOORDS
    int shadowIndex = 0;
    #endif

    vec4 totalLight = vec4(0, 0, 0, 1);
    
    #ifdef AMBIENT
    totalLight.rgb += ambientColor;
    #endif

    #ifdef DIRLIGHT0
    #ifdef SHADOW0
    totalLight.rgb += CalculateShadowDirLight(0, worldPos, normal, shadowPos);
    #else
    totalLight.rgb += CalculateDirLight(0, worldPos, normal);
    #endif
    #endif
    #ifdef DIRLIGHT1
    #ifdef SHADOW1
    totalLight.rgb += CalculateShadowDirLight(1, worldPos, normal, shadowPos);
    #else
    totalLight.rgb += CalculateDirLight(1, worldPos, normal);
    #endif
    #endif
    #ifdef DIRLIGHT2
    #ifdef SHADOW2
    totalLight.rgb += CalculateShadowDirLight(2, worldPos, normal, shadowPos);
    #else
    totalLight.rgb += CalculateDirLight(2, worldPos, normal);
    #endif
    #endif
    #ifdef DIRLIGHT3
    #ifdef SHADOW3
    totalLight.rgb += CalculateShadowDirLight(3, worldPos, normal, shadowPos);
    #else
    totalLight.rgb += CalculateDirLight(3, worldPos, normal);
    #endif
    #endif

    #ifdef POINTLIGHT0
    #ifdef SHADOW0
    totalLight.rgb += CalculateShadowPointLight(0, worldPos, normal);
    #else
    totalLight.rgb += CalculatePointLight(0, worldPos, normal);
    #endif
    #endif
    #ifdef POINTLIGHT1
    #ifdef SHADOW1
    totalLight.rgb += CalculateShadowPointLight(1, worldPos, normal);
    #else
    totalLight.rgb += CalculatePointLight(1, worldPos, normal);
    #endif
    #endif
    #ifdef POINTLIGHT2
    #ifdef SHADOW2
    totalLight.rgb += CalculateShadowPointLight(2, worldPos, normal);
    #else
    totalLight.rgb += CalculatePointLight(2, worldPos, normal);
    #endif
    #endif
    #ifdef POINTLIGHT3
    #ifdef SHADOW3
    totalLight.rgb += CalculateShadowPointLight(3, worldPos, normal);
    #else
    totalLight.rgb += CalculatePointLight(3, worldPos, normal);
    #endif
    #endif

    #ifdef SPOTLIGHT0
    #ifdef SHADOW0
    totalLight.rgb += CalculateShadowSpotLight(0, worldPos, normal, shadowPos[shadowIndex++]);
    #else
    totalLight.rgb += CalculateSpotLight(0, worldPos, normal);
    #endif
    #endif
    #ifdef SPOTLIGHT1
    #ifdef SHADOW1
    totalLight.rgb += CalculateShadowSpotLight(1, worldPos, normal, shadowPos[shadowIndex++]);
    #else
    totalLight.rgb += CalculateSpotLight(1, worldPos, normal);
    #endif
    #endif
    #ifdef SPOTLIGHT2
    #ifdef SHADOW2
    totalLight.rgb += CalculateShadowSpotLight(2, worldPos, normal, shadowPos[shadowIndex++]);
    #else
    totalLight.rgb += CalculateSpotLight(2, worldPos, normal);
    #endif
    #endif
    #ifdef SPOTLIGHT3
    #ifdef SHADOW3
    totalLight.rgb += CalculateShadowSpotLight(3, worldPos, normal, shadowPos[shadowIndex++]);
    #else
    totalLight.rgb += CalculateSpotLight(3, worldPos, normal);
    #endif
    #endif

    return totalLight;
}
