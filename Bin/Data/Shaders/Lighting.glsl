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

void CalculateDirLight(vec4 worldPos, vec3 normal, vec4 matDiffColor, vec4 matSpecColor, inout vec3 diffuseLight, inout vec3 specularLight)
{
    vec3 lightDir = dirLightDirection;
    vec4 lightColor = dirLightColor;

    float NdotL = dot(normal, lightDir);
    if (NdotL <= 0.0)
        return;

    vec4 shadowSplits = dirLightShadowSplits;
    vec4 shadowParameters = dirLightShadowParameters;

    if (shadowParameters.z < 1.0 && worldPos.w < shadowSplits.y)
    {
        int matIndex = worldPos.w > shadowSplits.x ? 1 : 0;

        mat4 shadowMatrix = dirLightShadowMatrices[matIndex];
        float shadowFade = shadowParameters.z + clamp((worldPos.w - shadowSplits.z) * shadowSplits.w, 0.0, 1.0);
        NdotL *= clamp(shadowFade + SampleShadowMap(dirShadowTex8, vec4(worldPos.xyz, 1.0) * shadowMatrix, shadowParameters), 0.0, 1.0);
    }

    diffuseLight += NdotL * lightColor.rgb * matDiffColor.rgb;

    if (lightColor.a > 0.0)
    {
        vec3 eyeVec = cameraPosition - worldPos.xyz;
        vec3 halfVec = normalize(normalize(eyeVec) + lightDir);
        float NdotH = dot(normal, halfVec);
        specularLight += NdotL * lightColor.a * matSpecColor.rgb * clamp(pow(NdotH, matSpecColor.a), 0.0, 1.0) * lightColor.rgb;
    }
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

void CalculateLight(uint index, vec4 worldPos, vec3 normal, vec4 matDiffColor, vec4 matSpecColor, inout vec3 diffuseLight, inout vec3 specularLight)
{
    vec3 lightPosition = lights[index].position.xyz;
    vec4 lightAttenuation = lights[index].attenuation;
    vec4 lightColor = lights[index].color;

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

    diffuseLight += atten * NdotL * lightColor.rgb * matDiffColor.rgb;

    if (atten > 0.0 && lightColor.a > 0.0)
    {
        vec3 eyeVec = cameraPosition - worldPos.xyz;
        vec3 halfVec = normalize(normalize(eyeVec) + lightDir);
        float NdotH = dot(normal, halfVec);
        specularLight += atten * NdotL * lightColor.a * matSpecColor.rgb * clamp(pow(NdotH, matSpecColor.a), 0.0, 1.0) * lightColor.rgb;
    }
}

void CalculateLighting(vec4 worldPos, vec3 normal, vec2 screenPos, vec4 matDiffColor, vec4 matSpecColor, out vec3 diffuseLight, out vec3 specularLight)
{
    diffuseLight = vec3(matDiffColor.rgb * ambientColor.rgb);
    specularLight = vec3(0.0, 0.0, 0.0);

    CalculateDirLight(worldPos, normal, matDiffColor, matSpecColor, diffuseLight, specularLight);

    uvec4 lightClusterData = texture(clusterTex12, CalculateClusterPos(screenPos, worldPos.w));

    while (lightClusterData.x > 0U)
    {
        CalculateLight((lightClusterData.x & 0xffU) - 1U, worldPos, normal, matDiffColor, matSpecColor, diffuseLight, specularLight);
        lightClusterData.x >>= 8U;
    }
    while (lightClusterData.y > 0U)
    {
        CalculateLight((lightClusterData.y & 0xffU) - 1U, worldPos, normal, matDiffColor, matSpecColor, diffuseLight, specularLight);
        lightClusterData.y >>= 8U;
    }
    while (lightClusterData.z > 0U)
    {
        CalculateLight((lightClusterData.z & 0xffU) - 1U, worldPos, normal, matDiffColor, matSpecColor, diffuseLight, specularLight);
        lightClusterData.z >>= 8U;
    }
    while (lightClusterData.w > 0U)
    {
        CalculateLight((lightClusterData.w & 0xffU) - 1U, worldPos, normal, matDiffColor, matSpecColor, diffuseLight, specularLight);
        lightClusterData.w >>= 8U;
    }
}
