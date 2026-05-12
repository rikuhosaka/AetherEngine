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

StructuredBuffer<matrix> world : register(SRV_WORLD_MAT); //ワールド変換行列

BasicType BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL)
{
    BasicType output;
    
    pos = mul(world[entity], pos);
    output.svpos = mul(mul(proj, view), pos); //シェーダでは列優先なので注意
    output.uv = uv;
    output.normal = normalize(mul((float3x3) world[entity], normal));
    
    return output;
}