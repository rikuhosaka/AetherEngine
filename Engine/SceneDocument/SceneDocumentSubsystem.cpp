#include "Engine/SceneDocument/SceneDocumentSubsystem.h"

#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/SceneDocument/SceneAssetLoader.h"

#include <utility>
#include <vector>

namespace
{
constexpr const char* kDependencies[] = { "World", "Render" };
} // namespace

std::unique_ptr<ISubsystem> CreateSceneDocumentSubsystem()
{
	return std::make_unique<SceneDocumentSubsystem>();
}

std::span<const char* const> SceneDocumentSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> SceneDocumentSubsystem::Initialize(SubsystemContext& ctx)
{
	m_content = ctx.GetService<SceneContentServices>();
	if (m_content == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneDocumentSubsystem requires SceneContentServices");
	}

	m_content->loader = this;
	ctx.RegisterService(&m_editorServices);
	return MakeOk();
}

void SceneDocumentSubsystem::Tick(SubsystemContext& /*ctx*/, float /*deltaSeconds*/)
{
	Publish(m_editorServices);
}

Result<void> SceneDocumentSubsystem::Load(
	World& world,
	SubsystemContext& ctx,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	RenderServices* renderServices = ctx.GetService<RenderServices>();
	if (renderServices == nullptr || renderServices->resources == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneDocumentSubsystem requires RenderServices");
	}

	const std::filesystem::path assetsRoot = renderServices->shaderRoot.parent_path();
	const std::filesystem::path scenePath = assetsRoot / "Scenes" / "PhysicsDrop.json";
	Result<SceneDocument> document = LoadSceneDocument(scenePath);
	if (!document)
	{
		return MakeFail(document.error.code, document.error.message);
	}

	Result<void> assetsLoaded = m_assetCache.LoadDirectory(assetsRoot);
	if (!assetsLoaded)
	{
		return MakeFail(assetsLoaded.error.code, assetsLoaded.error.message);
	}

	SceneAssetLoader loader{};
	Result<SceneAssets> assets = loader.Load(
		document.value,
		m_assetCache,
		*renderServices->resources,
		frameContext,
		commandList,
		assetsRoot,
		renderServices->shaderRoot);
	if (!assets)
	{
		return MakeFail(assets.error.code, assets.error.message);
	}

	Result<void> collision = ApplyMeshCollision(document.value, m_assetCache);
	if (!collision)
	{
		return MakeFail(collision.error.code, collision.error.message);
	}

	SetPath(scenePath);
	SetDocument(std::move(document.value));

	std::vector<SceneModelRef> modelRefs{};
	modelRefs.reserve(assets.value.models.size());
	for (const SceneModelAsset& model : assets.value.models)
	{
		modelRefs.push_back(SceneModelRef{ model.documentId, model.assets.get() });
	}

	Result<std::vector<WorldDocumentBinding>> instantiated = InstantiateScene(
		world,
		GetDocument(),
		assets.value.meshes,
		assets.value.render,
		modelRefs);
	if (!instantiated)
	{
		return MakeFail(instantiated.error.code, instantiated.error.message);
	}

	if (m_content != nullptr)
	{
		m_content->loaded = true;
	}

	Publish(m_editorServices);
	return MakeOk();
}

void SceneDocumentSubsystem::Shutdown(SubsystemContext& ctx)
{
	if (m_content != nullptr)
	{
		m_content->loader = nullptr;
		m_content->loaded = false;
		m_content = nullptr;
	}

	ctx.UnregisterService(&m_editorServices);
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
