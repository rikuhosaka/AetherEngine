#pragma once

#include "Game/IGameModule.h"
#include "Game/Camera/CameraState.h"
#include "Game/Camera/FreeFlyCameraController.h"
#include "Game/Lighting/DefaultLightingSetup.h"
#include "Game/PhysicsDropScene.h"
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
	void OnFixedTick(IGameHost& host, float fixedDeltaSeconds) override;
	void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) override;
	void OnExtractView(IGameHost& host, ExtractedView& outView) override;
	void OnExtractLighting(IGameHost& host, ExtractedLighting& outLighting) override;
	void OnShutdown(IGameHost& host) override;
	[[nodiscard]] World* GetWorld() override { return &m_world; }

private:
	static constexpr DirectX::XMFLOAT3 kCameraStartPosition{ 0.0f, 1.6f, -4.0f };
	static constexpr DirectX::XMFLOAT3 kCameraStartForward{ 0.0f, 0.0f, 1.0f };

	CameraState m_camera{};
	FreeFlyCameraController m_cameraController{};
	SceneLightingSettings m_lighting{};
	PhysicsDropScene m_drop{};
	World m_world{};
};
