#include "Engine/SceneDocument/SceneDocumentSubsystem.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Engine/Engine.h"
#include "Engine/SceneDocument/SceneAssetLoader.h"
#include "Engine/SceneDocument/SceneDocument.h"

#include <utility>

namespace
{
constexpr const char* kDependencies[] = { "Engine" };
} // namespace

SceneDocumentSubsystem::SceneDocumentSubsystem(std::filesystem::path assetsRoot)
	: m_assetsRoot(std::move(assetsRoot))
{
}

std::unique_ptr<ISubsystem> CreateSceneDocumentSubsystem(std::filesystem::path assetsRoot)
{
	return std::make_unique<SceneDocumentSubsystem>(std::move(assetsRoot));
}

std::span<const char* const> SceneDocumentSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> SceneDocumentSubsystem::Initialize(SubsystemContext& /*ctx*/)
{
	Engine* engine = SubsystemRegistry::FindCurrent<Engine>();
	if (engine == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneDocumentSubsystem requires Engine");
	}

	m_content = &engine->GetSceneContent();
	m_content->loader = this;
	return MakeOk();
}

void SceneDocumentSubsystem::Tick(SubsystemContext& /*ctx*/, float /*deltaSeconds*/)
{
	Publish(m_editorServices);
}

Result<void> SceneDocumentSubsystem::Load(World& /*world*/, SubsystemContext& /*ctx*/)
{
	if (m_assetsRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneDocumentSubsystem requires an assets root");
	}

	const std::filesystem::path scenePath = m_assetsRoot / "Scenes" / "PhysicsDrop.json";
	Result<SceneDocument> document = LoadSceneDocument(scenePath);
	if (!document)
	{
		return MakeFail(document.error.code, document.error.message);
	}

	Result<void> assetsLoaded = m_assetCache.LoadDirectory(m_assetsRoot);
	if (!assetsLoaded)
	{
		return MakeFail(assetsLoaded.error.code, assetsLoaded.error.message);
	}

	Result<void> collision = ApplyMeshCollision(document.value, m_assetCache);
	if (!collision)
	{
		return MakeFail(collision.error.code, collision.error.message);
	}

	SetPath(scenePath);
	SetDocument(std::move(document.value));
	Publish(m_editorServices);
	return MakeOk();
}

void SceneDocumentSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	if (m_content != nullptr)
	{
		m_content->loader = nullptr;
		m_content->loaded = false;
		m_content = nullptr;
	}

	m_editorServices = {};
	m_hasDocument = false;
}

void SceneDocumentSubsystem::SetDocument(SceneDocument document)
{
	m_document = std::move(document);
	m_hasDocument = true;
}

void SceneDocumentSubsystem::SetPath(std::filesystem::path path)
{
	m_path = std::move(path);
}

void SceneDocumentSubsystem::Publish(EditorSceneServices& out) const
{
	if (!m_hasDocument)
	{
		out.document = nullptr;
		out.path.clear();
		return;
	}

	out.document = &m_document;
	out.path = m_path;
}
