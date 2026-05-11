#pragma once
#include "Engine/RHI/Interface/RHIPipelineState.h"
#include "Engine/RHI/Interface/RHICommon.h"


class DX12Device;
class PipelineStateImpl;

class DX12PipelineState : public RHIPipelineState
{
public:

	~DX12PipelineState() override;

private:

	DX12PipelineState(const RHIPipelineDesc& pipelineDesc, const DX12Device* dxDevice);

	std::unique_ptr<PipelineStateImpl> m_impl = nullptr;

	PipelineStateImpl* GetImpl() const;

	friend class DX12Device;
	friend class DX12CommandList;
};