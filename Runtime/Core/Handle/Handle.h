#pragma once

template<typename T>
struct Handle
{
    uint32_t Index = UINT32_MAX;
    uint32_t Generation = 0;

    constexpr bool IsValid() const
    {
        return Index != UINT32_MAX;
    }

    constexpr explicit operator bool() const
    {
        return IsValid();
    }

    auto operator<=>(const Handle&) const = default;
};