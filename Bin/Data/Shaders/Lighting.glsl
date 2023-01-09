#ifdef SHADOWDIRLIGHT
uniform vec4 dirLightData[12];
#else
uniform vec4 dirLightData[2];
#endif

#ifdef NUMLIGHTS
uniform vec4 lightData[NUMLIGHTS*9];
#endif

uniform sampler2DShadow dirShadowTex8;
uniform sampler2DShadow shadowTex9;
uniform samplerCube faceSelectionTex10;
uniform samplerCube faceSelectionTex11;
uniform sampler2D ssaoTex12;

float SampleShadowMap(sampler2DShadow shadowTex, vec4 shadowPos, vec4 parameters)
{
#ifdef HQSHADOW
    vec4 offsets1 = vec4(2.0 * parameters.xy * shadowPos.w, 0.0, 0.0);
    vec4 offsets2 = vec4(2.0 * parameters.x * shadowPos.w, -2.0 * parameters.y * shadowPos.w, 0.0, 0.0);
    vec4 offsets3 = vec4(2.0 * parameters.x * shadowPos.w, 0.0, 0.0, 0.0);
    vec4 offsets4 = vec4(0.0, 2.0 * parameters.y * shadowPos.w, 0.0, 0.0);

    return smoothstep(0.0, 1.0, (
        textureProjLod(shadowTex, shadowPos, 0.0) +
        textureProjLod(shadowTex, shadowPos + offsets1, 0.0) +
        textureProjLod(shadowTex, shadowPos - offsets1, 0.0) +
        textureProjLod(shadowTex, shadowPos + offsets2, 0.0) +
        textureProjLod(shadowTex, shadowPos - offsets2, 0.0) +
        textureProjLod(shadowTex, shadowPos + offsets3, 0.0) +
        textureProjLod(shadowTex, shadowPos - offsets3, 0.0) +
        textureProjLod(shadowTex, shadowPos + offsets4, 0.0) +
        textureProjLod(shadowTex, shadowPos - offsets4, 0.0)
    ) * 0.1111);
#else
    vec4 offsets1 = vec4(parameters.xy * shadowPos.w, 0.0, 0.0);
    vec4 offsets2 = vec4(parameters.x * shadowPos.w, -parameters.y * shadowPos.w, 0.0, 0.0);

    return (
        textureProjLod(shadowTex, shadowPos + offsets1, 0.0) +
        textureProjLod(shadowTex, shadowPos - offsets1, 0.0) +
        textureProjLod(shadowTex, shadowPos + offsets2, 0.0) +
        textureProjLod(shadowTex, shadowPos - offsets2, 0.0)
    ) * 0.25;
#endif
}

#ifdef SHADOWDIRLIGHT
void CalculateShadowDirLight(vec4 worldPos, vec3 normal, inout vec3 accumulatedLight)
{
    vec4 lightDirection = dirLightData[0];
    vec3 lightColor = dirLightData[1].rgb;

    float NdotL = dot(normal, lightDirection.xyz);
    if (NdotL <= 0.0)
        return;

    vec4 shadowSplits = dirLightData[2];
    vec4 shadowParameters = dirLightData[3];

    if (worldPos.w < shadowSplits.y)
    {
        int matIndex = worldPos.w > shadowSplits.x ? 8 : 4;

        mat4x4 shadowMatrix = mat4x4(dirLightData[matIndex], dirLightData[matIndex+1], dirLightData[matIndex+2], dirLightData[matIndex+3]);
        float shadowFade = shadowParameters.z + clamp((worldPos.w - shadowSplits.z) * shadowSplits.w, 0.0, 1.0);
        NdotL *= clamp(shadowFade + SampleShadowMap(dirShadowTex8, vec4(worldPos.xyz, 1.0) * shadowMatrix, shadowParameters), 0.0, 1.0);
    }

    accumulatedLight += NdotL * lightColor;
}
#endif

void CalculateDirLight(vec4 worldPos, vec3 normal, inout vec3 accumulatedLight)
{
    vec3 lightDirection = dirLightData[0].xyz;
    vec3 lightColor = dirLightData[1].rgb;

    float NdotL = dot(normal, lightDirection);
    if (NdotL <= 0.0)
        return;

    accumulatedLight += NdotL * lightColor;
}

#ifdef NUMSHADOWLIGHTS
vec4 GetPointShadowPos(int index, vec3 lightVec)
{
    vec4 pointParameters = lightData[index*9+5];
    vec4 pointParameters2 = lightData[index*9+6];
    float zoom = pointParameters2.x;
    float q = pointParameters2.y;
    float r = pointParameters2.z;

    vec3 axis = textureLod(faceSelectionTex10, lightVec, 0.0).xyz;
    vec4 adjust = textureLod(faceSelectionTex11, lightVec, 0.0);
    float depth = abs(dot(lightVec, axis));
    vec3 normLightVec = (lightVec / depth) * zoom;
    vec2 coords = vec2(dot(normLightVec.zxx, axis), dot(normLightVec.yzy, axis)) * adjust.xy + adjust.zw;
    return vec4(coords * pointParameters.xy + pointParameters.zw, q + r / depth, 1.0);
}

void CalculateShadowLight(int i, vec4 worldPos, vec3 normal, inout vec3 accumulatedLight)
{
    vec3 lightPosition = lightData[i*9].xyz;
    vec4 lightAttenuation = lightData[i*9+2];
    vec3 lightColor = lightData[i*9+3].rgb;

    vec3 lightVec = lightPosition - worldPos.xyz;
    vec3 scaledLightVec = lightVec * lightAttenuation.x;
    vec3 lightDir = normalize(lightVec);
    float atten = 1.0 - dot(scaledLightVec, scaledLightVec);
    float NdotL = dot(normal, lightDir);

    if (atten <= 0.0 || NdotL <= 0.0)
        return;

    if (lightAttenuation.y > 0.0)
    {
        vec3 lightSpotDirection = lightData[i*9+1].xyz;
        float spotEffect = dot(lightDir, lightSpotDirection);
        float spotAtten = (spotEffect - lightAttenuation.y) * lightAttenuation.z;
        if (spotAtten <= 0.0)
            return;

        atten *= spotAtten;

        vec4 shadowParameters = lightData[i*9+4];
        mat4x4 shadowMatrix = mat4x4(lightData[i*9+5], lightData[i*9+6], lightData[i*9+7], lightData[i*9+8]);
        atten *= clamp(shadowParameters.z + SampleShadowMap(shadowTex9, vec4(worldPos.xyz, 1.0) * shadowMatrix, shadowParameters), 0.0, 1.0);
    }
    else
    {
        vec4 shadowParameters = lightData[i*9+4];
        atten *= clamp(shadowParameters.z + SampleShadowMap(shadowTex9, GetPointShadowPos(i, lightVec), shadowParameters), 0.0, 1.0);
    }

    accumulatedLight += atten * NdotL * lightColor;
}
#endif

#ifdef NUMLIGHTS
void CalculateLight(int i, vec4 worldPos, vec3 normal, inout vec3 accumulatedLight)
{
    vec3 lightPosition = lightData[i*9].xyz;
    vec4 lightAttenuation = lightData[i*9+2];
    vec3 lightColor = lightData[i*9+3].rgb;

    vec3 lightVec = lightPosition - worldPos.xyz;
    vec3 scaledLightVec = lightVec * lightAttenuation.x;
    vec3 lightDir = normalize(lightVec);
    float atten = 1.0 - dot(scaledLightVec, scaledLightVec);
    float NdotL = dot(normal, lightDir);

    if (atten <= 0.0 || NdotL <= 0.0)
        return;

    if (lightAttenuation.y > 0.0)
    {
        vec3 lightSpotDirection = lightData[i*9+1].xyz;
        float spotEffect = dot(lightDir, lightSpotDirection);
        float spotAtten = (spotEffect - lightAttenuation.y) * lightAttenuation.z;
        if (spotAtten <= 0.0)
            return;

        atten *= spotAtten;
    }

    accumulatedLight += atten * NdotL * lightColor;
}
#endif

vec3 CalculateLighting(vec4 worldPos, vec3 normal)
{
#ifdef AMBIENT
    vec3 accumulatedLight = vec3(0.15, 0.15, 0.15);
#ifdef SHADOWDIRLIGHT
    CalculateShadowDirLight(worldPos, normal, accumulatedLight);
#else
    CalculateDirLight(worldPos, normal, accumulatedLight);
#endif

#else
    vec3 accumulatedLight = vec3(0.0, 0.0, 0.0);
#endif

#ifdef NUMLIGHTS
#if NUMLIGHTS >= 1
#if NUMSHADOWLIGHTS >= 1
    CalculateShadowLight(0, worldPos, normal, accumulatedLight);
#else
    CalculateLight(0, worldPos, normal, accumulatedLight);
#endif
#endif

#if NUMLIGHTS >= 2
#if NUMSHADOWLIGHTS >= 2
    CalculateShadowLight(1, worldPos, normal, accumulatedLight);
#else
    CalculateLight(1, worldPos, normal, accumulatedLight);
#endif
#endif

#if NUMLIGHTS >= 3
#if NUMSHADOWLIGHTS >= 3
    CalculateShadowLight(2, worldPos, normal, accumulatedLight);
#else
    CalculateLight(2, worldPos, normal, accumulatedLight);
#endif
#endif

#if NUMLIGHTS >= 4
#if NUMSHADOWLIGHTS >= 4
    CalculateShadowLight(3, worldPos, normal, accumulatedLight);
#else
    CalculateLight(3, worldPos, normal, accumulatedLight);
#endif
#endif
#endif

    return accumulatedLight;
}
