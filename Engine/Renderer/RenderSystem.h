#pragma once

#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

#include <cstdint>
#include <memory>
#include <span>

class FrameContext;
class RHIImGui;
class RenderSystemImpl;
class SubsystemContext;

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
		std::span<const ExtractedObject> objects,
		RHIImGui* imgui);

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
	RHIServices m_rhi{};
	RenderServices m_render{};
	EngineLoopConfig m_config{};
	bool m_recording = false;
};
