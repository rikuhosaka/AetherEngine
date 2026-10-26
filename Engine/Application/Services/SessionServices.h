#pragma once

#include <cstdint>

enum class SessionMode : uint8_t
{
	Editor,
	Playing,
	Paused,
};

// Published by World. The UI writes the mode. Playing runs the fixed step.
// Paused keeps the play scene and skips that step.
struct SessionServices
{
	SessionMode mode = SessionMode::Editor;
};
