#include "Runtime/Subsystems/DisplaySubsystem.h"

#include "Engine/Application/Services/RHIServices.h"
#include "Engine/RHI/Interface/RHIFence.h"
#include "Engine/RHI/Interface/RHICommandQueue.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Graphics/DisplayConfig.h"
#include "Engine/Graphics/DisplayContext.h"

namespace
{
constexpr const char* kDependencies[] = { "Window", "RHI" };
} // namespace

struct DisplaySubsystemImpl
{
	std::unique_ptr<DisplayContext> display{};
};

DisplaySubsystem::DisplaySubsystem(EngineLoopConfig config)
	: m_config(std::move(config))
	, m_impl(std::make_unique<DisplaySubsystemImpl>())
{
}

DisplaySubsystem::~DisplaySubsystem() = default;

std::unique_ptr<ISubsystem> CreateDisplaySubsystem(const EngineLoopConfig& config)
{
	return std::make_unique<DisplaySubsystem>(config);
}

std::span<const char* const> DisplaySubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> DisplaySubsystem::Initialize(SubsystemContext& ctx)
{
	auto* windowServices = ctx.GetService<WindowServices>();
	auto* rhiServices = ctx.GetService<RHIServices>();
	if (windowServices == nullptr || rhiServices == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"DisplaySubsystem requires WindowServices and RHIServices");
	}

	if (windowServices->hwnd == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"DisplaySubsystem requires a valid window handle");
	}

	if (windowServices->clientWidth == 0 || windowServices->clientHeight == 0)
	{
		return FailRuntime(LogCategory::Core, ErrorCode::InvalidArgument,
			"DisplaySubsystem requires non-zero client dimensions");
	}

	DisplayConfig displayConfig{};
	displayConfig.width = windowServices->clientWidth;
	displayConfig.height = windowServices->clientHeight;
	displayConfig.bufferCount = EngineConstants::kSwapChainBufferCount;

	auto displayResult = DisplayContext::Create(
		rhiServices->device,
		rhiServices->graphicsQueue,
		windowServices->hwnd,
		displayConfig);
	if (!displayResult)
	{
		return MakeFail(displayResult.error.code, displayResult.error.message);
	}

	m_impl->display = std::move(displayResult.value);
	m_services.display = m_impl->display.get();
	ctx.RegisterService(&m_services);
	return MakeOk();
}

Result<void> DisplaySubsystem::OnResize(SubsystemContext& /*ctx*/, uint32_t width, uint32_t height)
{
	if (m_impl->display == nullptr || width == 0 || height == 0)
	{
		return MakeOk();
	}

	return m_impl->display->Resize(width, height);
}

void DisplaySubsystem::Shutdown(SubsystemContext& ctx)
{
	auto* rhiServices = ctx.GetService<RHIServices>();
	if (rhiServices != nullptr)
	{
		// D3D12 swap-chain back buffers are GPU-referenced; ensure all in-flight
		// command queue work is finished before destroying them.
		if (rhiServices->frameFence != nullptr)
		{
			for (FrameContext& frameContext : rhiServices->frameContexts)
			{
				if (frameContext.fenceValue != 0)
				{
					rhiServices->frameFence->WaitCPU(frameContext.fenceValue);
					frameContext.fenceValue = 0;
				}
			}
		}

		if (rhiServices->graphicsQueue != nullptr)
		{
			rhiServices->graphicsQueue->WaitForIdle();
		}
	}

	ctx.UnregisterService(&m_services);
	m_impl->display.reset();
	m_services = {};
}
