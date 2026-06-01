#include "Engine/RHI/DX12/Common/DX12Flag.h"

D3D12_RESOURCE_FLAGS ToResourceFlags(ERHITextureUsage usage)
{
    switch (usage)
    {
    case ERHITextureUsage::RenderTarget:
        return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    case ERHITextureUsage::DepthStencil:
        return D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    case ERHITextureUsage::ShaderResource:
        return D3D12_RESOURCE_FLAG_NONE;
    case ERHITextureUsage::UnorderedAccess:
        return D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    default:
        return D3D12_RESOURCE_FLAG_NONE;
    }
}