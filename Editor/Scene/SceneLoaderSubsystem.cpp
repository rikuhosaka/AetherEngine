#include "Editor/Scene/SceneLoaderSubsystem.h"

#include "Engine/SceneDocument/SceneCapture.h"
#include "Engine/SceneDocument/SceneDocument.h"

#include "Engine/Application/Services/EditorSceneServices.h"
#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Platform/InputState.h"

#include <fstream>

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
	ctx.RegisterService(&m_editorSave);
	return MakeOk();
}

void SceneLoaderSubsystem::Tick(SubsystemContext& ctx, float deltaSeconds)
{
	const SessionServices* session = ctx.GetService<SessionServices>();
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

	if (!m_editorSave.requested)
	{
		return;
	}

	m_editorSave.requested = false;
	if (!editorMode)
	{
		return;
	}

	if (auto saved = SaveEditorScene(ctx); !saved)
	{
		LogResult(saved, LogCategory::ECS);
	}
}

Result<void> SceneLoaderSubsystem::SaveEditorScene(SubsystemContext& ctx)
{
	const EditorSceneServices* scene = ctx.GetService<EditorSceneServices>();
	const WorldServices* world = ctx.GetService<WorldServices>();
	if (scene == nullptr || scene->document == nullptr || scene->path.empty()
		|| world == nullptr || world->world == nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Saving the editor scene requires a published editor scene");
	}

	const Result<SceneDocument> captured = CaptureScene(*world->world, *scene->document);
	if (!captured)
	{
		return MakeFail(captured.error.code, captured.error.message);
	}

	const Result<std::string> text = WriteSceneDocument(captured.value);
	if (!text)
	{
		return MakeFail(text.error.code, text.error.message);
	}

	std::ofstream file(scene->path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::FileNotFound,
			"Could not write the editor scene document");
	}

	file.write(text.value.data(), static_cast<std::streamsize>(text.value.size()));
	if (!file)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::RuntimeError,
			"Could not write the editor scene document");
	}

	LOG_INFO(LogCategory::ECS, "Saved the editor scene");
	return MakeOk();
}

void SceneLoaderSubsystem::Shutdown(SubsystemContext& ctx)
{
	ctx.UnregisterService(&m_editorSave);
	m_editorSave = {};
	ctx.UnregisterService(&m_cameraServices);
	m_cameraServices = {};
}
