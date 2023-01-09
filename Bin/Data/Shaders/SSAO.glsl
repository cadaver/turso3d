#ifdef COMPILEVS

in vec3 position;
out vec2 vUv;

#else

uniform vec2 noiseInvSize;
uniform vec2 screenInvSize;
uniform vec4 frustumSize;
uniform vec4 aoParameters;
uniform vec2 depthReconstruct;

uniform sampler2D depthTex0;
uniform sampler2D normalTex1;
uniform sampler2D noiseTex2;

in vec2 vUv;
out vec4 fragColor;

float GetLinearDepth(float hwDepth)
{
    return depthReconstruct.y / (hwDepth - depthReconstruct.x);
}

vec3 GetPosition(float depth, vec2 uv)
{
    vec3 pos = vec3((uv - 0.5) * frustumSize.xy, frustumSize.z) * depth;
    return pos;
}

vec3 GetPosition(vec2 uv)
{
    float depth = GetLinearDepth(texture(depthTex0, uv).r);
    return GetPosition(depth, uv);
}

float DoAmbientOcclusion(vec2 uv, vec2 offs, vec3 pos, vec3 n)
{
    offs.x *= frustumSize.w;
    vec3 diff = GetPosition(uv + offs) - pos;
    vec3 v = normalize(diff);
    return dot(v, n) * clamp(1.0 - abs(diff.z) * aoParameters.y, 0.0, 1.0);
}

#endif

void vert()
{
    gl_Position = vec4(position, 1.0);
    vUv = vec2(position.xy) * 0.5 + 0.5;
}

void frag()
{
    vec2 rand = texture(noiseTex2, vUv * noiseInvSize).rg * 2.0 - 1.0;
    float depth = GetLinearDepth(texture(depthTex0, vUv).r);
    vec3 pos = GetPosition(depth, vUv);

    float rad = min(aoParameters.x / pos.z, aoParameters.w);
    float ao = 0.0;

    if (rad > 3.5 * screenInvSize.x)
    {
        vec3 normal = texture(normalTex1, vUv).rgb * 2.0 - 1.0;

        vec2 vec[4] = vec2[](
            vec2(1,0),
            vec2(-1,0),
            vec2(0,1),
            vec2(0,-1)
        );

        int iterations = 4;

        for (int i = 0; i < iterations; ++i)
        {
            vec2 coord1 = reflect(vec[i], rand) * rad;
            vec2 coord2 = vec2(coord1.x * 0.707 - coord1.y * 0.707, coord1.x * 0.707 + coord1.y * 0.707);

            ao += DoAmbientOcclusion(vUv, coord1*0.5, pos, normal);
            ao += DoAmbientOcclusion(vUv, coord2, pos, normal);
        }
    }

    fragColor = vec4(clamp(ao * aoParameters.z, 0.0, 1.0), 1.0, 1.0, 1.0);
}
