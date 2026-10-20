#include "Engine/Scene/EditorScene.h"

#include <utility>

void EditorScene::SetDocument(SceneDocument document)
{
	m_document = std::move(document);
	m_hasDocument = true;
}

void EditorScene::SetBindings(std::vector<WorldDocumentBinding> bindings)
{
	m_bindings = std::move(bindings);
}

void EditorScene::SetPath(std::filesystem::path path)
{
	m_path = std::move(path);
}

void EditorScene::Publish(EditorSceneServices& out) const
{
	if (!m_hasDocument)
	{
		out.world = nullptr;
		out.document = nullptr;
		out.bindings.clear();
		out.path.clear();
		return;
	}

	out.world = &m_world;
	out.document = &m_document;
	out.bindings = m_bindings;
	out.path = m_path;
}
