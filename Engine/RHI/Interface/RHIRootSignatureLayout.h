#pragma once

#include "Engine/RHI/Common/RHIRootSignatureLayout.h"
#include "Engine/RHI/Common/RHIPipeline.h"

class RHIVertexShader;
class RHIPixelShader;

struct RHIPipelineStateLayout
{
    // --- シェーダ（実装では blob ハッシュでも可）---
    const RHIVertexShader* vertexShader = nullptr;
    const RHIPixelShader*  pixelShader  = nullptr;

    // --- 入力・アセンブラ ---
    InputLayoutType   inputLayout = InputLayoutType::Basic;
    PrimitiveTopology topology    = PrimitiveTopology::TriangleList;

    // --- 固定機能 ---
    RasterizerState    raster = RasterizerState::NoCull;
    BlendState         blend  = BlendState::Opaque;
    DepthStencilState  depth  = DepthStencilState::DepthDefault;

    // --- 出力 ---
    RTV_FORMAT rtvFormats[8]{};
    uint8_t    numRT = 1;
    DSV_FORMAT dsvFormat = DSV_FORMAT::D32_FLOAT;
    uint8_t    sampleCount = 1;

    // --- Root（Layout 参照で PSO と RS の整合をコンパイル時に縛る）---
    RHIRootSignatureLayout rootSignature;

    uint64_t Hash() const;
    bool operator==(const RHIPipelineStateLayout&) const = default;
};

// 既存 RHIPipelineDesc からの移行用
inline RHIPipelineStateLayout FromLegacyDesc(const RHIPipelineDesc& desc,
                                             RHIRootSignatureLayout rootLayout)
{
    RHIPipelineStateLayout layout{};
    layout.vertexShader   = desc.vertexShader;
    layout.pixelShader    = desc.pixelShader;
    layout.inputLayout    = desc.inputLayout;
    layout.topology       = desc.topology;
    layout.raster         = desc.raster;
    layout.blend          = desc.blend;
    layout.depth          = desc.depth;
    layout.numRT          = desc.numRT;
    layout.sampleCount    = desc.sampleCount;
    layout.dsvFormat      = desc.dsvFormat;
    for (uint8_t i = 0; i < desc.numRT; ++i)
        layout.rtvFormats[i] = desc.rtvFormats[i];
    layout.rootSignature  = std::move(rootLayout);
    return layout;
}