#include "Runtime/Subsystems/RenderSubsystem.h"

#include "Engine/Application/Services/DisplayServices.h"
#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Graphics/DisplayContext.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Core/RendererConfig.h"
#include "Engine/Renderer/Core/ShaderSourcePolicy.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHIDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Common/RHIScopedDebugEvent.h"

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
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSubsystem requires RHIServices");
	}

	if (m_config.shaderRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSubsystem requires a valid shader root path");
	}

	if (m_config.assetsRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSubsystem requires a valid assets root path");
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
	rendererConfig.compiledShaderRoot = m_config.compiledShaderRoot;
	rendererConfig.assetsRoot = m_config.assetsRoot;
	rendererConfig.visualizeSceneDepth = m_config.visualizeSceneDepth;
#ifdef AETHER_PRECOMPILED_SHADERS_ONLY
	rendererConfig.shaderSourcePolicy = ShaderSourcePolicy::PrecompiledOnly;
#elif defined(NDEBUG)
	rendererConfig.shaderSourcePolicy = ShaderSourcePolicy::PreferPrecompiled;
#else
	rendererConfig.shaderSourcePolicy = ShaderSourcePolicy::PreferSource;
#endif

	if (auto initResult = m_impl->renderer->Initialize(
			rhiServices->device,
			m_impl->descriptorAllocator.get(),
			rendererConfig);
		!initResult)
	{
		return initResult;
	}

	m_services.resources = m_impl->renderer->GetResourceServices();
	m_services.shaderRoot = m_config.shaderRoot;
	ctx.RegisterService(&m_services);
	return MakeOk();
}

Result<void> RenderSubsystem::DrawFrame(
	SubsystemContext& ctx,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const ExtractedView& view,
	const ExtractedLighting& lighting,
	std::span<const ExtractedObject> objects)
{
	auto* displayServices = ctx.GetService<DisplayServices>();
	if (displayServices == nullptr || displayServices->display == nullptr || m_impl->renderer == nullptr
		|| commandList == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSubsystem::DrawFrame missing required services");
	}

	m_impl->renderer->SetFrameContext(&frameContext);
	m_impl->renderer->BeginFrame(frameContext.frameIndex);
	m_impl->renderer->ExtractView(view);
	m_impl->renderer->ExtractLighting(lighting);
	m_impl->renderer->ExtractScene(objects);

	{
		const RHIScopedDebugEvent frameEvent(commandList, "Frame");
		m_impl->renderer->BuildScene(commandList);
		m_impl->renderer->RenderShadow(commandList);
		displayServices->display->BeginMainRenderPass(frameContext, commandList);
		m_impl->renderer->Render(commandList);
		displayServices->display->PrepareSceneDepthForRead(frameContext, commandList);
		m_impl->renderer->DrawSceneDepthDebug(commandList);
		displayServices->display->EndMainRenderPass(frameContext, commandList);
	}

	return MakeOk();
}

void RenderSubsystem::Shutdown(SubsystemContext& ctx)
{
	ctx.UnregisterService(&m_services);
	m_impl->renderer.reset();
	m_impl->descriptorAllocator.reset();
	m_services = {};
}
