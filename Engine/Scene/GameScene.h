#pragma once

#include "Engine/Camera/CameraState.h"
#include "Engine/Camera/FreeFlyCameraController.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Scene/SceneLighting.h"
#include "Engine/World/EntityPhysics.h"
#include "Engine/World/World.h"

#include <cstdint>

class EditorScene;
class PhysicsDropScene;
class SubsystemContext;
struct EditorCameraServices;
struct ExtractedLighting;
struct ExtractedView;

// Play clone of an EditorScene. Created when play starts and destroyed when it stops.
class GameScene
{
public:
	GameScene() = default;

	[[nodiscard]] static Result<GameScene> Begin(
		const EditorScene& editor,
		PhysicsDropScene& content,
		const EditorCameraServices* camera);

	[[nodiscard]] World& GetWorld() { return m_world; }
	[[nodiscard]] const World& GetWorld() const { return m_world; }

	void Tick(SubsystemContext& ctx, float deltaSeconds);
	[[nodiscard]] Result<void> Step(float fixedDeltaSeconds);
	void WriteRenderTransforms(float alpha);
	void ExtractView(uint32_t viewportWidth, uint32_t viewportHeight, ExtractedView& outView) const;
	void ExtractLighting(ExtractedLighting& outLighting) const;

private:
	GameScene(World world, EntityPhysics physics, SceneLightingSettings lighting);

	void ResetCamera(const EditorCameraServices* camera);
	void UpdateCamera(SubsystemContext& ctx, float deltaSeconds);

	World m_world{};
	EntityPhysics m_physics{};
	CameraState m_camera{};
	FreeFlyCameraController m_cameraController{};
	SceneLightingSettings m_lighting{};
};
