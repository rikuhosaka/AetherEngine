#pragma once

#include <Engine/RHI/Interface/RHIRootSignature.h>


class DX12Device;
class RootSignatureImpl;

class DX12RootSignature : public RHIRootSignature
{
public:
	~DX12RootSignature() override;

private:

	DX12RootSignature(const DX12Device* dxDevice);

	static std::unique_ptr<DX12RootSignature> Create(const DX12Device* dxDevice)
	{
		return std::unique_ptr<DX12RootSignature>(new DX12RootSignature(dxDevice));
	}

	std::unique_ptr<RootSignatureImpl> m_impl = nullptr;

	RootSignatureImpl* GetImpl() const;

	friend class DX12Device;
	friend class DX12CommandList;
};