#include "Runtime/Engine/Engine/EngineRender.h"

#include "Runtime/Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemContext.h"
#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Engine/Engine/Subsystem/SceneDocumentSubsystem.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Renderer/RenderSystem.h"
#include "Runtime/Renderer/Scene/WorldRenderBridge.h"

#include <utility>
#include <vector>

struct EngineRender::Impl
{
	RenderSystem render{};
	FrameContext* recording = nullptr;
};

EngineRender::EngineRender()
	: m_impl(std::make_unique<Impl>())
{
}

EngineRender::~EngineRender() = default;

Result<void> EngineRender::Initialize(
	HWND hwnd,
	uint32_t width,
	uint32_t height,
	const EngineLoopConfig& config,
	SubsystemContext& ctx)
{
	return m_impl->render.Initialize(hwnd, width, height, config, ctx);
}

void EngineRender::Shutdown(SubsystemContext& ctx)
{
	m_impl->render.Shutdown(ctx);
}

Result<void> EngineRender::Resize(uint32_t width, uint32_t height)
{
	return m_impl->render.Resize(width, height);
}

Result<void> EngineRender::Render(
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	const ExtractedView& view,
	const ExtractedLighting& lighting,
	std::span<const WorldExtractedObject> objects)
{
	std::vector<ExtractedObject> renderObjects{};
	renderObjects.reserve(objects.size());
	for (const WorldExtractedObject& object : objects)
	{
		renderObjects.push_back(ToExtractedObject(object));
	}

	return m_impl->render.Render(viewportWidth, viewportHeight, view, lighting, renderObjects);
}

bool EngineRender::BeginUiFrame()
{
	return m_impl->render.BeginUiFrame();
}

UiFrameCapture EngineRender::EndUiFrame()
{
	return m_impl->render.EndUiFrame();
}

Result<uint32_t> EngineRender::BeginRecording()
{
	auto started = m_impl->render.BeginRecording();
	if (!started)
	{
		return MakeFail<uint32_t>(started.error.code, started.error.message);
	}

	m_impl->recording = started.value;
	return MakeOk(started.value->frameIndex);
}

Result<void> EngineRender::SubmitAndWait()
{
	m_impl->recording = nullptr;
	return m_impl->render.SubmitAndWait();
}

void EngineRender::AbortRecording()
{
	m_impl->recording = nullptr;
	m_impl->render.AbortRecording();
}

Result<std::vector<WorldDocumentBinding>> EngineRender::UploadPendingScene(
	SceneDocumentSubsystem& scene,
	World& world)
{
	if (m_impl->recording == nullptr)
	{
		return FailInternal<std::vector<WorldDocumentBinding>>(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"EngineRender::UploadPendingScene requires BeginRecording");
	}

	auto uploaded = m_impl->render.UploadScene(
		scene.GetPendingDocument(),
		scene.GetAssetCache(),
		*m_impl->recording);
	if (!uploaded)
	{
		return MakeFail<std::vector<WorldDocumentBinding>>(uploaded.error.code, uploaded.error.message);
	}

	std::vector<SceneModelRef> modelRefs{};
	modelRefs.reserve(uploaded.value.models.size());
	for (const SceneModelAsset& model : uploaded.value.models)
	{
		modelRefs.push_back(SceneModelRef{ model.documentId, model.assets.get() });
	}

	return InstantiateScene(
		world,
		scene.GetPendingDocument(),
		uploaded.value.meshes,
		uploaded.value.render,
		modelRefs);
}

uint32_t EngineRender::GetFrameSlot() const
{
	return m_impl->render.GetRHI().currentFrameSlot;
}
