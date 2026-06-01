#include "Engine/RHI/DX12/Common/DX12Resource.h"

D3D12_RESOURCE_STATES ToResourceState(ERHIResourceState state)
{
    switch (state)
    {
    case ERHIResourceState::Common:
        return D3D12_RESOURCE_STATE_COMMON;
    case ERHIResourceState::CopyDest:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case ERHIResourceState::CopySource:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case ERHIResourceState::VertexAndConstantBuffer:
        return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    case ERHIResourceState::IndexBuffer:
        return D3D12_RESOURCE_STATE_INDEX_BUFFER;
    case ERHIResourceState::RenderTarget:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case ERHIResourceState::UnorderedAccess:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case ERHIResourceState::DepthWrite:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case ERHIResourceState::DepthRead:
        return D3D12_RESOURCE_STATE_DEPTH_READ;
    case ERHIResourceState::NonPixelShaderResource:
        return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case ERHIResourceState::PixelShaderResource:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case ERHIResourceState::Present:
        return D3D12_RESOURCE_STATE_PRESENT;
    default:
        return D3D12_RESOURCE_STATE_COMMON;
    }
}