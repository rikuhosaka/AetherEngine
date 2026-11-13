#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/SceneDocument/SceneDocument.h"

class World;

// Copies live names, local transforms, and hierarchy. Mesh keys, tint, and bodies stay authored.
[[nodiscard]] Result<SceneDocument> CaptureScene(const World& world, const SceneDocument& authored);
