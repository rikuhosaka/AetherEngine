#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHICommandQueue.h"

#include <memory>

class DX12Device;
class CommandQueueImpl;

class DX12CommandQueue : public RHICommandQueue
{
public:
	~DX12CommandQueue();
	void ExecuteCommandLists(const std::vector<RHICommandList*>& commandLists) override;
	uint64_t Signal(RHIFence* fence) override;
	void WaitGPU(RHIFence* fence, uint64_t value) override;
	void WaitForIdle() override;

	[[nodiscard]] bool IsValid() const;

protected:
	DX12CommandQueue(const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12CommandQueue>> Create(const DX12Device* dxDevice);

	std::unique_ptr<CommandQueueImpl> m_impl = nullptr;

	CommandQueueImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
	friend class DX12SwapChain;
};
