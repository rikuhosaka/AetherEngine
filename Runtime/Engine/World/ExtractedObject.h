#pragma once

#include "Runtime/Engine/World/EntityId.h"
#include "Runtime/Engine/World/Component/Renderable.h"

#include <cstdint>
#include <span>
#include <vector>

struct WorldExtractedParameterBlock
{
	std::span<const std::byte> data{};
	uint32_t bindingSlot = 0;
};

struct WorldExtractedOverrides
{
	WorldTextureId baseColor{};
	WorldTextureId normal{};
	std::vector<WorldExtractedParameterBlock> parameters{};
};

// Renderer の GPU ハンドルへ変換する前の抽出結果。World はこの型だけを書く。
struct WorldExtractedObject
{
	EntityId objectId{};
	WorldMeshId mesh{};
	WorldMaterialId material{};
	WorldExtractedOverrides overrides{};
	uint32_t submeshIndex = 0;
	float worldMatrix[16]{};
	uint8_t layerMask = WorldLayer::Opaque;
	bool visible = true;
};
