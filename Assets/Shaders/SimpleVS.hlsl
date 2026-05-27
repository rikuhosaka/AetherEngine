struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer SceneConstants : register(b0)
{
    float4x4 mvp;
};

VSOut SimpleVS(float3 pos : POSITION, float2 uv : TEXCOORD0)
{
    VSOut output;
    output.pos = mul(mvp, float4(pos, 1.0f));
    output.uv = uv;
    return output;
}
