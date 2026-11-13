#pragma once

#include "Runtime/Engine/World/ExtractedObject.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"

#include <cstdint>

static_assert(WorldLayer::Opaque == RenderLayer::Opaque);
static_assert(WorldLayer::Shadow == RenderLayer::Shadow);
static_assert(WorldLayer::Transparent == RenderLayer::Transparent);

[[nodiscard]] inline WorldMeshId ToWorldMeshId(MeshHandle handle)
{
	return WorldMeshId{ handle.Index, handle.Generation };
}

[[nodiscard]] inline MeshHandle ToMeshHandle(WorldMeshId id)
{
	return MeshHandle{ id.Index, id.Generation };
}

[[nodiscard]] inline WorldMaterialId ToWorldMaterialId(MaterialHandle handle)
{
	return WorldMaterialId{ handle.Index, handle.Generation };
}

[[nodiscard]] inline MaterialHandle ToMaterialHandle(WorldMaterialId id)
{
	return MaterialHandle{ id.Index, id.Generation };
}

[[nodiscard]] inline WorldTextureId ToWorldTextureId(TextureHandle handle)
{
	return WorldTextureId{ handle.Index, handle.Generation };
}

[[nodiscard]] inline TextureHandle ToTextureHandle(WorldTextureId id)
{
	return TextureHandle{ id.Index, id.Generation };
}

[[nodiscard]] inline ExtractedObject ToExtractedObject(const WorldExtractedObject& source)
{
	ExtractedObject object{};
	object.objectId.Index = source.objectId.Index;
	object.objectId.Generation = source.objectId.Generation;
	object.mesh = ToMeshHandle(source.mesh);
	object.material = ToMaterialHandle(source.material);
	object.submeshIndex = source.submeshIndex;
	for (int element = 0; element < 16; ++element)
	{
		object.worldMatrix[element] = source.worldMatrix[element];
	}
	object.layerMask = source.layerMask;
	object.visible = source.visible;
	object.overrides.baseColor = ToTextureHandle(source.overrides.baseColor);
	object.overrides.normal = ToTextureHandle(source.overrides.normal);
	object.overrides.parameters.reserve(source.overrides.parameters.size());
	for (const WorldExtractedParameterBlock& block : source.overrides.parameters)
	{
		MaterialParameterBlock extractedBlock{};
		extractedBlock.bindingSlot = block.bindingSlot;
		extractedBlock.data = block.data;
		object.overrides.parameters.push_back(extractedBlock);
	}
	return object;
}
