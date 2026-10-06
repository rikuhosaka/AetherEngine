#include "Include/FrameConstants.hlsli"
#include "Include/ObjectConstants.hlsli"

struct VSOut
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

VSOut LitBasicVS(float4 pos : POSITION, float2 uv : TEXCOORD0, float3 normal : NORMAL)
{
    VSOut output;
    float4 worldPos4 = mul(pos, worldMatrix);
    output.worldPos = worldPos4.xyz;
    output.normalW = mul(normal, (float3x3)worldInverseTranspose);
    output.pos = mul(worldPos4, viewProjectionMatrix);
    output.uv = uv;
    return output;
}
