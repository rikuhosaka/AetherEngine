#include "Include/FrameConstants.hlsli"
#include "Include/ObjectConstants.hlsli"

struct VSOut
{
    float4 pos : SV_Position;
    float clip : SV_ClipDistance0;
};

VSOut ShadowDepthVS(float4 pos : POSITION, float2 uv : TEXCOORD0, float3 normal : NORMAL)
{
    VSOut output;
    float4 worldPos = mul(pos, worldMatrix);
    output.pos = mul(worldPos, lightViewProjection);
    // Keep the Basic input signature, and stay on the visible side of the clip plane.
    output.clip = 1.0 + abs(uv.x) + abs(normal.x);
    return output;
}
