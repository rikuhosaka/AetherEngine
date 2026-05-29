#include "Engine/Renderer/Pipeline/RootSignatureCache.h"

#include "Engine/Renderer/Pipeline/RootSignatureHash.h"
#include "Engine/RHI/Interface/RHIDevice.h"

RootSignatureCache::RootSignatureCache(RHIDevice* device)
	: m_device(device)
{
}

RootSignatureCache::~RootSignatureCache() = default;

Result<RootSignatureHandle> RootSignatureCache::GetOrCreateRootSignature(
	const RHIRootSignatureLayout& layout)
{
	const std::uint64_t key = HashRootSignatureLayout(layout);
	const auto found = m_rootSignatureMap.find(key);
	if (found != m_rootSignatureMap.end())
	{
		return MakeOk(found->second);
	}

	if (m_device == nullptr)
	{
		return MakeFail<RootSignatureHandle>(
			ErrorCode::InvalidArgument,
			"Root signature cache requires a valid RHIDevice.");
	}

	auto rootSignatureResult = m_device->CreateRootSignature(layout);
	if (!rootSignatureResult)
	{
		return MakeFail<RootSignatureHandle>(
			rootSignatureResult.error.code,
			rootSignatureResult.error.message);
	}

	const RootSignatureHandle handle = AddRootSignature(layout, std::move(rootSignatureResult.value));
	m_rootSignatureMap.emplace(key, handle);
	return MakeOk(handle);
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
