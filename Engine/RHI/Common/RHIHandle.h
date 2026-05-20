#pragma once
#include <cstdint>

template<typename Tag>
struct RHIHandle
{
    static constexpr uint32_t Invalid = UINT32_MAX;

    uint32_t index = Invalid;

    constexpr bool IsValid() const { return index != Invalid; }
    constexpr bool operator==(const RHIHandle&) const = default;
    constexpr bool operator!=(const RHIHandle& o) const { return !(*this == o); }
};

struct RootSignatureTag {};
struct PipelineStateTag {};

using RootSignatureHandle = RHIHandle<RootSignatureTag>;
using PipelineStateHandle   = RHIHandle<PipelineStateTag>;