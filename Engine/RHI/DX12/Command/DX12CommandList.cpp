#include "DX12CommandList.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Command/CommandImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Memory/DX12Buffer.h"
#include "Engine/RHI/DX12/Pipeline/DX12PipelineState.h"
#include "Engine/RHI/DX12/Pipeline/DX12RootSignature.h"
#include "Engine/RHI/DX12/Pipeline/PipelineImpl.h"
#include "Engine/RHI/DX12/Descriptor/DX12DescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12TransientDescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12DSVAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12RTVAllocator.h"
#include "Engine/RHI/DX12/Descriptor/AllocatorImpl.h"
#include "Engine/RHI/DX12/Memory/DX12UploadBuffer.h"
#include "Engine/RHI/DX12/Resource/DX12Texture.h"
#include "Engine/RHI/DX12/Common/DX12Format.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"
#include "Engine/RHI/Interface/RHIBuffer.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"

#include <atomic>
#include <format>



CommandListImpl*
DX12CommandList::GetImpl() const
{
	return m_impl.get();
}

bool DX12CommandList::IsValid() const
{
	return m_impl != nullptr && m_impl->commandAllocator != nullptr && m_impl->commandList != nullptr;
}

Result<std::unique_ptr<DX12CommandList>> DX12CommandList::Create(const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12CommandList>(new DX12CommandList(dxDevice)),
		"Failed to create command list");
}

DX12CommandList::DX12CommandList(const DX12Device* dxDevice)
	: m_impl(std::make_unique<CommandListImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	const HRESULT allocatorHr =
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	if (FAILED(allocatorHr))
	{
		return;
	}
	if (FAILED(commandAllocator->Reset()))
	{
		return;
	}
	m_impl->commandAllocator = commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	const HRESULT listHr = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList));
	if (FAILED(listHr))
	{
		m_impl->commandAllocator.Reset();
		return;
	}
	if (FAILED(commandList->Close()))
	{
		m_impl->commandAllocator.Reset();
		return;
	}
	m_impl->commandList = commandList;

	static std::atomic<uint32_t> s_instanceCounter = 0;
	const uint32_t instanceId = s_instanceCounter.fetch_add(1, std::memory_order_relaxed);
	const std::string allocatorName = std::format("CmdAllocator/Direct/{}", instanceId);
	const std::string commandListName = std::format("CmdList/Direct/{}", instanceId);
	DX12GpuNaming::SetName(commandAllocator.Get(), allocatorName);
	DX12GpuNaming::SetName(commandList.Get(), commandListName);
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
	dxHeaps.reserve(heaps.size());
	for (const RHIDescriptorAllocator* heap : heaps)
	{
		if (heap == nullptr)
		{
			continue;
		}
		const auto* dxHeap = static_cast<const DX12DescriptorAllocator*>(heap);
		dxHeaps.push_back(dxHeap->GetImpl()->heap.Get());
	}
	if (m_impl->commandList && !dxHeaps.empty())
	{
		m_impl->commandList->SetDescriptorHeaps(static_cast<UINT>(dxHeaps.size()), dxHeaps.data());
	}
}

void DX12CommandList::SetTransientDescriptorHeap(RHITransientDescriptorAllocator* heap)
{
	if (heap == nullptr || m_impl->commandList == nullptr)
	{
		return;
	}
	const auto* dxHeap = static_cast<DX12TransientDescriptorAllocator*>(heap);
	ID3D12DescriptorHeap* heaps[] = { dxHeap->GetImpl()->heap.Get() };
	m_impl->commandList->SetDescriptorHeaps(1, heaps);
}

void DX12CommandList::CopyBufferRegion(
	RHIBuffer* dstBuffer,
	size_t dstOffset,
	RHIUploadBuffer* srcUpload,
	size_t srcOffset,
	size_t numBytes)
{
	if (dstBuffer == nullptr || srcUpload == nullptr || m_impl->commandList == nullptr)
	{
		return;
	}

	ResourceImpl* dstResource = nullptr;
	if (auto* vertexBuffer = dynamic_cast<RHIVertexBuffer*>(dstBuffer))
	{
		dstResource = static_cast<DX12VertexBuffer*>(vertexBuffer)->GetBufferResourceImpl();
	}
	else if (auto* indexBuffer = dynamic_cast<RHIIndexBuffer*>(dstBuffer))
	{
		dstResource = static_cast<DX12IndexBuffer*>(indexBuffer)->GetBufferResourceImpl();
	}

	auto* src = static_cast<DX12UploadBuffer*>(srcUpload);
	if (dstResource == nullptr || dstResource->resource == nullptr || src->GetImpl()->resource == nullptr)
	{
		return;
	}

	m_impl->commandList->CopyBufferRegion(
		dstResource->resource.Get(),
		dstOffset,
		src->GetImpl()->resource.Get(),
		srcOffset,
		numBytes);
}

void DX12CommandList::CopyTextureRegion(
	RHITexture* dstTexture,
	uint32_t dstSubresource,
	RHIUploadBuffer* srcUpload,
	size_t srcOffset,
	uint32_t bytesPerRow,
	uint32_t numRows)
{
	(void)numRows;
	if (dstTexture == nullptr || srcUpload == nullptr || m_impl->commandList == nullptr)
	{
		return;
	}

	auto* dst = static_cast<DX12Texture*>(dstTexture);
	auto* src = static_cast<DX12UploadBuffer*>(srcUpload);
	if (dst->GetResourceImpl()->resource == nullptr || src->GetImpl()->resource == nullptr)
	{
		return;
	}

	D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
	dstLocation.pResource = dst->GetResourceImpl()->resource.Get();
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstLocation.SubresourceIndex = dstSubresource;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	footprint.Footprint.Format = ToDxgiFormat(dst->GetFormat());
	footprint.Footprint.Width = dst->GetWidth();
	footprint.Footprint.Height = dst->GetHeight();
	footprint.Footprint.Depth = 1;
	footprint.Footprint.RowPitch = bytesPerRow;

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.pResource = src->GetImpl()->resource.Get();
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLocation.PlacedFootprint.Offset = srcOffset;
	srcLocation.PlacedFootprint.Footprint = footprint.Footprint;

	m_impl->commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
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
			LOG_ERROR(LogCategory::RHI, "Unsupported index format");
			dxView.Format = DXGI_FORMAT_UNKNOWN; // ?f?t?H???g?l????
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
		LOG_FATAL(LogCategory::RHI, "ResourceBarrier called with null resource");
		return;
	}
	resource->TransitionResource(newState, this);
}