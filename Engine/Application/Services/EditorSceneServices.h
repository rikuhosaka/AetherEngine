#pragma once

#include "Engine/World/SceneDocument.h"

#include <cstddef>
#include <filesystem>

class World;

// Published by Scene. Points at the editor world, never the play clone.
struct EditorSceneServices
{
	const World* world = nullptr;
	const SceneDocument* document = nullptr;
	const WorldDocumentBinding* bindings = nullptr;
	size_t bindingCount = 0;
	std::filesystem::path path{};
};
