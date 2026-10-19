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

void EditorScene::SetPhysics(EntityPhysics physics)
{
	m_physics = std::move(physics);
}

void EditorScene::SetPath(std::filesystem::path path)
{
	m_path = std::move(path);
}

void EditorScene::Publish(EditorSceneServices& out) const
{
	out = {};
	if (!m_hasDocument)
	{
		return;
	}

	out.world = &m_world;
	out.document = &m_document;
	out.bindings = m_bindings.data();
	out.bindingCount = m_bindings.size();
	out.path = m_path;
}
