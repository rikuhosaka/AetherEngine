#include "Engine/World/GameScene.h"

#include "Engine/Camera/CameraExtract.h"
#include "Engine/Platform/InputState.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderViewTypes.h"
#include "Engine/World/SceneLighting.h"

#include <utility>

namespace
{
constexpr DirectX::XMFLOAT3 kPlayCameraFallbackPosition{ 0.0f, 1.6f, -4.0f };
constexpr DirectX::XMFLOAT3 kPlayCameraFallbackForward{ 0.0f, 0.0f, 1.0f };
} // namespace

GameScene::GameScene(World world, EntityPhysics physics)
	: m_world(std::move(world))
	, m_physics(std::move(physics))
{
}

Result<GameScene> GameScene::Begin(
	const World& editor,
	const DirectX::XMFLOAT3* cameraPosition,
	const DirectX::XMFLOAT3* cameraForward)
{
	World world = editor.Clone();
	Result<EntityPhysics> physics = BindRigidBodies(world);
	if (!physics)
	{
		return MakeFail<GameScene>(physics.error.code, physics.error.message);
	}

	GameScene scene(std::move(world), std::move(physics.value));
	scene.ResetCamera(cameraPosition, cameraForward);
	return MakeOk(std::move(scene));
}

bool GameScene::Tick(
	const InputState* input,
	float deltaSeconds,
	bool mouseCaptured,
	bool keyboardCaptured)
{
	return UpdateCamera(input, deltaSeconds, mouseCaptured, keyboardCaptured);
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
	BuildExtractedLighting(m_world, outLighting);
}

void GameScene::ResetCamera(const DirectX::XMFLOAT3* cameraPosition, const DirectX::XMFLOAT3* cameraForward)
{
	if (cameraPosition != nullptr && cameraForward != nullptr)
	{
		m_cameraController.Reset(m_camera, *cameraPosition, *cameraForward);
		return;
	}

	m_cameraController.Reset(m_camera, kPlayCameraFallbackPosition, kPlayCameraFallbackForward);
}

bool GameScene::UpdateCamera(
	const InputState* input,
	float deltaSeconds,
	bool mouseCaptured,
	bool keyboardCaptured)
{
	if (input == nullptr)
	{
		return false;
	}

	const bool useMouse = !mouseCaptured;
	const bool useKeyboard = !keyboardCaptured;
	m_cameraController.Update(m_camera, *input, deltaSeconds, useMouse, useKeyboard);
	return useMouse && input->IsDown(Key::MouseRight);
}
