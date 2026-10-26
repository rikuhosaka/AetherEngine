#pragma once

#include "Engine/Application/Subsystem/ISceneContent.h"

// Published by World. The document subsystem installs the loader and marks content loaded.
struct SceneContentServices
{
	ISceneContent* loader = nullptr;
	bool loaded = false;
};
