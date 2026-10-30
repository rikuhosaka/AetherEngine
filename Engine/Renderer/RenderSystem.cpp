#include "Engine/Renderer/RenderSystem.h"

#include "Engine/Application/Services/GpuQueueRetirement.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Graphics/DisplayConfig.h"
#include "Engine/Graphics/DisplayContext.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Core/RendererConfig.h"
#include "Engine/Renderer/Core/ShaderSourcePolicy.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Common/RHIScopedDebugEvent.h"
#include "Engine/RHI/DX12/Debug/DX12BarrierDebug.h"
#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/Interface/RHICommandQueue.h"
#include "Engine/RHI/Interface/RHIDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIFence.h"
#include "Engine/RHI/Interface/RHIImGui.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <array>

namespace
{
constexpr size_t kUploadBufferCapacityBytes = 64 * 1024 * 1024;
constexpr uint32_t kTransientDescriptorCount = 2048;
constexpr uint32_t kPersistentDescriptorCount = 4096;
} // namespace

struct RenderSystemImpl
{
	std::unique_ptr<RHIDevice> device{};
	std::unique_ptr<RHIBarrierDebug> barrierDebug{};
	std::unique_ptr<RHICommandQueue> graphicsQueue{};
	std::unique_ptr<RHIFence> frameFence{};
	std::array<std::unique_ptr<RHICommandList>, RHIServices::kFrameCount> commandLists{};
	std::array<std::unique_ptr<RHIUploadBuffer>, RHIServices::kFrameCount> uploadBuffers{};
	std::array<std::unique_ptr<RHITransientDescriptorAllocator>, RHIServices::kFrameCount> transientAllocators{};
	std::unique_ptr<DisplayContext> display{};
	std::unique_ptr<RHIDescriptorAllocator> descriptorAllocator{};
	std::unique_ptr<Renderer> renderer{};
};

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() = default;

Result<void> RenderSystem::CreateFrameResources()
{
	for (uint32_t frameIndex = 0; frameIndex < RHIServices::kFrameCount; ++frameIndex)
	{
		auto commandListResult = m_impl->device->CreateCommandList();
		if (!commandListResult)
		{
			return MakeFail(commandListResult.error.code, commandListResult.error.message);
		}
		m_impl->commandLists[frameIndex] = std::move(commandListResult.value);

		auto uploadBufferResult = m_impl->device->CreateUploadBuffer(kUploadBufferCapacityBytes);
		if (!uploadBufferResult)
		{
			return MakeFail(uploadBufferResult.error.code, uploadBufferResult.error.message);
		}
		m_impl->uploadBuffers[frameIndex] = std::move(uploadBufferResult.value);

		auto transientAllocatorResult = m_impl->device->CreateTransientDescriptorAllocator(kTransientDescriptorCount);
		if (!transientAllocatorResult)
		{
			return MakeFail(
				transientAllocatorResult.error.code,
				transientAllocatorResult.error.message);
		}
		m_impl->transientAllocators[frameIndex] = std::move(transientAllocatorResult.value);

		FrameContext& frameContext = m_rhi.frameContexts[frameIndex];
		frameContext.frameIndex = frameIndex;
		frameContext.fenceValue = 0;
		frameContext.graphicsCommandList = m_impl->commandLists[frameIndex].get();
		frameContext.uploadBuffer = m_impl->uploadBuffers[frameIndex].get();
		frameContext.transientDescriptors = m_impl->transientAllocators[frameIndex].get();
	}

	return MakeOk();
}

void RenderSystem::EndBarrierFrame()
{
	if (m_rhi.barrierDebug != nullptr)
	{
		m_rhi.barrierDebug->EndFrame();
	}
}

void RenderSystem::DiscardRecording(FrameContext& frameContext)
{
	RHICommandList* commandList = frameContext.graphicsCommandList;
	if (commandList != nullptr && commandList->IsRecording())
	{
		commandList->Close();
	}
	EndBarrierFrame();
	m_recording = false;
}

Result<void> RenderSystem::Initialize(
	HWND hwnd,
	uint32_t width,
	uint32_t height,
	const EngineLoopConfig& config,
	SubsystemContext& ctx)
{
	if (hwnd == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem requires a valid window handle");
	}

	if (width == 0 || height == 0)
	{
		return FailRuntime(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem requires non-zero client dimensions");
	}

	if (config.shaderRoot.empty() || config.assetsRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem requires shader and asset roots");
	}

	m_config = config;
	m_impl = std::make_unique<RenderSystemImpl>();

	DX12DebugSettingsData::LoadFromFile(m_config.dx12DebugConfigPath);

	m_impl->device = std::make_unique<DX12Device>();
	m_impl->barrierDebug = std::make_unique<DX12BarrierDebug>();
	if (auto initResult = m_impl->device->Initialize(); !initResult)
	{
		Shutdown(ctx);
		return initResult;
	}

	auto queueResult = m_impl->device->CreateCommandQueue();
	if (!queueResult)
	{
		Shutdown(ctx);
		return MakeFail(queueResult.error.code, queueResult.error.message);
	}
	m_impl->graphicsQueue = std::move(queueResult.value);

	auto fenceResult = m_impl->device->CreateFence();
	if (!fenceResult)
	{
		Shutdown(ctx);
		return MakeFail(fenceResult.error.code, fenceResult.error.message);
	}
	m_impl->frameFence = std::move(fenceResult.value);

	if (auto frameResourcesResult = CreateFrameResources(); !frameResourcesResult)
	{
		Shutdown(ctx);
		return frameResourcesResult;
	}

	m_rhi.device = m_impl->device.get();
	m_rhi.graphicsQueue = m_impl->graphicsQueue.get();
	m_rhi.frameFence = m_impl->frameFence.get();
	m_rhi.barrierDebug = m_impl->barrierDebug.get();
	m_rhi.currentFrameSlot = 0;

	DisplayConfig displayConfig{};
	displayConfig.width = width;
	displayConfig.height = height;
	displayConfig.bufferCount = EngineConstants::kSwapChainBufferCount;

	auto displayResult = DisplayContext::Create(
		m_rhi.device,
		m_rhi.graphicsQueue,
		hwnd,
		displayConfig);
	if (!displayResult)
	{
		Shutdown(ctx);
		return MakeFail(displayResult.error.code, displayResult.error.message);
	}
	m_impl->display = std::move(displayResult.value);

	auto descriptorAllocatorResult = m_rhi.device->CreateDescriptorAllocator(kPersistentDescriptorCount);
	if (!descriptorAllocatorResult)
	{
		Shutdown(ctx);
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

	if (auto rendererResult = m_impl->renderer->Initialize(
			m_rhi.device,
			m_impl->descriptorAllocator.get(),
			rendererConfig);
		!rendererResult)
	{
		Shutdown(ctx);
		return rendererResult;
	}

	m_render.resources = m_impl->renderer->GetResourceServices();
	m_render.shaderRoot = m_config.shaderRoot;
	ctx.RegisterService(&m_rhi);
	ctx.RegisterService(&m_render);
	return MakeOk();
}

void RenderSystem::Shutdown(SubsystemContext& ctx)
{
	if (m_impl == nullptr)
	{
		return;
	}

	RetireGraphicsQueue(&m_rhi);
	ctx.UnregisterService(&m_render);
	ctx.UnregisterService(&m_rhi);
	m_render = {};
	m_impl->renderer.reset();
	m_impl->descriptorAllocator.reset();
	m_impl->display.reset();
	m_impl.reset();
	m_rhi = {};
	m_recording = false;
}

Result<void> RenderSystem::Resize(uint32_t width, uint32_t height)
{
	if (m_impl == nullptr || m_impl->display == nullptr || width == 0 || height == 0)
	{
		return MakeOk();
	}

	RetireGraphicsQueue(&m_rhi);
	return m_impl->display->Resize(width, height);
}

Result<FrameContext*> RenderSystem::BeginRecording()
{
	if (m_impl == nullptr || m_rhi.frameFence == nullptr || m_rhi.graphicsQueue == nullptr)
	{
		return FailInternal<FrameContext*>(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem::BeginRecording requires an initialized device");
	}

	if (m_recording)
	{
		return FailInternal<FrameContext*>(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem::BeginRecording called while a frame is already recording");
	}

	const uint32_t slot = m_rhi.currentFrameSlot;
	FrameContext& frameContext = m_rhi.frameContexts[slot];
	frameContext.frameIndex = slot;

	if (m_rhi.barrierDebug != nullptr)
	{
		m_rhi.barrierDebug->BeginFrame();
	}

	RHICommandList* commandList = frameContext.graphicsCommandList;
	if (commandList == nullptr)
	{
		EndBarrierFrame();
		return FailInternal<FrameContext*>(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem requires a command list");
	}

	commandList->Reset();
	if (frameContext.uploadBuffer != nullptr)
	{
		frameContext.uploadBuffer->Reset();
	}
	if (frameContext.transientDescriptors != nullptr)
	{
		frameContext.transientDescriptors->Reset();
	}

	m_recording = true;
	return MakeOk(&frameContext);
}

Result<void> RenderSystem::SubmitAndWait()
{
	if (!m_recording)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem::SubmitAndWait requires BeginRecording");
	}

	const uint32_t slot = m_rhi.currentFrameSlot;
	FrameContext& frameContext = m_rhi.frameContexts[slot];
	RHICommandList* commandList = frameContext.graphicsCommandList;
	if (commandList == nullptr || !commandList->IsRecording())
	{
		EndBarrierFrame();
		m_recording = false;
		return FailRuntime(LogCategory::Core, ErrorCode::RuntimeError,
			"RenderSystem::SubmitAndWait failed to close the command list");
	}

	commandList->Close();
	if (commandList->IsRecording())
	{
		EndBarrierFrame();
		m_recording = false;
		return FailRuntime(LogCategory::Core, ErrorCode::RuntimeError,
			"RenderSystem::SubmitAndWait failed to close the command list");
	}

	m_rhi.graphicsQueue->ExecuteCommandLists({ commandList });
	const uint64_t fenceValue = m_rhi.graphicsQueue->Signal(m_rhi.frameFence);
	m_rhi.frameFence->WaitCPU(fenceValue);
	frameContext.fenceValue = 0;
	EndBarrierFrame();
	m_recording = false;
	return MakeOk();
}

void RenderSystem::AbortRecording()
{
	if (!m_recording)
	{
		return;
	}

	const uint32_t slot = m_rhi.currentFrameSlot;
	DiscardRecording(m_rhi.frameContexts[slot]);
}

Result<void> RenderSystem::Render(
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	const ExtractedView& view,
	const ExtractedLighting& lighting,
	std::span<const ExtractedObject> objects,
	RHIImGui* imgui)
{
	if (viewportWidth == 0 || viewportHeight == 0)
	{
		return MakeOk();
	}

	if (m_impl == nullptr || m_impl->display == nullptr || m_impl->renderer == nullptr
		|| m_rhi.frameFence == nullptr || m_rhi.graphicsQueue == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem::Render requires an initialized device and display");
	}

	const uint32_t slot = m_rhi.currentFrameSlot;
	FrameContext& frameContext = m_rhi.frameContexts[slot];
	frameContext.frameIndex = slot;

	if (frameContext.fenceValue != 0)
	{
		m_rhi.frameFence->WaitCPU(frameContext.fenceValue);
	}

	if (m_rhi.barrierDebug != nullptr)
	{
		m_rhi.barrierDebug->BeginFrame();
	}

	m_impl->display->BeginFrame(frameContext);

	RHICommandList* commandList = frameContext.graphicsCommandList;
	if (commandList == nullptr)
	{
		EndBarrierFrame();
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RenderSystem requires a command list");
	}

	commandList->Reset();
	if (frameContext.uploadBuffer != nullptr)
	{
		frameContext.uploadBuffer->Reset();
	}
	if (frameContext.transientDescriptors != nullptr)
	{
		frameContext.transientDescriptors->Reset();
	}

	m_recording = true;
	m_impl->renderer->SetFrameContext(&frameContext);
	m_impl->renderer->BeginFrame(frameContext.frameIndex);
	m_impl->renderer->ExtractView(view);
	m_impl->renderer->ExtractLighting(lighting);
	m_impl->renderer->ExtractScene(objects);

	{
		const RHIScopedDebugEvent frameEvent(commandList, "Frame");
		m_impl->renderer->BuildScene(commandList);
		m_impl->renderer->RenderShadow(commandList);
		m_impl->display->BeginMainRenderPass(frameContext, commandList);
		m_impl->renderer->Render(commandList);
		m_impl->display->PrepareSceneDepthForRead(frameContext, commandList);
		m_impl->renderer->DrawSceneDepthDebug(commandList);
		if (imgui != nullptr)
		{
			imgui->Draw(commandList);
		}
		m_impl->display->EndMainRenderPass(frameContext, commandList);
	}

	if (!commandList->IsRecording())
	{
		EndBarrierFrame();
		m_recording = false;
		return FailRuntime(LogCategory::Core, ErrorCode::RuntimeError,
			"RenderSystem::Render failed to close the command list");
	}

	commandList->Close();
	if (commandList->IsRecording())
	{
		EndBarrierFrame();
		m_recording = false;
		return FailRuntime(LogCategory::Core, ErrorCode::RuntimeError,
			"RenderSystem::Render failed to close the command list");
	}

	m_rhi.graphicsQueue->ExecuteCommandLists({ commandList });
	frameContext.fenceValue = m_rhi.graphicsQueue->Signal(m_rhi.frameFence);
	m_impl->display->Present(m_config.vsync ? 1u : 0u, 0u);
	EndBarrierFrame();
	m_recording = false;

	m_rhi.currentFrameSlot = (slot + 1) % RHIServices::kFrameCount;
	return MakeOk();
}
