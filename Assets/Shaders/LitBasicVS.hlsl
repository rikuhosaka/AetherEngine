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
    float4 worldPos4 = mul(worldMatrix, pos);
    output.worldPos = worldPos4.xyz;
    output.normalW = mul((float3x3)worldInverseTranspose, normal);
    output.pos = mul(viewProjectionMatrix, worldPos4);
    output.uv = uv;
    return output;
}
