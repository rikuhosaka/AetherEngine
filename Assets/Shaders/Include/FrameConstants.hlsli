#ifndef FRAME_CONSTANTS_HLSLI
#define FRAME_CONSTANTS_HLSLI

cbuffer FrameConstants : register(b0)
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjectionMatrix;
    float4 cameraPosition;
    float4 ambientColor;
    float4 mainLightDirection;
    float4 mainLightColor;
    float4x4 lightViewProjection;
    float4 shadowParams;
    float4 shadowPadding[11];
};

#endif
