#include "Runtime/Engine/EngineInstance/EngineRender.h"

#include "Runtime/Renderer/RenderSystemConfig.h"
#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Engine/EngineInstance/Subsystem/SceneDocumentSubsystem.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Engine/EngineInstance/ModelSceneSpawn.h"
#include "Runtime/Engine/EngineInstance/SceneGpuUpload.h"
#include "Runtime/Engine/EngineInstance/WorldRenderBridge.h"
#include "Runtime/Engine/SceneDocument/SceneDocument.h"
#include "Runtime/Renderer/RenderSystem.h"

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
	const EngineLoopConfig& config)
{
	RenderSystemConfig renderConfig{};
	renderConfig.renderer.shaderRoot = config.shaderRoot;
	renderConfig.renderer.compiledShaderRoot = config.compiledShaderRoot;
	renderConfig.renderer.assetsRoot = config.assetsRoot;
	renderConfig.renderer.visualizeSceneDepth = config.visualizeSceneDepth;
	renderConfig.dx12DebugConfigPath = config.dx12DebugConfigPath;
	renderConfig.vsync = config.vsync;
	return m_impl->render.Initialize(hwnd, width, height, renderConfig);
}

void EngineRender::Shutdown()
{
	m_impl->render.Shutdown();
}

Result<void> EngineRender::Resize(uint32_t width, uint32_t height)
{
	return m_impl->render.Resize(width, height);
}

Result<void> EngineRender::Render(
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	const RenderFrame& frame)
{
	return m_impl->render.Render(viewportWidth, viewportHeight, frame);
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

	SceneGpuUploadSources sources{};
	if (auto built = BuildSceneGpuUpload(
			scene.GetPendingDocument(),
			scene.GetAssetCache(),
			scene.GetAssetsRoot(),
			sources);
		!built)
	{
		return MakeFail<std::vector<WorldDocumentBinding>>(built.error.code, built.error.message);
	}

	auto uploaded = m_impl->render.UploadScene(sources.upload, *m_impl->recording);
	if (!uploaded)
	{
		return MakeFail<std::vector<WorldDocumentBinding>>(uploaded.error.code, uploaded.error.message);
	}

	std::vector<SceneMeshRef> meshes{};
	meshes.reserve(uploaded.value.meshes.size());
	for (const UploadedMeshRef& mesh : uploaded.value.meshes)
	{
		meshes.push_back(SceneMeshRef{ mesh.key, ToWorldMeshId(mesh.mesh) });
	}

	SceneRenderResources render{};
	render.material = ToWorldMaterialId(uploaded.value.render.material);
	render.baseColor = ToWorldTextureId(uploaded.value.render.baseColor);
	render.materialConstantsSlot = uploaded.value.render.materialConstantsSlot;

	auto instantiated = InstantiateScene(
		world,
		scene.GetPendingDocument(),
		meshes,
		render);
	if (!instantiated)
	{
		return instantiated;
	}

	if (auto spawned = SpawnUploadedModels(world, instantiated.value, uploaded.value.models); !spawned)
	{
		for (const WorldDocumentBinding& binding : instantiated.value)
		{
			world.Destroy(binding.entity);
		}

		return MakeFail<std::vector<WorldDocumentBinding>>(spawned.error.code, spawned.error.message);
	}

	return instantiated;
}

uint32_t EngineRender::GetFrameSlot() const
{
	return m_impl->render.GetRHI().currentFrameSlot;
}
