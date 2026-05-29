#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIPipelineState.h"
#include "Engine/RHI/Common/RHIPipeline.h"

#include <memory>

class DX12Device;
class PipelineStateImpl;

class DX12PipelineState : public RHIPipelineState
{
public:
	~DX12PipelineState() override;

	[[nodiscard]] bool IsValid() const;

private:
	DX12PipelineState(const RHIPipelineDesc& pipelineDesc, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12PipelineState>> Create(
		const RHIPipelineDesc& pipelineDesc,
		const DX12Device* dxDevice);

	std::unique_ptr<PipelineStateImpl> m_impl = nullptr;

	PipelineStateImpl* GetImpl() const;

	friend class DX12Device;
	friend class DX12CommandList;
};
