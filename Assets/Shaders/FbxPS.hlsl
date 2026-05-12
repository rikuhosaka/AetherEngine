#include "BasicType.hlsli"

Texture2D<float4> g_Textures[] : register(SRV_TEXTURES);

SamplerState smp : register(SAMPLER_LINEAR); //0番スロットに設定されたサンプラ
SamplerState smpToon : register(SAMPLER_ANISO); //1番スロットに設定されたサンプラ

//定数バッファ1
//マテリアル用
cbuffer Material : register(CBV_MATERIAL)
{
    float4 diffuse; //ディフューズ色
    float4 specular; //スペキュラ
    float3 ambient; //アンビエント
};

//ピクセルシェーダ
float4 FbxPS(FbxOutput input) : SV_TARGET
{
   
    // ==========================
    // テクスチャのサンプリング
    // ==========================
    float4 texColor = g_Textures[0].Sample(smp, input.uv);
    float4 specColor = g_Textures[1].Sample(smp, input.uv);
    float3 normalTex = g_Textures[2].Sample(smp, input.uv).rgb;

    // ==========================
    // 法線マップの変換（TBN）
    // ==========================
    normalTex = normalTex * 2.0f - 1.0f; // [0,1] → [-1,1]
    float3x3 TBN = float3x3(normalize(input.worldTangent),
                            normalize(input.worldBinormal),
                            normalize(input.worldNormal));
    float3 bumpedNormal = normalize(mul(TBN, normalTex));

    // ==========================
    // 簡易的なライティング
    // ==========================
    float3 lightDir = normalize(float3(0, 0.577, -0.577)); // 上前方向の平行光源
    float3 viewDir = normalize(input.ray);

    float3 diffuseLight = saturate(dot(bumpedNormal, lightDir));
    float3 reflectDir = reflect(-lightDir, bumpedNormal);
    float specPower = 32.0f; // スペキュラの鋭さ
    float specIntensity = pow(saturate(dot(viewDir, reflectDir)), specPower);

    // ==========================
    // ライティング結果を合成
    // ==========================
    float3 finalColor =
        ambient * texColor.rgb +
        texColor.rgb * diffuse.rgb * diffuseLight +
        specColor.rgb * specular.rgb * specIntensity;

    return float4(finalColor, texColor.a);


}