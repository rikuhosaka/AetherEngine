#include "Game/DefaultGameModule.h"

#include "Game/Camera/CameraExtract.h"
#include "Game/IGameHost.h"

#include "Engine/Platform/InputState.h"

#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/World/WorldExtract.h"

Result<void> DefaultGameModule::OnInit(IGameHost& /*host*/)
{
	m_cameraController.Reset(m_camera, kCameraStartPosition, kCameraStartForward);
	return MakeOk();
}

Result<void> DefaultGameModule::OnPrepareRender(
	IGameHost& host,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	RenderServices* renderServices = host.GetRenderServices();
	if (renderServices == nullptr || renderServices->renderer == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"DefaultGameModule requires RenderServices");
	}

	if (auto initResult = m_room.EnsureInitialized(
			*renderServices->renderer,
			frameContext,
			commandList,
			host.GetShaderRoot(),
			kRoomDimensions);
		!initResult)
	{
		return initResult;
	}

	RenderResourceServices* resources = renderServices->renderer->GetResourceServices();
	if (resources == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"DefaultGameModule requires renderer resource services");
	}

	if (auto fbxResult = m_fbx.EnsureInitialized(
			*renderServices->renderer,
			frameContext,
			commandList,
			host.GetShaderRoot(),
			resources->GetTextureServices().GetAssetsRoot(),
			kFbxModelPath);
		!fbxResult)
	{
		return fbxResult;
	}

	if (auto roomSpawn = m_room.SpawnInto(m_world); !roomSpawn)
	{
		return roomSpawn;
	}

	return m_fbx.SpawnInto(m_world);
}

void DefaultGameModule::OnTick(IGameHost& host, float deltaSeconds)
{
	if (const InputState* input = host.GetInput())
	{
		m_cameraController.Update(m_camera, *input, deltaSeconds);
		host.SetRelativeMouse(input->IsDown(Key::MouseRight));
	}
}

void DefaultGameModule::OnExtract(IGameHost& /*host*/, std::vector<ExtractedObject>& outObjects)
{
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
