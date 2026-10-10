#pragma once

#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/IEngineLoopRender.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

#include <memory>

class Renderer;
class RHIDescriptorAllocator;

class RenderSubsystemImpl;

class RenderSubsystem final : public ISubsystem, public IEngineLoopRender
{
public:
	explicit RenderSubsystem(EngineLoopConfig config);
	~RenderSubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "Render"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Shutdown(SubsystemContext& ctx) override;

	Result<void> DrawFrame(
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const ExtractedView& view,
		const ExtractedLighting& lighting,
		std::span<const ExtractedObject> objects) override;

	[[nodiscard]] const RenderServices& GetServices() const noexcept { return m_services; }

private:
	EngineLoopConfig m_config{};
	std::unique_ptr<RenderSubsystemImpl> m_impl{};
	RenderServices m_services{};
};

std::unique_ptr<ISubsystem> CreateRenderSubsystem(const EngineLoopConfig& config);
