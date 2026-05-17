#pragma once


// CBV (b#)
constexpr uint32_t DRAW_INFO = 0;
constexpr uint32_t CBV_VIEWPROJ = 1;

// SRV (t#)
constexpr uint32_t SRV_WORLD_MAT = 2;
constexpr uint32_t SRV_BONES = 3;
constexpr uint32_t SRV_MATERIAL = 4;
constexpr uint32_t SRV_TEXTURES = 5;

// Sampler (s#)
constexpr uint32_t SAMPLER_LINEAR = 0;
constexpr uint32_t SAMPLER_ANISO = 1;
constexpr uint32_t SAMPLER_WRAP = 2;