#pragma once


inline void HashCombine(
    size_t& seed,
    size_t value)
{
    seed ^= value + 0x9e3779b9
        + (seed << 6)
        + (seed >> 2);
}