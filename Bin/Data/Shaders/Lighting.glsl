uniform sampler2DShadow dirShadowTex8;
uniform sampler2DShadow shadowTex9;
uniform samplerCube faceSelectionTex10;
uniform samplerCube faceSelectionTex11;
uniform usampler3D clusterTex12;

vec3 CalculateClusterPos(vec2 screenPos, float depth)
{
    return vec3(
        screenPos.x,
        screenPos.y,
        sqrt(depth)
    );
}

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

void CalculateDirLight(vec4 worldPos, vec3 normal, inout vec3 accumulatedLight)
{
    vec4 lightDirection = dirLightData[0];
    vec3 lightColor = dirLightData[1].rgb;

    float NdotL = dot(normal, lightDirection.xyz);
    if (NdotL <= 0.0)
        return;

    vec4 shadowSplits = dirLightData[2];
    vec4 shadowParameters = dirLightData[3];

    if (shadowParameters.z < 1.0 && worldPos.w < shadowSplits.y)
    {
        int matIndex = worldPos.w > shadowSplits.x ? 8 : 4;

        mat4 shadowMatrix = mat4(dirLightData[matIndex], dirLightData[matIndex+1], dirLightData[matIndex+2], dirLightData[matIndex+3]);
        float shadowFade = shadowParameters.z + clamp((worldPos.w - shadowSplits.z) * shadowSplits.w, 0.0, 1.0);
        NdotL *= clamp(shadowFade + SampleShadowMap(dirShadowTex8, vec4(worldPos.xyz, 1.0) * shadowMatrix, shadowParameters), 0.0, 1.0);
    }

    accumulatedLight += NdotL * lightColor;
}

vec4 GetPointShadowPos(uint index, vec3 lightVec)
{
    vec4 pointParameters = lights[index].shadowMatrix[0];
    vec4 pointParameters2 = lights[index].shadowMatrix[1];
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

void CalculateLight(uint index, vec4 worldPos, vec3 normal, inout vec3 accumulatedLight)
{
    vec3 lightPosition = lights[index].position.xyz;
    vec4 lightAttenuation = lights[index].attenuation;
    vec3 lightColor = lights[index].color.rgb;

    vec3 lightVec = lightPosition - worldPos.xyz;
    vec3 scaledLightVec = lightVec * lightAttenuation.x;
    vec3 lightDir = normalize(lightVec);
    float atten = 1.0 - dot(scaledLightVec, scaledLightVec);
    float NdotL = dot(normal, lightDir);

    if (atten <= 0.0 || NdotL <= 0.0)
        return;

    vec4 shadowParameters = lights[index].shadowParameters;

    if (lightAttenuation.y > 0.0)
    {
        vec3 lightSpotDirection = lights[index].direction.xyz;
        float spotEffect = dot(lightDir, lightSpotDirection);
        float spotAtten = (spotEffect - lightAttenuation.y) * lightAttenuation.z;
        if (spotAtten <= 0.0)
            return;

        atten *= spotAtten;

        if (shadowParameters.z < 1.0)
            atten *= clamp(shadowParameters.z + SampleShadowMap(shadowTex9, vec4(worldPos.xyz, 1.0) * lights[index].shadowMatrix, shadowParameters), 0.0, 1.0);
    }
    else if (shadowParameters.z < 1.0)
        atten *= clamp(shadowParameters.z + SampleShadowMap(shadowTex9, GetPointShadowPos(index, lightVec), shadowParameters), 0.0, 1.0);

    accumulatedLight += atten * NdotL * lightColor;
}

vec3 CalculateLighting(vec4 worldPos, vec3 normal, vec2 screenPos)
{
    vec3 accumulatedLight = vec3(0.1, 0.1, 0.1);

    CalculateDirLight(worldPos, normal, accumulatedLight);

    uvec4 lightClusterData = texture(clusterTex12, CalculateClusterPos(screenPos, worldPos.w));

    while (lightClusterData.x > 0U)
    {
        CalculateLight((lightClusterData.x & 0xffU) - 1U, worldPos, normal, accumulatedLight);
        lightClusterData.x >>= 8U;
    }
    while (lightClusterData.y > 0U)
    {
        CalculateLight((lightClusterData.y & 0xffU) - 1U, worldPos, normal, accumulatedLight);
        lightClusterData.y >>= 8U;
    }
    while (lightClusterData.z > 0U)
    {
        CalculateLight((lightClusterData.z & 0xffU) - 1U, worldPos, normal, accumulatedLight);
        lightClusterData.z >>= 8U;
    }
    while (lightClusterData.w > 0U)
    {
        CalculateLight((lightClusterData.w & 0xffU) - 1U, worldPos, normal, accumulatedLight);
        lightClusterData.w >>= 8U;
    }

    return accumulatedLight;
}
