#pragma once

#include <filesystem>

struct SceneDocument;

// Published by SceneDocument. Points at the open document, never the play clone.
struct EditorSceneServices
{
	const SceneDocument* document = nullptr;
	std::filesystem::path path{};
};
