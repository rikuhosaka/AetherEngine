#pragma once
<<<<<<< HEAD

#include <Engine/RHI/Interface/RHIUploadBuffer.h>

#include <d3d12.h>
#include <memory>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class DX12Device;

=======
#include "Engine/RHI/Interface/RHIUploadBuffer.h"


class DX12Device;
>>>>>>> c0092d2 (shaderSystemのヘッダーファイルを追加)
class DX12UploadBuffer final : public RHIUploadBuffer
{
public:
	~DX12UploadBuffer() override;

	void Reset() override;
	RHIUploadAllocation Allocate(size_t size, size_t alignment) override;

private:
	explicit DX12UploadBuffer(size_t capacityInBytes, const DX12Device* dxDevice);

	static std::unique_ptr<DX12UploadBuffer> Create(size_t capacityInBytes, const DX12Device* dxDevice)
	{
		return std::unique_ptr<DX12UploadBuffer>(new DX12UploadBuffer(capacityInBytes, dxDevice));
	}

<<<<<<< HEAD
	ComPtr<ID3D12Resource> m_resource;
=======
	std::unique_ptr<ResourceImpl> m_impl;

	ResourceImpl* GetImpl() const { return m_impl.get(); }
>>>>>>> c0092d2 (shaderSystemのヘッダーファイルを追加)
	void* m_mappedBase = nullptr;
	uint64_t m_gpuVirtualAddress = 0;
	size_t m_capacity = 0;
	size_t m_cursor = 0;

	friend class DX12Device;
};
