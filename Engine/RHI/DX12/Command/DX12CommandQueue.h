#pragma once
#include "Engine/RHI/Interface/RHICommandQueue.h"


class DX12Device;
class CommandQueueImpl;

class DX12CommandQueue : public RHICommandQueue
{
public:
	~DX12CommandQueue();
	void ExecuteCommandLists(const std::vector<RHICommandList*>& commandLists) override;
	uint64_t Signal(RHIFence* fence) override;
	void WaitGPU(RHIFence* fence, uint64_t value) override;

protected:

	DX12CommandQueue(const DX12Device* dxDevice);

	static std::unique_ptr<DX12CommandQueue> Create(const DX12Device* dxDevice)
	{
		return std::make_unique<DX12CommandQueue>(dxDevice);
	}

	std::unique_ptr<CommandQueueImpl> m_impl = nullptr;

	CommandQueueImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
	friend class DX12SwapChain;
};
