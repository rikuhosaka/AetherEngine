#include "Game/DefaultGameModule.h"

#include "Game/Camera/CameraExtract.h"
#include "Game/IGameHost.h"

#include "Engine/Platform/InputState.h"

#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Game/WorldExtract.h"

Result<void> DefaultGameModule::OnInit(IGameHost& /*host*/)
{
	m_cameraController.Reset(m_camera, kCameraStartPosition, kCameraStartForward);
	return MakeOk();
}

Result<void> DefaultGameModule::OnLoadContent(
	IGameHost& host,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	RenderServices* renderServices = host.GetRenderServices();
	if (renderServices == nullptr || renderServices->resources == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"DefaultGameModule requires RenderServices");
	}

	RenderResourceServices& resources = *renderServices->resources;
	const std::filesystem::path scenePath = host.GetShaderRoot().parent_path() / "Scenes" / "PhysicsDrop.json";
	if (auto initResult = m_drop.EnsureInitialized(
			resources,
			frameContext,
			commandList,
			host.GetShaderRoot(),
			scenePath);
		!initResult)
	{
		return initResult;
	}

	return m_drop.SpawnInto(m_world);
}

Result<std::string> DefaultGameModule::CaptureEditorScene() const
{
	return m_drop.CaptureEditor(m_world);
}

void DefaultGameModule::OnTick(IGameHost& host, float deltaSeconds)
{
	if (const InputState* input = host.GetInput())
	{
		const bool useMouse = !host.IsMouseCaptured();
		const bool useKeyboard = !host.IsKeyboardCaptured();
		ActiveCameraController().Update(ActiveCamera(), *input, deltaSeconds, useMouse, useKeyboard);
		host.SetRelativeMouse(useMouse && input->IsDown(Key::MouseRight));
	}
}

Result<void> DefaultGameModule::OnBeginPlay(IGameHost& /*host*/)
{
	if (m_playWorld.has_value())
	{
		return MakeOk();
	}

	m_playWorld = m_world.Clone();
	if (auto begun = m_drop.BeginPlay(*m_playWorld); !begun)
	{
		m_playWorld.reset();
		return begun;
	}

	m_playCameraController.Reset(m_playCamera, m_camera.position, m_camera.forward);
	return MakeOk();
}

void DefaultGameModule::OnEndPlay(IGameHost& /*host*/)
{
	m_drop.EndPlay();
	m_playWorld.reset();
}

void DefaultGameModule::OnFixedTick(IGameHost& /*host*/, float fixedDeltaSeconds)
{
	World* world = ActiveWorld();
	if (world == nullptr)
	{
		return;
	}

	if (auto step = m_drop.Step(*world, fixedDeltaSeconds); !step)
	{
		LogResult(step, LogCategory::Physics);
	}
}

void DefaultGameModule::OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects)
{
	World* world = ActiveWorld();
	if (world == nullptr)
	{
		return;
	}

	float alpha = host.GetInterpolationAlpha();
	if (const SessionServices* session = host.GetContext().GetService<SessionServices>())
	{
		if (session->mode == SessionMode::Paused)
		{
			alpha = 1.0f;
		}
	}
	m_drop.WriteRenderTransforms(*world, alpha);
	ExtractWorld(*world, outObjects);
}

void DefaultGameModule::OnExtractView(IGameHost& host, ExtractedView& outView)
{
	const auto [viewportWidth, viewportHeight] = host.GetViewportSize();
	BuildExtractedView(ActiveCamera(), viewportWidth, viewportHeight, outView);
}

void DefaultGameModule::OnExtractLighting(IGameHost& /*host*/, ExtractedLighting& outLighting)
{
	BuildExtractedLighting(m_lighting, outLighting);
}

void DefaultGameModule::OnShutdown(IGameHost& host)
{
	OnEndPlay(host);
}

IGameModule* CreateGameModule()
{
	return new DefaultGameModule();
}
