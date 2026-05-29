#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIRootSignature.h"
#include "Engine/RHI/Common/RHIRootSignatureLayout.h"

#include <memory>

class DX12Device;
class RootSignatureImpl;

class DX12RootSignature : public RHIRootSignature
{
public:
	~DX12RootSignature() override;

	[[nodiscard]] bool IsValid() const;

private:
	DX12RootSignature(const DX12Device* dxDevice, const RHIRootSignatureLayout& layout);

	static Result<std::unique_ptr<DX12RootSignature>> Create(
		const DX12Device* dxDevice,
		const RHIRootSignatureLayout& layout);

	std::unique_ptr<RootSignatureImpl> m_impl = nullptr;

	RootSignatureImpl* GetImpl() const;

	friend class DX12Device;
	friend class DX12CommandList;
	friend class DX12PipelineState;
};
