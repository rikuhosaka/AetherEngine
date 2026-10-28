#pragma once

#include <filesystem>

struct SceneDocument;

// Published by SceneDocument. Points at the open document.
struct EditorSceneServices
{
	const SceneDocument* document = nullptr;
	std::filesystem::path path{};
};
