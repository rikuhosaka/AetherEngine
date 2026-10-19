#include "Engine/Scene/GameScene.h"

#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Camera/CameraExtract.h"
#include "Engine/Platform/InputState.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderViewTypes.h"
#include "Engine/Scene/EditorScene.h"
#include "Engine/Scene/SceneDocument.h"

#include <utility>

namespace
{
constexpr DirectX::XMFLOAT3 kPlayCameraFallbackPosition{ 0.0f, 1.6f, -4.0f };
constexpr DirectX::XMFLOAT3 kPlayCameraFallbackForward{ 0.0f, 0.0f, 1.0f };
} // namespace

GameScene::GameScene(World world, EntityPhysics physics, SceneLightingSettings lighting)
	: m_world(std::move(world))
	, m_physics(std::move(physics))
	, m_lighting(lighting)
{
}

Result<GameScene> GameScene::Begin(
	const EditorScene& editor,
	const EditorCameraServices* camera)
{
	if (!editor.HasDocument())
	{
		return FailInternal<GameScene>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"GameScene::Begin requires a scene document");
	}

	World world = editor.GetWorld().Clone();
	Result<EntityPhysics> physics = BindScenePhysics(world, editor.GetDocument(), editor.GetBindings());
	if (!physics)
	{
		return MakeFail<GameScene>(physics.error.code, physics.error.message);
	}

	GameScene scene(std::move(world), std::move(physics.value), editor.GetLighting());
	scene.ResetCamera(camera);
	return MakeOk(std::move(scene));
}

void GameScene::Tick(SubsystemContext& ctx, float deltaSeconds)
{
	UpdateCamera(ctx, deltaSeconds);
}

Result<void> GameScene::Step(float fixedDeltaSeconds)
{
	return m_physics.Step(m_world, fixedDeltaSeconds);
}

void GameScene::WriteRenderTransforms(float alpha)
{
	m_physics.WriteTransforms(m_world, alpha);
}

void GameScene::ExtractView(uint32_t viewportWidth, uint32_t viewportHeight, ExtractedView& outView) const
{
	BuildExtractedView(m_camera, viewportWidth, viewportHeight, outView);
}

void GameScene::ExtractLighting(ExtractedLighting& outLighting) const
{
	BuildExtractedLighting(m_lighting, outLighting);
}

void GameScene::ResetCamera(const EditorCameraServices* camera)
{
	if (camera != nullptr && camera->valid)
	{
		m_cameraController.Reset(m_camera, camera->position, camera->forward);
		return;
	}

	m_cameraController.Reset(m_camera, kPlayCameraFallbackPosition, kPlayCameraFallbackForward);
}

void GameScene::UpdateCamera(SubsystemContext& ctx, float deltaSeconds)
{
	InputServices* inputServices = ctx.GetService<InputServices>();
	if (inputServices == nullptr || inputServices->state == nullptr)
	{
		return;
	}

	const bool useMouse = !inputServices->mouseCaptured;
	const bool useKeyboard = !inputServices->keyboardCaptured;
	m_cameraController.Update(m_camera, *inputServices->state, deltaSeconds, useMouse, useKeyboard);
	inputServices->relativeMouse = useMouse && inputServices->state->IsDown(Key::MouseRight);
}
