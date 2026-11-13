#pragma once

#include "Runtime/Core/Containers/ResourcePool.h"
#include "Runtime/Core/Handle/Handle.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/RHI/Common/RHIPipelineStateLayout.h"
#include "Runtime/RHI/Interface/RHIPipelineState.h"

class RHIDevice;
class RootSignatureCache;

struct PipelineStateEntry
{
	RHIPipelineStateLayout layout{};
	std::unique_ptr<RHIPipelineState> pipelineState{};
};

using PipelineStateHandle = Handle<PipelineStateEntry>;

class PipelineStateCache
{
public:
	PipelineStateCache(RHIDevice* device, RootSignatureCache* rootSignatureCache);
	~PipelineStateCache();

	Result<PipelineStateHandle> GetOrCreatePipelineState(const RHIPipelineStateLayout& layout);
	[[nodiscard]] PipelineStateHandle AddPipelineState(
		const RHIPipelineStateLayout& layout,
		std::unique_ptr<RHIPipelineState> pipelineState);

	[[nodiscard]] RHIPipelineState* GetPipelineState(PipelineStateHandle handle);

private:
	RHIDevice* m_device;
	RootSignatureCache* m_rootSignatureCache;
	std::unordered_map<uint64_t, PipelineStateHandle> m_pipelineStateMap;

	ResourcePool<PipelineStateEntry> m_resourcePool;
};
