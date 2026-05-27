#include "ResourceImpl.h"
#include "Engine/RHI/DX12/Command/DX12CommandList.h"
#include "Engine/RHI/DX12/Command/CommandImpl.h"


D3D12_RESOURCE_STATES ConvertToD3D12ResourceState(ERHIResourceState state)
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
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

void ResourceImpl::TransitionResource(ERHIResourceState newState, const RHICommandList* rhiCommandList)
{
	const DX12CommandList* dxCommandList = static_cast<const DX12CommandList*>(rhiCommandList);
	ID3D12GraphicsCommandList* commandList = dxCommandList->GetImpl()->commandList.Get();

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = static_cast<ResourceImpl*>(this)->resource.Get();
	barrier.Transition.StateBefore = ConvertToD3D12ResourceState(m_currentState);
	barrier.Transition.StateAfter = ConvertToD3D12ResourceState(newState);

	commandList->ResourceBarrier(1, &barrier);
	m_currentState = newState;
}