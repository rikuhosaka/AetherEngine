#pragma once

#include "Runtime/Renderer/RHIServices.h"
#include "Runtime/Renderer/RenderServices.h"
#include "Runtime/Renderer/RenderSystemConfig.h"
#include "Runtime/Renderer/UiFrameCapture.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"
#include "Runtime/Renderer/Scene/SceneAssetUpload.h"

#include <Windows.h>

#include <cstdint>
#include <memory>

class FrameContext;
class RHIImGui;
class RenderSystemImpl;

class RenderSystem
{
public:
	RenderSystem();
	~RenderSystem();

	Result<void> Initialize(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		const RenderSystemConfig& config);
	void Shutdown();
	Result<void> Resize(uint32_t width, uint32_t height);

	Result<void> Render(
		uint32_t viewportWidth,
		uint32_t viewportHeight,
		const RenderFrame& frame);

	// UI サブシステムの Tick より前に呼ぶ。ImGui が無ければ false。
	[[nodiscard]] bool BeginUiFrame();
	// ウィジェットを描いた後、UiPresent より前に呼ぶ。
	[[nodiscard]] UiFrameCapture EndUiFrame();

	[[nodiscard]] Result<UploadedSceneAssets> UploadScene(
		const SceneGpuUpload& upload,
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
	RenderSystemConfig m_config{};
	bool m_recording = false;
};
