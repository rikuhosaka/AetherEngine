#pragma once

#include <cstdint>

enum class SessionMode : uint8_t
{
	Editor,
	Playing,
	Paused,
};

// Published by GameSubsystem. Playing runs the game fixed step.
// Paused keeps the play world and skips that step.
struct SessionServices
{
	SessionMode mode = SessionMode::Editor;
};
