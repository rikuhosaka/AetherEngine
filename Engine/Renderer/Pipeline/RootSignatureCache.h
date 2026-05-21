#pragma once

#include "Engine/Core/Containers/ResourcePool.h"
#include "Engine/Core/Handle/Handle.h"
#include "Engine/RHI/Common/RHIRootSignatureLayout.h"
#include "Engine/RHI/Interface/RHIRootSignature.h"

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

    RootSignatureHandle GetOrCreateRootSignature(const RHIRootSignatureLayout& layout);
    [[nodiscard]] RootSignatureHandle AddRootSignature(
        const RHIRootSignatureLayout& layout,
        std::unique_ptr<RHIRootSignature> rootSignature);

    [[nodiscard]] RHIRootSignature* GetRootSignature(RootSignatureHandle handle);

private:

    RHIDevice* m_device;
    std::unordered_map<uint64_t, RootSignatureHandle> m_rootSignatureMap;

    ResourcePool<RootSignatureEntry> m_resourcePool;
};