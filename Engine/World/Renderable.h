#pragma once

#include "Engine/Core/Handle/Handle.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct WorldMeshTag
{
};

struct WorldMaterialTag
{
};

struct WorldTextureTag
{
};

using WorldMeshId = Handle<WorldMeshTag>;
using WorldMaterialId = Handle<WorldMaterialTag>;
using WorldTextureId = Handle<WorldTextureTag>;

namespace WorldLayer
{
constexpr uint8_t Opaque = 1u << 0;
constexpr uint8_t Shadow = 1u << 1;
constexpr uint8_t Transparent = 1u << 2;
} // namespace WorldLayer

struct WorldMaterialParameterBlock
{
	std::vector<std::byte> data{};
	uint32_t bindingSlot = 0;
};

struct WorldMaterialOverrides
{
	WorldTextureId baseColor{};
	WorldTextureId normal{};
	std::vector<WorldMaterialParameterBlock> parameters{};
};

struct Renderable
{
	WorldMeshId mesh{};
	WorldMaterialId material{};
	WorldMaterialOverrides overrides{};
	uint32_t submeshIndex = 0;
	uint8_t layerMask = WorldLayer::Opaque;
	bool visible = true;
};
