#pragma once
#include "Engine/RHI/Common/RHIShaderBinding.h"
#include <cstdint>
#include <vector>
#include <array>

enum class RHIRootParamType : uint8_t
{
    Constants,
    CBV_Table,
    SRV_Table,
    UAV_Table,
    StaticSampler,
};

// HLSL register と 1:1 で書ける単位
struct RHIRootDescriptorRange
{
    RHIRootParamType type = RHIRootParamType::SRV_Table;
    uint32_t         count = 1;
    uint32_t         baseRegister = 0;  // b#/t#/u#
    uint32_t         space = 0;
};

struct RHIRootConstants
{
    uint32_t num32BitValues = 0;
    uint32_t baseRegister = 0;
    uint32_t space = 0;
};

struct RHIRootStaticSampler
{
    // サンプラーのバインディング
    RHISamplerBinding samplerBinding;
};

struct RHIRootParameterDesc
{
    RHIRootParamType kind = RHIRootParamType::Constants;

    RHIRootConstants constants{};
    RHIRootDescriptorRange range{};

    // シェーダーのバインディング
    RHIShaderBinding shaderBinding;
};

enum class RHIRootSignatureFlags : uint32_t
{
    None = 0,
    AllowInputAssembler = 1 << 0,
};
inline RHIRootSignatureFlags operator|(RHIRootSignatureFlags a, RHIRootSignatureFlags b)
{
    return static_cast<RHIRootSignatureFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct RHIRootSignatureLayout
{
    std::vector<RHIRootParameterDesc> parameters;
    std::vector<RHIRootStaticSampler> staticSamplers;
    RHIRootSignatureFlags flags = RHIRootSignatureFlags::AllowInputAssembler;

    // キャッシュ / unordered_map 用
    uint64_t Hash() const;
    bool operator==(const RHIRootSignatureLayout&) const = default;
};

// ビルダー（既存のハードコード RS を Layout 化する例）
inline RHIRootSignatureLayout MakeDefaultForwardRootSignatureLayout()
{

    RHIRootSignatureLayout layout{};
    layout.parameters = {
        { RHIRootParamType::Constants,    { .num32BitValues = 3, .baseRegister = 0 }, {}, RHIShaderBinding::DrawInfo },
        { RHIRootParamType::CBV_Table,    {}, { RHIRootParamType::CBV_Table, 1, 1 }, RHIShaderBinding::CbvViewProj },
        { RHIRootParamType::SRV_Table,    {}, { RHIRootParamType::SRV_Table, 1, 0 }, RHIShaderBinding::SrvWorldMat },
        { RHIRootParamType::SRV_Table,    {}, { RHIRootParamType::SRV_Table, 1, 1 }, RHIShaderBinding::SrvBones },
        { RHIRootParamType::SRV_Table,    {}, { RHIRootParamType::SRV_Table, 1, 2 }, RHIShaderBinding::SrvMaterial },
        { RHIRootParamType::SRV_Table,    {}, { RHIRootParamType::SRV_Table, 8, 3 }, RHIShaderBinding::SrvTextures },
    };
    layout.staticSamplers = {
        { RHISamplerBinding::Linear },
        { RHISamplerBinding::Aniso },
        { RHISamplerBinding::Wrap },
    };
    return layout;
}