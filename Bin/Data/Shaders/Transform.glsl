#if defined(INSTANCED)
in vec4 texCoord3;
in vec4 texCoord4;
in vec4 texCoord5;

mat3x4 GetWorldMatrix()
{
    return mat3x4(texCoord3, texCoord4, texCoord5);
}
#elif defined(SKINNED)
in vec4 blendWeights;
in vec4 blendIndices;

mat3x4 GetWorldMatrix()
{
    ivec4 indices = ivec4(blendIndices);
    return skinMatrices[indices.x] * blendWeights.x + skinMatrices[indices.y] * blendWeights.y +
           skinMatrices[indices.z] * blendWeights.z + skinMatrices[indices.w] * blendWeights.w;
}
#else
uniform mat3x4 worldMatrix;

mat3x4 GetWorldMatrix()
{
    return worldMatrix;
}
#endif

float CalculateDepth(vec4 outPos)
{
    return dot(depthParameters.zw, outPos.zw);
}

vec2 CalculateScreenPos(vec4 outPos)
{
    return vec2(
        outPos.x / outPos.w * 0.5 + 0.5,
        -outPos.y / outPos.w * 0.5 + 0.5
    );
}
