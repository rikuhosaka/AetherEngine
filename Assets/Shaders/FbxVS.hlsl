#include "BasicType.hlsli"

cbuffer cbuff0 : register(CBV_VIEWPROJ)
{
    matrix view; //変換行列
    matrix proj;
    float3 eye;
};

cbuffer Entity : register(CBV_ENTITY)
{
    uint entity; //ワールド変換行列
};

cbuffer AnimIndex : register(CBV_ANIMINDEX)
{
    uint animIndex; //アニメーション用インデックス
};

StructuredBuffer<matrix> world : register(SRV_WORLD_MAT); //ワールド変換行列
StructuredBuffer<float4x4> boneMatrices : register(SRV_BONES); //ボーン行列

//頂点シェーダ
FbxOutput FbxVS(FbxInput input)
{
    FbxOutput output;
    
    
    //===== スキニング処理 =====
    float4 skinnedPos = float4(0, 0, 0, 0); // スキニング後の位置
    float3 skinnedNormal = float3(0, 0, 0); // スキニング後の法線
    float3 skinnedTangent = float3(0, 0, 0); // スキニング後の接線
    for (int i = 0; i < 4; i++)
    {
        // ボーンインデックスとウェイトを取得
        uint boneIndex = animIndex + input.boneIndices[i];
        float weight = input.boneWeights[i];
        // ボーン行列を取得
        float4x4 boneMatrix = boneMatrices[boneIndex];
        // スキニングを適用
        skinnedPos += mul(boneMatrix, input.position) * weight;
        skinnedNormal += mul((float3x3) boneMatrix, input.normal) * weight;
        skinnedTangent += mul((float3x3) boneMatrix, input.tangent) * weight;
    }

    
    
    
    //===== 変換処理 =====
    
    /*
    float4 pos = mul(world[entity], input.position);
    float3 normal = input.normal;
    float3 tangent = input.tangent; // 接線はそのまま使用（スキニング後の接線を使用する場合は上記で計算済み）
    */
    
    
    float4 pos = mul(world[entity], skinnedPos); // ワールド変換を適用   
    float3 normal = normalize(skinnedNormal); // スキニング後の法線を使用
    float3 tangent = normalize(skinnedTangent); // スキニング後の接線を使用
    
    
    // ライティングをワールド空間基準にするため view での変換を削除
    output.pos = pos;
    output.svpos = mul(mul(proj, view), pos); // システム用頂点座標（クリップ空間）を計算
  
    output.normal = normalize(mul((float3x3) world[entity], normal)); // ワールド空間の法線
    output.vnormal = normalize(mul(mul(proj, view), output.normal)); // スフィアマッピングもワールド空間基準に変更
    
    float3 worldNormal = output.normal;
    float3 worldTangent = normalize(mul((float3x3) world[entity], tangent));
    float3 worldBinormal = normalize(cross(worldNormal, worldTangent)); // ビットンジェントを自作

    output.worldNormal = worldNormal;
    output.worldTangent = worldTangent;
    output.worldBinormal = worldBinormal;
    
    float2 uv = input.uv;
    
    output.uv = uv;

    // 視線ベクトルをカメラ固定（任意に固定するか、無効化）
    output.ray = normalize(float3(eye - pos.xyz)); // 固定視線ベクトル（例として-Z方向）
    
    return output;
}

