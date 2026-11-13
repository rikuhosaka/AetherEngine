#pragma once

#include "Runtime/Core/Log/Result.h"

// Process-wide Jolt startup. Safe to call again after ShutdownPhysicsLibrary.
[[nodiscard]] Result<void> InitializePhysicsLibrary();

void ShutdownPhysicsLibrary();
