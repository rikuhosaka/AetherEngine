#pragma once

#include "Runtime/Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"
#include "Runtime/Renderer/UiFrameCapture.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/World/WorldDocument.h"

#include <cstdint>
#include <memory>
#include <vector>

class SceneDocumentSubsystem;
class World;

// RenderSystem の窓口。RenderSystem は実装ファイルに閉じ、UI のキャプチャ結果だけ公開する。
class EngineRender
{
public:
	EngineRender();
	~EngineRender();

	EngineRender(const EngineRender&) = delete;
	EngineRender& operator=(const EngineRender&) = delete;

	Result<void> Initialize(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		const EngineLoopConfig& config);
	void Shutdown();
	Result<void> Resize(uint32_t width, uint32_t height);

	Result<void> Render(
		uint32_t viewportWidth,
		uint32_t viewportHeight,
		const RenderFrame& frame);

	[[nodiscard]] bool BeginUiFrame();
	[[nodiscard]] UiFrameCapture EndUiFrame();

	// 記録を開始し、そのフレーム番号を返す。
	Result<uint32_t> BeginRecording();
	Result<void> SubmitAndWait();
	void AbortRecording();

	// 直前の BeginRecording で開いたフレームへ、保留中の文書を GPU へ上げてエンティティを生成する。
	// 失敗時の巻き戻しは InstantiateScene が行う。
	Result<std::vector<WorldDocumentBinding>> UploadPendingScene(
		SceneDocumentSubsystem& scene,
		World& world);

	[[nodiscard]] uint32_t GetFrameSlot() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl{};
};
