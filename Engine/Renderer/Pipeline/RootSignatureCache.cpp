#include "Engine/Renderer/Pipeline/RootSignatureCache.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Pipeline/RootSignatureHash.h"
#include "Engine/RHI/Interface/RHIDevice.h"

RootSignatureCache::RootSignatureCache(RHIDevice* device)
	: m_device(device)
{
}

RootSignatureCache::~RootSignatureCache() = default;

RootSignatureHandle RootSignatureCache::GetOrCreateRootSignature(const RHIRootSignatureLayout& layout)
{
	const std::uint64_t key = HashRootSignatureLayout(layout);
	const auto found = m_rootSignatureMap.find(key);
	if (found != m_rootSignatureMap.end())
	{
		return found->second;
	}

	if (m_device == nullptr)
	{
		return {};
	}

	auto rootSignatureResult = m_device->CreateRootSignature(layout);
	if (!rootSignatureResult)
	{
		LogResult(rootSignatureResult, LogCategory::Renderer);
		return {};
	}

	const RootSignatureHandle handle = AddRootSignature(layout, std::move(rootSignatureResult.value));
	m_rootSignatureMap.emplace(key, handle);
	return handle;
}

RootSignatureHandle RootSignatureCache::AddRootSignature(
	const RHIRootSignatureLayout& layout,
	std::unique_ptr<RHIRootSignature> rootSignature)
{
	auto entry = std::make_unique<RootSignatureEntry>();
	entry->layout = layout;
	entry->rootSignature = std::move(rootSignature);
	return m_resourcePool.Add(std::move(entry));
}

RHIRootSignature* RootSignatureCache::GetRootSignature(RootSignatureHandle handle)
{
	const RootSignatureEntry* entry = m_resourcePool.Get(handle);
	if (entry == nullptr)
	{
		return nullptr;
	}
	return entry->rootSignature.get();
}
