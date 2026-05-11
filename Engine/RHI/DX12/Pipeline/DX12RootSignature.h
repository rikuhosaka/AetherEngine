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

	std::unique_ptr<RootSignatureImpl> m_impl = nullptr;

	RootSignatureImpl* GetImpl() const;

	friend class DX12Device;
	friend class DX12CommandList;
};