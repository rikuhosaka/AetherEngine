#pragma once

#include "Game/IGameModule.h"
#include "Game/Camera/CameraState.h"
#include "Game/Camera/FreeFlyCameraController.h"
#include "Game/FbxSceneAssets.h"
#include "Game/RoomSceneAssets.h"
#include "Game/Lighting/DefaultLightingSetup.h"
#include "Engine/World/World.h"

class DefaultGameModule final : public IGameModule
{
public:
	Result<void> OnInit(IGameHost& host) override;
	Result<void> OnLoadContent(
		IGameHost& host,
		FrameContext& frameContext,
		RHICommandList* commandList) override;
	void OnTick(IGameHost& host, float deltaSeconds) override;
	void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) override;
	void OnExtractView(IGameHost& host, ExtractedView& outView) override;
	void OnExtractLighting(IGameHost& host, ExtractedLighting& outLighting) override;
	void OnShutdown(IGameHost& host) override;

private:
	static constexpr RoomDimensions kRoomDimensions{ 10.0f, 10.0f, 3.0f };
	static constexpr DirectX::XMFLOAT3 kCameraStartPosition{ 0.0f, 1.6f, -4.0f };
	static constexpr DirectX::XMFLOAT3 kCameraStartForward{ 0.0f, 0.0f, 1.0f };
	static constexpr const char* kFbxModelPath = "Models/trangle.fbx";

	CameraState m_camera{};
	FreeFlyCameraController m_cameraController{};
	SceneLightingSettings m_lighting{};
	RoomSceneAssets m_room{};
	FbxSceneAssets m_fbx{};
	World m_world{};
};
