#ifndef OBJECT_CONSTANTS_HLSLI
#define OBJECT_CONSTANTS_HLSLI

cbuffer ObjectConstants : register(b1)
{
    float4x4 worldMatrix;
    float4x4 worldInverseTranspose;
    float4 objectPadding[8];
};

#endif
