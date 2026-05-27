#pragma once

#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/RenderItem/RenderItem.h"
#include "Engine/Renderer/Scene/RenderObjectId.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

#include <cstdint>
#include <span>
#include <vector>

namespace RenderLayer
{
constexpr uint8_t Opaque = 1u << 0;
constexpr uint8_t Shadow = 1u << 1;
constexpr uint8_t Transparent = 1u << 2;
} // namespace RenderLayer

struct ObjectConstants
{
	float worldMatrix[16]{};
};

struct MaterialParameterBlock
{
    std::span<const std::byte> data;

    uint32_t bindingSlot;
};

struct MaterialParameterOverrides
{
    TextureHandle baseColor;

    TextureHandle normal;

    std::vector<MaterialParameterBlock> parameters;
};

struct ExtractedObject
{
	RenderObjectId objectId{};
	MeshHandle mesh{};
	MaterialHandle material{};
	MaterialParameterOverrides overrides{};
	uint32_t submeshIndex = 0;
	float worldMatrix[16]{};
	uint8_t layerMask = RenderLayer::Opaque;
	bool visible = true;
};

struct StoredMaterialParameterBlock
{
	std::vector<std::byte> data{};
	uint32_t bindingSlot = 0;
};

struct StoredMaterialParameterOverrides
{
	TextureHandle baseColor{};
	TextureHandle normal{};
	std::vector<StoredMaterialParameterBlock> parameters{};
};

struct ExtractedObjectSnapshot
{
	RenderObjectId objectId{};
	MeshHandle mesh{};
	MaterialHandle material{};
	StoredMaterialParameterOverrides overrides{};
	uint32_t submeshIndex = 0;
	float worldMatrix[16]{};
	uint8_t layerMask = RenderLayer::Opaque;
	bool visible = true;
};

struct ExtractedFrame
{
	uint32_t frameIndex = 0;
	std::vector<ExtractedObjectSnapshot> objects{};
};

struct RenderFrameSnapshot
{
	uint32_t frameIndex = 0;
	std::vector<RenderItem> opaqueItems{};
	std::vector<RenderItem> shadowItems{};
	std::vector<RenderItem> transparentItems{};
	std::vector<ObjectConstants> objectConstants{};
};
