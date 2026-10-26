#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/SceneDocument/SceneDocument.h"

class World;

// Copies live names, local transforms, and hierarchy. Mesh keys, tint, and bodies stay authored.
[[nodiscard]] Result<SceneDocument> CaptureScene(const World& world, const SceneDocument& authored);
