#pragma once

#include "Engine/Renderer/RHIServices.h"
#include "Engine/Renderer/RenderServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Renderer/Scene/SceneAssetUpload.h"

#include <cstdint>
#include <memory>
#include <span>

class FrameContext;
class RHIImGui;
class RenderSystemImpl;
class SubsystemContext;

struct UiFrameCapture
{
	bool wantCaptureMouse = false;
	bool wantCaptureKeyboard = false;
	bool anyWindowHovered = false;
};

class RenderSystem
{
public:
	RenderSystem();
	~RenderSystem();

	Result<void> Initialize(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		const EngineLoopConfig& config,
		SubsystemContext& ctx);
	void Shutdown(SubsystemContext& ctx);
	Result<void> Resize(uint32_t width, uint32_t height);

	Result<void> Render(
		uint32_t viewportWidth,
		uint32_t viewportHeight,
		const ExtractedView& view,
		const ExtractedLighting& lighting,
		std::span<const ExtractedObject> objects);

	// UI サブシステムの Tick より前に呼ぶ。ImGui が無ければ false。
	[[nodiscard]] bool BeginUiFrame();
	// ウィジェットを描いた後、UiPresent より前に呼ぶ。
	[[nodiscard]] UiFrameCapture EndUiFrame();

	[[nodiscard]] Result<UploadedSceneAssets> UploadScene(
		const SceneDocument& document,
		const AssetCache& cache,
		FrameContext& frameContext);

	Result<FrameContext*> BeginRecording();
	Result<void> SubmitAndWait();
	void AbortRecording();

	[[nodiscard]] RHIServices& GetRHI() noexcept { return m_rhi; }
	[[nodiscard]] const RHIServices& GetRHI() const noexcept { return m_rhi; }
	[[nodiscard]] RenderServices& GetRender() noexcept { return m_render; }
	[[nodiscard]] const RenderServices& GetRender() const noexcept { return m_render; }

private:
	Result<void> CreateFrameResources();
	void EndBarrierFrame();
	void DiscardRecording(FrameContext& frameContext);

	std::unique_ptr<RenderSystemImpl> m_impl{};
	std::unique_ptr<RHIImGui> m_imgui{};
	RHIServices m_rhi{};
	RenderServices m_render{};
	EngineLoopConfig m_config{};
	bool m_recording = false;
};
