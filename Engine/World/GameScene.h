#pragma once

#include "Engine/Camera/CameraState.h"
#include "Engine/Camera/FreeFlyCameraController.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/World/EntityPhysics.h"
#include "Engine/World/World.h"

#include <cstdint>
#include <DirectXMath.h>

class InputState;
struct ExtractedLighting;
struct ExtractedView;

// Play clone of an editor world. Created when play starts and destroyed when it stops.
class GameScene
{
public:
	GameScene() = default;

	// A null position or forward uses the play fallback pose.
	[[nodiscard]] static Result<GameScene> Begin(
		const World& editor,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward);

	[[nodiscard]] World& GetWorld() { return m_world; }
	[[nodiscard]] const World& GetWorld() const { return m_world; }

	// Returns whether the play camera wants relative mouse movement.
	[[nodiscard]] bool Tick(
		const InputState* input,
		float deltaSeconds,
		bool mouseCaptured,
		bool keyboardCaptured);
	[[nodiscard]] Result<void> Step(float fixedDeltaSeconds);
	void WriteRenderTransforms(float alpha);
	void ExtractView(uint32_t viewportWidth, uint32_t viewportHeight, ExtractedView& outView) const;
	void ExtractLighting(ExtractedLighting& outLighting) const;

private:
	GameScene(World world, EntityPhysics physics);

	void ResetCamera(const DirectX::XMFLOAT3* cameraPosition, const DirectX::XMFLOAT3* cameraForward);
	[[nodiscard]] bool UpdateCamera(
		const InputState* input,
		float deltaSeconds,
		bool mouseCaptured,
		bool keyboardCaptured);

	World m_world{};
	EntityPhysics m_physics{};
	CameraState m_camera{};
	FreeFlyCameraController m_cameraController{};
};
