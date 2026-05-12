#pragma once
#include "Engine/RHI/Interface/RHIFence.h"


class DX12Device;
class FenceImpl;

class DX12Fence : public RHIFence
{
public:
	~DX12Fence() override;
	void Increment() override;
	bool IsComplete(uint64_t) override;
	void WaitCPU(uint64_t) override;

private:
	DX12Fence(const DX12Device* dxDevice);
	static std::unique_ptr<DX12Fence> Create(const DX12Device* dxDevice)
	{
		return std::unique_ptr<DX12Fence>(new DX12Fence(dxDevice));
	}

	std::unique_ptr<FenceImpl> m_impl = nullptr;
	
	FenceImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
	friend class DX12CommandQueue;
};