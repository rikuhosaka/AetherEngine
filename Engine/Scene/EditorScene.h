#pragma once

#include "Engine/Application/Services/EditorSceneServices.h"
#include "Engine/Scene/SceneDocument.h"
#include "Engine/Scene/SceneLighting.h"
#include "Engine/World/World.h"

#include <filesystem>
#include <span>
#include <vector>

// Editor world. Save reads this. Play uses a separate GameScene.
class EditorScene
{
public:
	[[nodiscard]] World& GetWorld() { return m_world; }
	[[nodiscard]] const World& GetWorld() const { return m_world; }

	[[nodiscard]] bool HasDocument() const { return m_hasDocument; }
	[[nodiscard]] const SceneDocument& GetDocument() const { return m_document; }
	void SetDocument(SceneDocument document);

	[[nodiscard]] std::span<const WorldDocumentBinding> GetBindings() const { return m_bindings; }
	void SetBindings(std::vector<WorldDocumentBinding> bindings);

	[[nodiscard]] const std::filesystem::path& GetPath() const { return m_path; }
	void SetPath(std::filesystem::path path);

	[[nodiscard]] SceneLightingSettings& GetLighting() { return m_lighting; }
	[[nodiscard]] const SceneLightingSettings& GetLighting() const { return m_lighting; }

	void Publish(EditorSceneServices& out) const;

private:
	World m_world{};
	SceneDocument m_document{};
	std::vector<WorldDocumentBinding> m_bindings{};
	std::filesystem::path m_path{};
	SceneLightingSettings m_lighting{};
	bool m_hasDocument = false;
};
