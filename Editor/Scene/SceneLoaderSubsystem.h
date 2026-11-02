#pragma once

#include "Editor/Camera/EditorCamera.h"

#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

// 責任: 編集中の視点を更新し、他のサブシステムから読めるようにする。
// 役割: Editor モードの間だけ自由飛行し、その位置と前方をプレイ開始時のカメラ初期値にする。登録名は "Editor"。
// 所有: SubsystemRegistry が unique_ptr で持つ。編集カメラと公開用サービスをメンバとして所有する。
//       サービスは SubsystemContext へ非所有ポインタで登録する。プレイ中の視点は CameraSubsystem が持つ。
class SceneLoaderSubsystem final : public ISubsystem
{
public:
	SceneLoaderSubsystem() = default;

	[[nodiscard]] const char* GetName() const override { return "Editor"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	// このサブシステムが所有する編集カメラ。
	EditorCamera m_camera{};
	// このサブシステムが所有する公開データ。Context に載るのはこのアドレスだけ。
	EditorCameraServices m_cameraServices{};
};

std::unique_ptr<ISubsystem> CreateEditorSubsystem();
