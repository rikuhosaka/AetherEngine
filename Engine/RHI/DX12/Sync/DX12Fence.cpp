#include "DX12Fence.h"

#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Sync/FenceImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"

bool DX12Fence::IsValid() const
{
	return m_impl != nullptr && m_impl->fence != nullptr && m_impl->fenceEvent != nullptr;
}

Result<std::unique_ptr<DX12Fence>> DX12Fence::Create(const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12Fence>(new DX12Fence(dxDevice)),
		"Failed to create fence");
}

DX12Fence::DX12Fence(const DX12Device* dxDevice)
{
	m_impl = std::make_unique<FenceImpl>();
	auto deviceImpl = dxDevice->GetImpl();
	auto result = deviceImpl->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_impl->fence));
	if (FAILED(result))
	{
		return;
	}
	m_impl->currentFenceValue = 0;
	m_impl->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_impl->fenceEvent == nullptr)
	{
		m_impl->fence.Reset();
		return;
	}

	DX12GpuNaming::SetName(m_impl->fence.Get(), "Fence/Frame");
}

DX12Fence::~DX12Fence()
{
	if (m_impl && m_impl->fenceEvent)
	{
		CloseHandle(m_impl->fenceEvent);
		m_impl->fenceEvent = nullptr;
	}
}

void DX12Fence::Increment()
{
	m_impl->currentFenceValue++;
}

bool DX12Fence::IsComplete(uint64_t value)
{
	return m_impl->fence->GetCompletedValue() >= value;
}

void DX12Fence::WaitCPU(uint64_t value)
{
	if (!IsComplete(value))
	{
		m_impl->fence->SetEventOnCompletion(value, m_impl->fenceEvent);
		WaitForSingleObject(m_impl->fenceEvent, INFINITE);
	}
}
