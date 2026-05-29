#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIFence.h"

#include <memory>

class DX12Device;
class FenceImpl;

class DX12Fence : public RHIFence
{
public:
	~DX12Fence() override;
	void Increment() override;
	bool IsComplete(uint64_t) override;
	void WaitCPU(uint64_t) override;

	[[nodiscard]] bool IsValid() const;

private:
	DX12Fence(const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12Fence>> Create(const DX12Device* dxDevice);

	std::unique_ptr<FenceImpl> m_impl = nullptr;

	FenceImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
	friend class DX12CommandQueue;
};
