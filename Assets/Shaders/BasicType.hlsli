//頂点シェーダ→ピクセルシェーダへのやり取りに使用する

// ====== CBV ======
#define CBV_ENTITY     b0
#define CBV_ANIMINDEX  b1
#define CBV_VIEWPROJ   b2
#define CBV_MATERIAL   b3

// ====== SRV ======
#define SRV_WORLD_MAT  t0
#define SRV_BONES      t1
#define SRV_TEXTURES   t2

// ====== SAMPLER ======
#define SAMPLER_LINEAR s0
#define SAMPLER_ANISO  s1
#define SAMPLER_WRAP   s2

//構造体
struct FbxInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT; //接線ベクトル
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};
struct FbxOutput
{
    float4 svpos : SV_POSITION; //システム用頂点座標
    float4 pos : POSITION; //システム用頂点座標
    float3 normal : NORMAL0; //法線ベクトル
    float3 vnormal : NORMAL1; //法線ベクトル
    float3 worldNormal : TEXCOORD0;
    float3 worldTangent : TEXCOORD1;
    float3 worldBinormal : TEXCOORD2;
    float2 uv : TEXCOORD3; //UV値
    float3 ray : VECTOR; //ベクトル
};

struct BasicType
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL0;
};
