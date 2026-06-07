#pragma once

#include "Game/IGameModule.h"
#include "Game/Camera/CameraState.h"
#include "Game/Camera/FreeFlyCameraController.h"
#include "Game/FbxSceneAssets.h"
#include "Game/Lighting/DefaultLightingSetup.h"

class DefaultGameModule final : public IGameModule
{
public:
	Result<void> OnInit(IGameHost& host) override;
	Result<void> OnPrepareRender(
		IGameHost& host,
		FrameContext& frameContext,
		RHICommandList* commandList) override;
	void OnTick(IGameHost& host, float deltaSeconds) override;
	void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) override;
	void OnExtractView(IGameHost& host, ExtractedView& outView) override;
	void OnExtractLighting(IGameHost& host, ExtractedLighting& outLighting) override;
	void OnShutdown(IGameHost& host) override;

private:
	static constexpr const char* kDefaultModelPath = "Models/Test/Triangle.fbx";

	CameraState m_camera{};
	FreeFlyCameraController m_cameraController{};
	SceneLightingSettings m_lighting{};
	FbxSceneAssets m_fbxScene{};
};
