#include "Include/FrameConstants.hlsli"

struct PSIn
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

Texture2D g_texture : register(t0);
Texture2D<float> g_shadowMap : register(t1);
SamplerState g_sampler : register(s0);

float SampleShadow(float3 worldPos)
{
    float4 lightClip = mul(float4(worldPos, 1.0), lightViewProjection);
    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        return 1.0;
    }

    float bias = shadowParams.x;
    int mapSize = (int)shadowParams.z;
    int2 basePixel = int2(uv * shadowParams.z);
    float receiverDepth = ndc.z;
    float litSamples = 0.0;
    float sampleCount = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 coord = basePixel + int2(x, y);
            if (coord.x < 0 || coord.y < 0 || coord.x >= mapSize || coord.y >= mapSize)
            {
                continue;
            }

            float closestDepth = g_shadowMap.Load(int3(coord, 0));
            litSamples += receiverDepth <= closestDepth + bias ? 1.0 : 0.0;
            sampleCount += 1.0;
        }
    }

    return sampleCount > 0.0 ? litSamples / sampleCount : 1.0;
}

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
    float shadow = SampleShadow(input.worldPos);
    float3 litColor = albedo * (ambient + diffuse * shadow);

    return float4(litColor, 1.0f);
}
