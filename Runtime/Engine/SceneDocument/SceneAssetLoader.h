#pragma once

#include "Runtime/Core/Log/Result.h"

class AssetCache;
struct SceneDocument;

// Copies mesh and convex-hull collision points from the asset cache onto authored bodies.
[[nodiscard]] Result<void> ApplyMeshCollision(SceneDocument& document, const AssetCache& cache);
