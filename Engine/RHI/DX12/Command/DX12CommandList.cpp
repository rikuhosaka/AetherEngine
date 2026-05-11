#include "DX12CommandList.h"
#include "Engine/RHI/DX12/Command/CommandImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Memory/DX12Buffer.h"
#include "Engine/RHI/DX12/Pipeline/DX12PipelineState.h"
#include "Engine/RHI/DX12/Pipeline/DX12RootSignature.h"
#include "Engine/RHI/DX12/Pipeline/PipelineImpl.h"
#include "Engine/RHI/DX12/Descriptor/DX12DescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12DSVAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12RTVAllocator.h"
#include "Engine/RHI/DX12/Descriptor/AllocatorImpl.h"



CommandListImpl*
DX12CommandList::GetImpl() const
{
	return m_impl.get();
}

DX12CommandList::DX12CommandList(const DX12Device* dxDevice)
	: m_impl(std::make_unique<CommandListImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	if (FAILED(commandAllocator->Reset()))
	{
		LOG_FATAL("Failed to reset command allocator");
		return;
	}
	m_impl->commandAllocator = commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	if (FAILED(commandList->Close()))
	{
		LOG_FATAL("Failed to close command list");
		return;
	}
	m_impl->commandList = commandList;
}


DX12CommandList::~DX12CommandList()
{
	if (m_impl->commandList)
	{
		m_impl->commandList->Release();
		m_impl->commandList = nullptr;
	}
	if (m_impl->commandAllocator)
	{
		m_impl->commandAllocator->Release();
		m_impl->commandAllocator = nullptr;
	}
}



void DX12CommandList::Reset()
{
	if (m_impl->commandAllocator)
	{
		m_impl->commandAllocator->Reset();
	}
	if (m_impl->commandList)
	{
		m_impl->commandList->Reset(m_impl->commandAllocator.Get(), nullptr);
	}
}

void DX12CommandList::Close()
{
	if (m_impl->commandList)
	{
		m_impl->commandList->Close();
	}
}

void
DX12CommandList::SetPipelineState(const RHIPipelineState* pso)
{
	if (m_impl->commandList && pso)
	{
		const DX12PipelineState* dxPso = static_cast<const DX12PipelineState*>(pso);
		ID3D12PipelineState* pipelineState = dxPso->GetImpl()->pipelineState.Get();
		m_impl->commandList->SetPipelineState(pipelineState);
	}
}

void
DX12CommandList::SetRootSignature(const RHIRootSignature* rootSig)
{
	if (m_impl->commandList && rootSig)
	{
		const DX12RootSignature* dxRootSig = static_cast<const DX12RootSignature*>(rootSig);
		ID3D12RootSignature* rootSignature = dxRootSig->GetImpl()->rootSignature.Get();
		m_impl->commandList->SetGraphicsRootSignature(rootSignature);
	}
}


void
DX12CommandList::SetDescriptorHeaps(std::span<const RHIDescriptorAllocator*> heaps)
{
	std::vector<ID3D12DescriptorHeap*> dxHeaps;
	for (const auto& heap : heaps)
	{
		const DX12DescriptorAllocator* dxHeap = static_cast<const DX12DescriptorAllocator*>(heap);
		ID3D12DescriptorHeap* descriptorHeap = dxHeap->GetImpl()->heap.Get();
		dxHeaps.push_back(descriptorHeap);
	}
	if (m_impl->commandList)
	{
		m_impl->commandList->SetDescriptorHeaps(static_cast<UINT>(dxHeaps.size()), dxHeaps.data());
	}
}

void
DX12CommandList::SetGraphicsRootDescriptorTable(uint32_t index, uint64_t baseDescriptor)
{
	if (m_impl->commandList)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
		handle.ptr = baseDescriptor;
		m_impl->commandList->SetGraphicsRootDescriptorTable(index, handle);
	}
}


void
DX12CommandList::IASetVertexBuffers(uint32_t startSlot, std::span<const RHIVertexBuffer*> views)
{
	std::array<D3D12_VERTEX_BUFFER_VIEW, 8> dxViews;

	for (size_t i = 0; i < views.size(); ++i)
	{
		dxViews[i].BufferLocation = views[i]->GetGPUAddress();
		dxViews[i].SizeInBytes = static_cast<UINT>(views[i]->GetSize());
		dxViews[i].StrideInBytes = views[i]->GetStride();
	}

	m_impl->commandList->IASetVertexBuffers(
		startSlot,
		static_cast<UINT>(views.size()),
		dxViews.data());
}

void
DX12CommandList::IASetIndexBuffer(const RHIIndexBuffer* view)
{
	if (view)
	{
		D3D12_INDEX_BUFFER_VIEW dxView = {};
		dxView.BufferLocation = view->GetGPUAddress();
		dxView.SizeInBytes = static_cast<UINT>(view->GetSize());
		IndexFormat indexFormat = view->GetIndexFormat();
		switch (indexFormat)
		{
		case IndexFormat::R16_UINT:
			dxView.Format = DXGI_FORMAT_R16_UINT;
			break;
		case IndexFormat::R32_UINT:
			dxView.Format = DXGI_FORMAT_R32_UINT;
			break;
		default:
			LOG_ERROR("Unsupported index format");
			dxView.Format = DXGI_FORMAT_UNKNOWN; // デフォルト値を設定
			return;
		}
		m_impl->commandList->IASetIndexBuffer(&dxView);
	}
}

void
DX12CommandList::IASetPrimitiveTopology(uint32_t topology)
{
	if (m_impl->commandList)
	{
		m_impl->commandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(topology));
	}
}

void
DX12CommandList::RSSetViewports(float x, float y, float w, float h)
{
	if (m_impl->commandList)
	{
		D3D12_VIEWPORT viewport = {};
		viewport.TopLeftX = x;
		viewport.TopLeftY = y;
		viewport.Width = w;
		viewport.Height = h;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		m_impl->commandList->RSSetViewports(1, &viewport);
	}
}

void
DX12CommandList::RSSetScissorRects(int l, int t, int r, int b)
{
	if (m_impl->commandList)
	{
		D3D12_RECT scissorRect = {};
		scissorRect.left = l;
		scissorRect.top = t;
		scissorRect.right = r;
		scissorRect.bottom = b;
		m_impl->commandList->RSSetScissorRects(1, &scissorRect);
	}
}

void
DX12CommandList::OMSetRenderTargets(uint32_t numRTs, const RtvHandle rtvs, bool singleHandle, const DsvHandle dsv)
{
	if (m_impl->commandList)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8] = {};
		for (uint32_t i = 0; i < numRTs; ++i)
		{
			rtvHandles[i] = { rtvs.cpu.ptr + i * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) };
		}
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = { dsv.cpu.ptr };
		m_impl->commandList->OMSetRenderTargets(numRTs, rtvHandles, singleHandle, &dsvHandle);
	}
}


void
DX12CommandList::ClearRenderTargetView(const RtvHandle rtv, const float color[4])
{
	if (m_impl->commandList)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = { rtv.cpu.ptr };
		m_impl->commandList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
	}
}

void
DX12CommandList::ClearDepthStencilView(const DsvHandle dsv, float depth, uint8_t stencil)
{
	if (m_impl->commandList)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = { dsv.cpu.ptr };
		m_impl->commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
	}
}

void
DX12CommandList::DrawIndexedInstanced(
	uint32_t indexCount,
	uint32_t instanceCount,
	uint32_t startIndex,
	int32_t baseVertex,
	uint32_t startInstance)
{
	if (m_impl->commandList)
	{
		m_impl->commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
	}
}

// Barrier
void
DX12CommandList::ResourceBarrier(RHIResource* resource, ERHIResourceState newState)
{
	if (!resource)
	{
		LOG_ERROR("Resource is null");
		return;
	}
	resource->TransitionResource(newState, this);
}