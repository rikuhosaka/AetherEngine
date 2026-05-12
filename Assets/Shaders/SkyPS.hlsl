#include "BasicType.hlsli"


Texture2D<float4> skyboxTexture : register(SRV_TEXTURES); // スカイボックス用のキューブマップテクスチャ
SamplerState samplerState : register(SAMPLER_WRAP); // サンプラー

float4 SkyPS(BasicType input) : SV_Target
{

    // テクスチャの色を取得
    float4 texColor = skyboxTexture.Sample(samplerState, input.uv);
    
    return texColor;
}