#include "Runtime/Engine/EngineInstance/Subsystem/SceneDocumentSubsystem.h"

#include "Runtime/Engine/Application/Subsystem/SubsystemContext.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Engine/EngineInstance/Subsystem/Engine.h"
#include "Runtime/Engine/SceneDocument/SceneAssetLoader.h"
#include "Runtime/Engine/SceneDocument/SceneDocument.h"

#include <utility>

namespace
{
constexpr const char* kDependencies[] = { "Engine" };
} // namespace

SceneDocumentSubsystem::SceneDocumentSubsystem(
	std::filesystem::path assetsRoot,
	std::filesystem::path scenePath)
	: m_assetsRoot(std::move(assetsRoot))
	, m_scenePath(std::move(scenePath))
{
}

std::unique_ptr<ISubsystem> CreateSceneDocumentSubsystem(
	std::filesystem::path assetsRoot,
	std::filesystem::path scenePath)
{
	return std::make_unique<SceneDocumentSubsystem>(std::move(assetsRoot), std::move(scenePath));
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
	return ReadScene(m_scenePath);
}

Result<void> SceneDocumentSubsystem::ReadScene(const std::filesystem::path& scenePath)
{
	if (m_assetsRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneDocumentSubsystem requires an assets root");
	}

	if (scenePath.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneDocumentSubsystem requires a scene path");
	}

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

	m_pending = std::move(document.value);
	m_pendingPath = scenePath;
	m_hasPending = true;
	return MakeOk();
}

void SceneDocumentSubsystem::CommitPending()
{
	if (!m_hasPending)
	{
		return;
	}

	SetPath(std::move(m_pendingPath));
	SetDocument(std::move(m_pending));
	m_pending = {};
	m_pendingPath.clear();
	m_hasPending = false;
	Publish(m_editorServices);
}

void SceneDocumentSubsystem::DiscardPending()
{
	m_pending = {};
	m_pendingPath.clear();
	m_hasPending = false;
}

void SceneDocumentSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	if (m_content != nullptr)
	{
		m_content->loader = nullptr;
		m_content->loaded = false;
		m_content = nullptr;
	}

	DiscardPending();
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
