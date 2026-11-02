#include "Editor/Scene/SceneLoaderSubsystem.h"

#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Platform/InputState.h"

namespace
{
constexpr const char* kDependencies[] = { "Window", "Input", "Engine", "SceneDocument" };
} // namespace

std::unique_ptr<ISubsystem> CreateEditorSubsystem()
{
	return std::make_unique<SceneLoaderSubsystem>();
}

std::span<const char* const> SceneLoaderSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> SceneLoaderSubsystem::Initialize(SubsystemContext& ctx)
{
	const WindowServices* windowServices = ctx.GetService<WindowServices>();
	if (windowServices == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SceneLoaderSubsystem requires WindowServices");
	}

	m_camera.Reset();
	m_camera.Publish(windowServices->clientWidth, windowServices->clientHeight, m_cameraServices);

	ctx.RegisterService(&m_cameraServices);
	return MakeOk();
}

void SceneLoaderSubsystem::Tick(SubsystemContext& ctx, float deltaSeconds)
{
	const SessionServices* session = ctx.GetService<SessionServices>();
	// セッションサービスが無い起動直後も編集カメラとして扱う。
	// 相対マウスのフラグは編集中だけここが書く。再生中は Engine::Tick がプレイカメラの結果で上書きする。
	const bool editorMode = session == nullptr || session->mode == SessionMode::Editor;
	if (editorMode)
	{
		InputServices* inputServices = ctx.GetService<InputServices>();
		const InputState* input = inputServices != nullptr ? inputServices->state : nullptr;
		const bool useMouse = inputServices == nullptr || !inputServices->mouseCaptured;
		const bool useKeyboard = inputServices == nullptr || !inputServices->keyboardCaptured;
		m_camera.Update(input, deltaSeconds, useMouse, useKeyboard);
		if (inputServices != nullptr && input != nullptr)
		{
			inputServices->relativeMouse = useMouse && input->IsDown(Key::MouseRight) && !inputServices->mouseCaptured;
		}

		const WindowServices* windowServices = ctx.GetService<WindowServices>();
		const uint32_t width = windowServices != nullptr ? windowServices->clientWidth : 0;
		const uint32_t height = windowServices != nullptr ? windowServices->clientHeight : 0;
		m_camera.Publish(width, height, m_cameraServices);
	}
}

void SceneLoaderSubsystem::Shutdown(SubsystemContext& ctx)
{
	ctx.UnregisterService(&m_cameraServices);
	m_cameraServices = {};
}
