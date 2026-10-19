#include "Engine/Scene/SceneSubsystem.h"

#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Scene/SceneLighting.h"
#include "Engine/Scene/WorldExtract.h"

#include <memory>
#include <utility>
#include <vector>

std::unique_ptr<ISubsystem> CreateSceneSubsystem()
{
	return std::make_unique<SceneSubsystem>();
}

Result<void> SceneSubsystem::Initialize(SubsystemContext& ctx)
{
	m_context = &ctx;
	m_session.mode = SessionMode::Editor;
	m_appliedMode = SessionMode::Editor;
	PublishWorld();
	m_editor.Publish(m_editorServices);
	ctx.RegisterService(&m_debugStats);
	ctx.RegisterService(&m_worldServices);
	ctx.RegisterService(&m_session);
	ctx.RegisterService(&m_editorServices);
	return MakeOk();
}

Result<void> SceneSubsystem::PostInitialize(SubsystemContext& ctx)
{
	if (auto* registry = ctx.GetService<SubsystemRegistry>())
	{
		registry->SetSceneExtractor(this);
	}

	return MakeOk();
}

void SceneSubsystem::Tick(SubsystemContext& ctx, float deltaSeconds)
{
	m_editor.Publish(m_editorServices);
	ApplySession();
	PublishWorld();

	const bool sessionOpen = m_appliedMode == SessionMode::Playing;
	const bool wantCamera = m_session.mode == SessionMode::Playing || m_session.mode == SessionMode::Paused;
	if (sessionOpen && wantCamera && m_game.has_value())
	{
		m_game->Tick(ctx, deltaSeconds);
	}
}

void SceneSubsystem::FixedTick(SubsystemContext& /*ctx*/, float fixedDeltaSeconds)
{
	if (m_session.mode != SessionMode::Playing)
	{
		return;
	}

	if (auto step = Step(fixedDeltaSeconds); !step)
	{
		LogResult(step, LogCategory::Physics);
	}
}

Result<void> SceneSubsystem::LoadContent(
	SubsystemContext& ctx,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	RenderServices* renderServices = ctx.GetService<RenderServices>();
	if (renderServices == nullptr || renderServices->resources == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneSubsystem requires RenderServices");
	}

	const std::filesystem::path scenePath = renderServices->shaderRoot.parent_path() / "Scenes" / "PhysicsDrop.json";
	if (auto initResult = m_drop.EnsureInitialized(
			m_editor,
			*renderServices->resources,
			frameContext,
			commandList,
			renderServices->shaderRoot,
			scenePath);
		!initResult)
	{
		return initResult;
	}

	if (auto spawned = m_drop.SpawnInto(m_editor); !spawned)
	{
		return spawned;
	}

	m_editor.Publish(m_editorServices);
	PublishWorld();
	return MakeOk();
}

void SceneSubsystem::Extract(std::vector<ExtractedObject>& outObjects)
{
	if (m_context == nullptr)
	{
		return;
	}

	World* world = m_game.has_value() ? &m_game->GetWorld() : &m_editor.GetWorld();
	float alpha = m_context->GetInterpolationAlpha();
	if (m_session.mode == SessionMode::Paused)
	{
		alpha = 1.0f;
	}

	if (m_game.has_value())
	{
		m_game->WriteRenderTransforms(alpha);
	}

	ExtractWorld(*world, outObjects);
	m_debugStats.extractedObjectCount = static_cast<uint32_t>(outObjects.size());
}

void SceneSubsystem::ExtractView(ExtractedView& outView)
{
	if (m_context == nullptr)
	{
		return;
	}

	if (m_game.has_value())
	{
		const WindowServices* windowServices = m_context->GetService<WindowServices>();
		const uint32_t width = windowServices != nullptr ? windowServices->clientWidth : 0;
		const uint32_t height = windowServices != nullptr ? windowServices->clientHeight : 0;
		m_game->ExtractView(width, height, outView);
		return;
	}

	const EditorCameraServices* camera = m_context->GetService<EditorCameraServices>();
	if (camera != nullptr && camera->valid)
	{
		outView = camera->view;
	}
}

void SceneSubsystem::ExtractLighting(ExtractedLighting& outLighting)
{
	if (m_game.has_value())
	{
		m_game->ExtractLighting(outLighting);
		return;
	}

	BuildExtractedLighting(m_editor.GetLighting(), outLighting);
}

void SceneSubsystem::Shutdown(SubsystemContext& ctx)
{
	EndPlay();
	m_appliedMode = SessionMode::Editor;

	if (auto* registry = ctx.GetService<SubsystemRegistry>())
	{
		if (registry->GetSceneExtractor() == this)
		{
			registry->SetSceneExtractor(nullptr);
		}
	}

	ctx.UnregisterService(&m_editorServices);
	m_editorServices = {};
	ctx.UnregisterService(&m_session);
	m_session = {};
	ctx.UnregisterService(&m_worldServices);
	m_worldServices = {};
	ctx.UnregisterService(&m_debugStats);
	m_debugStats = {};
	m_context = nullptr;
}

void SceneSubsystem::ApplySession()
{
	const bool sessionOpen = m_appliedMode == SessionMode::Playing;
	const bool wantSession = m_session.mode == SessionMode::Playing || m_session.mode == SessionMode::Paused;
	if (wantSession && !sessionOpen)
	{
		if (m_session.mode == SessionMode::Paused)
		{
			m_session.mode = SessionMode::Editor;
		}
		else if (auto begun = BeginPlay(); !begun)
		{
			LogResult(begun, LogCategory::Core);
			m_session.mode = SessionMode::Editor;
		}
		else
		{
			m_appliedMode = SessionMode::Playing;
		}
	}
	else if (!wantSession && sessionOpen)
	{
		EndPlay();
		m_appliedMode = SessionMode::Editor;
	}
}

void SceneSubsystem::PublishWorld()
{
	m_worldServices.world = m_game.has_value() ? &m_game->GetWorld() : &m_editor.GetWorld();
}

Result<void> SceneSubsystem::BeginPlay()
{
	if (m_game.has_value())
	{
		return MakeOk();
	}

	const EditorCameraServices* camera = m_context != nullptr ? m_context->GetService<EditorCameraServices>() : nullptr;
	Result<GameScene> opened = GameScene::Begin(m_editor, m_drop, camera);
	if (!opened)
	{
		return MakeFail(opened.error.code, opened.error.message);
	}

	m_game = std::move(opened.value);
	return MakeOk();
}

void SceneSubsystem::EndPlay()
{
	m_game.reset();
}

Result<void> SceneSubsystem::Step(float fixedDeltaSeconds)
{
	if (!m_game.has_value())
	{
		return MakeOk();
	}

	return m_game->Step(fixedDeltaSeconds);
}
