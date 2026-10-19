#pragma once

#include "Engine/World/SceneDocument.h"

#include <filesystem>
#include <vector>

class World;

// Published by Scene. Points at the editor world, never the play clone.
// bindings is a copy, so it stays valid after the editor scene replaces its own list.
struct EditorSceneServices
{
	const World* world = nullptr;
	const SceneDocument* document = nullptr;
	std::vector<WorldDocumentBinding> bindings{};
	std::filesystem::path path{};
};
