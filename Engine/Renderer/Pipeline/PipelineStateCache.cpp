#include "Engine/Renderer/Pipeline/PipelineStateCache.h"

#include "Engine/Renderer/Pipeline/PipelineStateHash.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/RHI/Common/RHIPipeline.h"
#include "Engine/RHI/Interface/RHIDevice.h"

namespace
{
RHIPipelineDesc ToPipelineDesc(
	const RHIPipelineStateLayout& layout,
	RHIRootSignature* rootSignature)
{
	RHIPipelineDesc desc{};
	desc.vertexShader = const_cast<RHIVertexShader*>(layout.vertexShader);
	desc.pixelShader = const_cast<RHIPixelShader*>(layout.pixelShader);
	desc.inputLayout = layout.inputLayout;
	desc.topology = layout.topology;
	desc.raster = layout.raster;
	desc.blend = layout.blend;
	desc.depth = layout.depth;
	desc.numRT = layout.numRT;
	desc.sampleCount = layout.sampleCount;
	desc.dsvFormat = layout.dsvFormat;
	for (uint8_t i = 0; i < layout.numRT; ++i)
	{
		desc.rtvFormats[i] = layout.rtvFormats[i];
	}
	desc.rootSignature = rootSignature;
	return desc;
}
} // namespace

PipelineStateCache::PipelineStateCache(RHIDevice* device, RootSignatureCache* rootSignatureCache)
	: m_device(device)
	, m_rootSignatureCache(rootSignatureCache)
{
}

PipelineStateCache::~PipelineStateCache() = default;

PipelineStateHandle PipelineStateCache::GetOrCreatePipelineState(const RHIPipelineStateLayout& layout)
{
	const std::uint64_t key = HashPipelineStateLayout(layout);
	const auto found = m_pipelineStateMap.find(key);
	if (found != m_pipelineStateMap.end())
	{
		return found->second;
	}

	if (m_device == nullptr || m_rootSignatureCache == nullptr)
	{
		return {};
	}

	const RootSignatureHandle rootSignatureHandle =
		m_rootSignatureCache->GetOrCreateRootSignature(layout.rootSignature);
	RHIRootSignature* rootSignature = m_rootSignatureCache->GetRootSignature(rootSignatureHandle);
	if (rootSignature == nullptr)
	{
		return {};
	}

	const RHIPipelineDesc pipelineDesc = ToPipelineDesc(layout, rootSignature);
	std::unique_ptr<RHIPipelineState> pipelineState = m_device->CreatePipelineState(pipelineDesc);
	if (pipelineState == nullptr)
	{
		return {};
	}

	const PipelineStateHandle handle = AddPipelineState(layout, std::move(pipelineState));
	m_pipelineStateMap.emplace(key, handle);
	return handle;
}

PipelineStateHandle PipelineStateCache::AddPipelineState(
	const RHIPipelineStateLayout& layout,
	std::unique_ptr<RHIPipelineState> pipelineState)
{
	auto entry = std::make_unique<PipelineStateEntry>();
	entry->layout = layout;
	entry->pipelineState = std::move(pipelineState);
	return m_resourcePool.Add(std::move(entry));
}

RHIPipelineState* PipelineStateCache::GetPipelineState(PipelineStateHandle handle)
{
	const PipelineStateEntry* entry = m_resourcePool.Get(handle);
	if (entry == nullptr)
	{
		return nullptr;
	}
	return entry->pipelineState.get();
}
