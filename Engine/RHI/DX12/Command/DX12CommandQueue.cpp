#include "DX12CommandQueue.h"
#include "Engine/RHI/DX12/Command/CommandImpl.h"
#include "Engine/RHI/DX12/Command/DX12CommandList.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Sync/DX12Fence.h"
#include "Engine/RHI/DX12/Sync/FenceImpl.h"


DX12CommandQueue::DX12CommandQueue(const DX12Device* dxDevice)
	: m_impl(std::make_unique<CommandQueueImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;
	ComPtr<ID3D12CommandQueue> commandQueue;
	auto result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
	if (FAILED(result)) {
		LOG_FATAL("Failed to create command queue");
		return;
	}
	m_impl->commandQueue = commandQueue;
}

DX12CommandQueue::~DX12CommandQueue()
{
	if (m_impl->commandQueue)
	{
		m_impl->commandQueue->Release();
		m_impl->commandQueue = nullptr;
	}
}

void DX12CommandQueue::ExecuteCommandLists(const std::vector<RHICommandList*>& commandLists)
{
	std::vector<ID3D12CommandList*> dxCommandLists;
	for (const auto& cmdList : commandLists)
	{
		const DX12CommandList* dxCmdList = static_cast<DX12CommandList*>(cmdList);
		dxCommandLists.push_back(dxCmdList->GetImpl()->commandList.Get());
	}
	m_impl->commandQueue->ExecuteCommandLists(static_cast<UINT>(dxCommandLists.size()), dxCommandLists.data());
}

uint64_t 
DX12CommandQueue::Signal(RHIFence* fence)
{
	DX12Fence* dxFence = static_cast<DX12Fence*>(fence);
	dxFence->Increment();
	return m_impl->commandQueue->Signal(dxFence->GetImpl()->fence.Get(), dxFence->GetImpl()->currentFenceValue);
}

void 
DX12CommandQueue::WaitGPU(RHIFence* fence, uint64_t value)
{
	DX12Fence* dxFence = static_cast<DX12Fence*>(fence);
	if (!dxFence->IsComplete(value))
	{
		m_impl->commandQueue->Wait(dxFence->GetImpl()->fence.Get(), value);
	}
}
