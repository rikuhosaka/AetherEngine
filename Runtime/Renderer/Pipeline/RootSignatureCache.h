#pragma once

#include "Runtime/Core/Containers/ResourcePool.h"
#include "Runtime/Core/Handle/Handle.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/RHI/Common/RHIRootSignatureLayout.h"
#include "Runtime/RHI/Interface/RHIRootSignature.h"

class RHIDevice;

struct RootSignatureEntry
{
	RHIRootSignatureLayout layout{};
	std::unique_ptr<RHIRootSignature> rootSignature{};
};

using RootSignatureHandle = Handle<RootSignatureEntry>;

class RootSignatureCache
{
public:
	RootSignatureCache(RHIDevice* device);
	~RootSignatureCache();

	Result<RootSignatureHandle> GetOrCreateRootSignature(const RHIRootSignatureLayout& layout);
	[[nodiscard]] RootSignatureHandle AddRootSignature(
		const RHIRootSignatureLayout& layout,
		std::unique_ptr<RHIRootSignature> rootSignature);

	[[nodiscard]] RHIRootSignature* GetRootSignature(RootSignatureHandle handle);

private:
	RHIDevice* m_device;
	std::unordered_map<uint64_t, RootSignatureHandle> m_rootSignatureMap;

	ResourcePool<RootSignatureEntry> m_resourcePool;
};
