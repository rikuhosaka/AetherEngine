#pragma once

#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct WorldMaterialParameterBlock
{
	std::vector<std::byte> data{};
	uint32_t bindingSlot = 0;
};

struct WorldMaterialOverrides
{
	TextureHandle baseColor{};
	TextureHandle normal{};
	std::vector<WorldMaterialParameterBlock> parameters{};
};

struct Renderable
{
	MeshHandle mesh{};
	MaterialHandle material{};
	WorldMaterialOverrides overrides{};
	uint32_t submeshIndex = 0;
	uint8_t layerMask = RenderLayer::Opaque;
	bool visible = true;
};
