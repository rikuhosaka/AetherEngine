#include "Runtime/Subsystems/RenderSubsystem.h"

#include "Engine/Application/Services/DisplayServices.h"
#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/ISceneExtractor.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Graphics/DisplayContext.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Core/RendererConfig.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHICommandQueue.h"
#include "Engine/RHI/Interface/RHIDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIFence.h"

namespace
{
constexpr uint32_t kPersistentDescriptorCount = 4096;

constexpr const char* kDependencies[] = { "Display", "RHI" };
} // namespace

struct RenderSubsystemImpl
{
	std::unique_ptr<Renderer> renderer{};
	std::unique_ptr<RHIDescriptorAllocator> descriptorAllocator{};
};

RenderSubsystem::RenderSubsystem(EngineLoopConfig config)
	: m_config(std::move(config))
	, m_impl(std::make_unique<RenderSubsystemImpl>())
{
}

RenderSubsystem::~RenderSubsystem() = default;

std::unique_ptr<ISubsystem> CreateRenderSubsystem(const EngineLoopConfig& config)
{
	return std::make_unique<RenderSubsystem>(config);
}

std::span<const char* const> RenderSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> RenderSubsystem::Initialize(SubsystemContext& ctx)
{
	auto* rhiServices = ctx.GetService<RHIServices>();
	if (rhiServices == nullptr || rhiServices->device == nullptr)
	{
		return MakeFail(ErrorCode::InvalidArgument, "RenderSubsystem requires RHIServices");
	}

	if (m_config.shaderRoot.empty())
	{
		return MakeFail(ErrorCode::InvalidArgument, "RenderSubsystem requires a valid shader root path");
	}

	auto descriptorAllocatorResult =
		rhiServices->device->CreateDescriptorAllocator(kPersistentDescriptorCount);
	if (!descriptorAllocatorResult)
	{
		return MakeFail(
			descriptorAllocatorResult.error.code,
			descriptorAllocatorResult.error.message);
	}
	m_impl->descriptorAllocator = std::move(descriptorAllocatorResult.value);

	m_impl->renderer = std::make_unique<Renderer>();
	RendererConfig rendererConfig{};
	rendererConfig.shaderRoot = m_config.shaderRoot;

	if (auto initResult = m_impl->renderer->Initialize(
			rhiServices->device,
			m_impl->descriptorAllocator.get(),
			rendererConfig);
		!initResult)
	{
		return initResult;
	}

	m_services.renderer = m_impl->renderer.get();
	ctx.RegisterService(&m_services);
	return MakeOk();
}

Result<void> RenderSubsystem::RenderFrame(SubsystemContext& ctx)
{
	auto* rhiServices = ctx.GetService<RHIServices>();
	auto* displayServices = ctx.GetService<DisplayServices>();
	auto* windowServices = ctx.GetService<WindowServices>();
	if (rhiServices == nullptr || displayServices == nullptr || displayServices->display == nullptr
		|| m_impl->renderer == nullptr)
	{
		return MakeFail(ErrorCode::InvalidArgument, "RenderSubsystem::RenderFrame missing required services");
	}

	if (windowServices != nullptr
		&& (windowServices->isMinimized || windowServices->clientWidth == 0 || windowServices->clientHeight == 0))
	{
		return MakeOk();
	}

	const uint32_t slot = rhiServices->currentFrameSlot;
	FrameContext& frameContext = rhiServices->frameContexts[slot];
	frameContext.frameIndex = slot;

	if (frameContext.fenceValue != 0)
	{
		rhiServices->frameFence->WaitCPU(frameContext.fenceValue);
	}

	ctx.SetFrameSlot(slot);
	m_impl->renderer->SetFrameContext(&frameContext);

	displayServices->display->BeginFrame(frameContext);
	m_impl->renderer->BeginFrame(slot);

	std::vector<ExtractedObject> extractedObjects;
	if (auto* registry = ctx.GetService<SubsystemRegistry>())
	{
		if (ISceneExtractor* sceneExtractor = registry->GetSceneExtractor())
		{
			sceneExtractor->Extract(extractedObjects);
		}
	}
	m_impl->renderer->ExtractScene(extractedObjects);

	RHICommandList* commandList = frameContext.graphicsCommandList;
	if (commandList == nullptr)
	{
		return MakeFail(ErrorCode::InvalidArgument, "RenderSubsystem::RenderFrame missing command list");
	}

	m_impl->renderer->BuildScene(commandList);
	displayServices->display->BeginMainRenderPass(frameContext, commandList);
	m_impl->renderer->Render(commandList);
	displayServices->display->EndMainRenderPass(frameContext, commandList);

	commandList->Close();
	rhiServices->graphicsQueue->ExecuteCommandLists({ commandList });
	frameContext.fenceValue = rhiServices->graphicsQueue->Signal(rhiServices->frameFence);

	displayServices->display->Present(m_config.vsync ? 1u : 0u, 0u);

	rhiServices->currentFrameSlot = (slot + 1) % RHIServices::kFrameCount;
	ctx.SetFrameSlot(rhiServices->currentFrameSlot);
	return MakeOk();
}

void RenderSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	m_impl->renderer.reset();
	m_impl->descriptorAllocator.reset();
	m_services = {};
}
