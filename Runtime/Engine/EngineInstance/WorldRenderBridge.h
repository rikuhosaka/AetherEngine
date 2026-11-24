#pragma once

#include "Runtime/Engine/World/Component/Renderable.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Mesh/MeshTypes.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"
#include "Runtime/Renderer/Texture/TextureTypes.h"

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
