#include "Game/DefaultGameModule.h"

#include "Game/Camera/CameraExtract.h"
#include "Game/IGameHost.h"

#include "Engine/Platform/InputState.h"

#include "Engine/Application/Services/RenderServices.h"
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
	if (auto initResult = m_drop.EnsureInitialized(
			resources,
			frameContext,
			commandList,
			host.GetShaderRoot());
		!initResult)
	{
		return initResult;
	}

	return m_drop.SpawnInto(m_world);
}

void DefaultGameModule::OnTick(IGameHost& host, float deltaSeconds)
{
	if (const InputState* input = host.GetInput())
	{
		const bool useMouse = !host.IsMouseCaptured();
		const bool useKeyboard = !host.IsKeyboardCaptured();
		m_cameraController.Update(m_camera, *input, deltaSeconds, useMouse, useKeyboard);
		host.SetRelativeMouse(useMouse && input->IsDown(Key::MouseRight));
	}
}

void DefaultGameModule::OnFixedTick(IGameHost& /*host*/, float fixedDeltaSeconds)
{
	if (auto step = m_drop.Step(m_world, fixedDeltaSeconds); !step)
	{
		LogResult(step, LogCategory::Physics);
	}
}

void DefaultGameModule::OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects)
{
	m_drop.WriteRenderTransforms(m_world, host.GetInterpolationAlpha());
	ExtractWorld(m_world, outObjects);
}

void DefaultGameModule::OnExtractView(IGameHost& host, ExtractedView& outView)
{
	const auto [viewportWidth, viewportHeight] = host.GetViewportSize();
	BuildExtractedView(m_camera, viewportWidth, viewportHeight, outView);
}

void DefaultGameModule::OnExtractLighting(IGameHost& /*host*/, ExtractedLighting& outLighting)
{
	BuildExtractedLighting(m_lighting, outLighting);
}

void DefaultGameModule::OnShutdown(IGameHost& /*host*/)
{
}

IGameModule* CreateGameModule()
{
	return new DefaultGameModule();
}
