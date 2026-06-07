#include "Include/FrameConstants.hlsli"

struct PSIn
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

cbuffer MaterialConstants : register(b2)
{
    float4 tint;
};

float4 LitBasicPS(PSIn input) : SV_Target
{
    float3 albedo = g_texture.Sample(g_sampler, input.uv).rgb * tint.rgb;

    float3 normal = normalize(input.normalW);
    float3 lightDirection = normalize(-mainLightDirection.xyz);
    float ndotl = saturate(dot(normal, lightDirection));

    float3 ambient = ambientColor.rgb * ambientColor.a;
    float3 diffuse = mainLightColor.rgb * mainLightColor.a * ndotl;
    float3 litColor = albedo * (ambient + diffuse);

    return float4(litColor, 1.0f);
}
