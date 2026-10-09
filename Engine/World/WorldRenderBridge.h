#pragma once

#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Engine/World/Renderable.h"

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
