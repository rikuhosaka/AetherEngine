#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/World/WorldDocument.h"
#include "Runtime/Renderer/Scene/SceneAssetUpload.h"

#include <span>
#include <vector>

class World;

// Parents each uploaded model under the entity bound to its document id.
// A failure leaves the new entities in the world; the caller destroys the document bindings.
[[nodiscard]] Result<void> SpawnUploadedModels(
	World& world,
	std::span<const WorldDocumentBinding> bindings,
	std::span<const SceneModelAsset> models);
