#include "BasicType.hlsli"

cbuffer Material : register(CBV_MATERIAL)
{
    float4 diffuse; //ディフューズ色
    float4 specular; //スペキュラ
    float3 ambient; //アンビエント
};

float4 BasicPS(BasicType input) : SV_Target
{
    float3 lightDir = normalize(float3(0, 0.577, -0.577)); // 上前方向の平行光源
    float3 lightColor = float3(0.8f, 0.8f, 0.8f); // 白色光
    // 法線を正規化
    float3 N = normalize(input.normal);
    

    // 拡散反射係数
    float NdotL = max(dot(N, lightDir), 0.0f);

    // 拡散反射成分
    float3 diffuseColor = diffuse.rgb * lightColor * NdotL;

    // アンビエント成分
    float3 ambientColor = ambient;

    // 合計
    float3 finalColor = ambientColor + diffuseColor;

    // アルファ値はdiffuse.a
    return float4(finalColor, diffuse.a);
}