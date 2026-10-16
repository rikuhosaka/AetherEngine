#pragma once

#include "Game/IGameModule.h"
#include "Game/Camera/CameraState.h"
#include "Game/Camera/FreeFlyCameraController.h"
#include "Game/Lighting/DefaultLightingSetup.h"
#include "Game/PhysicsDropScene.h"
#include "Engine/World/World.h"

#include <optional>
#include <string>

class DefaultGameModule final : public IGameModule
{
public:
	Result<void> OnInit(IGameHost& host) override;
	Result<void> OnLoadContent(
		IGameHost& host,
		FrameContext& frameContext,
		RHICommandList* commandList) override;
	void OnTick(IGameHost& host, float deltaSeconds) override;
	Result<void> OnBeginPlay(IGameHost& host) override;
	void OnEndPlay(IGameHost& host) override;
	void OnFixedTick(IGameHost& host, float fixedDeltaSeconds) override;
	void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) override;
	void OnExtractView(IGameHost& host, ExtractedView& outView) override;
	void OnExtractLighting(IGameHost& host, ExtractedLighting& outLighting) override;
	void OnShutdown(IGameHost& host) override;
	[[nodiscard]] World* GetWorld() override { return ActiveWorld(); }

	// Captures m_world. Play mode keeps using the cloned world and does not change this result.
	[[nodiscard]] Result<std::string> CaptureEditorScene() const;

private:
	[[nodiscard]] World* ActiveWorld()
	{
		return m_playWorld.has_value() ? &m_playWorld.value() : &m_world;
	}
	[[nodiscard]] CameraState& ActiveCamera()
	{
		return m_playWorld.has_value() ? m_playCamera : m_camera;
	}
	[[nodiscard]] FreeFlyCameraController& ActiveCameraController()
	{
		return m_playWorld.has_value() ? m_playCameraController : m_cameraController;
	}
	static constexpr DirectX::XMFLOAT3 kCameraStartPosition{ 0.0f, 1.6f, -4.0f };
	static constexpr DirectX::XMFLOAT3 kCameraStartForward{ 0.0f, 0.0f, 1.0f };

	CameraState m_camera{};
	FreeFlyCameraController m_cameraController{};
	CameraState m_playCamera{};
	FreeFlyCameraController m_playCameraController{};
	SceneLightingSettings m_lighting{};
	PhysicsDropScene m_drop{};
	World m_world{};
	std::optional<World> m_playWorld{};
};
