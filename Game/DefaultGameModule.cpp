#include "Game/DefaultGameModule.h"

#include "Game/Camera/CameraExtract.h"
#include "Game/IGameHost.h"

#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"

Result<void> DefaultGameModule::OnInit(IGameHost& /*host*/)
{
	m_cameraController.Reset(m_camera);
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

	RenderResourceServices* resources = renderServices->renderer->GetResourceServices();
	if (resources == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"DefaultGameModule requires renderer resource services");
	}

	const std::filesystem::path shaderRoot = host.GetShaderRoot();
	const std::filesystem::path assetsRoot = resources->GetTextureServices().GetAssetsRoot();
	return m_fbxScene.EnsureInitialized(
		*renderServices->renderer,
		frameContext,
		commandList,
		shaderRoot,
		assetsRoot,
		kDefaultModelPath);
}

void DefaultGameModule::OnTick(IGameHost& host, float deltaSeconds)
{
	m_cameraController.Update(m_camera, host.GetInput(), deltaSeconds);
}

void DefaultGameModule::OnExtract(IGameHost& /*host*/, std::vector<ExtractedObject>& outObjects)
{
	m_fbxScene.FillExtractedObjects(outObjects);
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
