#include "Runtime/Engine/World/Component/PlayerController.h"

#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Engine/World/Component/CameraSystemComponent.h"
#include "Runtime/Core/Platform/InputState.h"
#include "Runtime/Engine/Engine/Subsystem/InputSubsystem.h"
#include "Runtime/Engine/World/World.h"

void PlayerController::Tick(World& world, float deltaSeconds)
{
	InputSubsystem* input = SubsystemRegistry::FindCurrent<InputSubsystem>();
	if (input == nullptr)
	{
		return;
	}

	InputServices& services = input->GetInput();
	// mouseCaptured / keyboardCaptured は UI が入力を取っているという意味。取られている軸はカメラへ渡さない。
	const bool useMouse = !services.mouseCaptured;
	const bool useKeyboard = !services.keyboardCaptured;
	const InputState* state = services.state;
	const EntityId owner = GetOwner();
	if (CameraSystemComponent* camera = world.Get<CameraSystemComponent>(owner))
	{
		camera->ApplyControl(world, state, deltaSeconds, useMouse, useKeyboard);
	}

	// 右ボタン押下かつマウスが空いているときだけ、相対マウス（カーソルを隠して視点を回す）を要求する。
	services.relativeMouse = useMouse && state != nullptr && state->IsDown(Key::MouseRight);
}
