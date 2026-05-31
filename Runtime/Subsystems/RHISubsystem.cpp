#include "Runtime/Subsystems/RHISubsystem.h"

#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHICommandQueue.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIFence.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <array>

namespace
{
constexpr size_t kUploadBufferCapacityBytes = 64 * 1024 * 1024;
constexpr uint32_t kTransientDescriptorCount = 2048;

constexpr const char* kDependencies[] = { "Window" };
} // namespace

struct RHISubsystemImpl
{
	std::unique_ptr<RHIDevice> device{};
	std::unique_ptr<RHICommandQueue> graphicsQueue{};
	std::unique_ptr<RHIFence> frameFence{};
	std::array<std::unique_ptr<RHICommandList>, RHIServices::kFrameCount> commandLists{};
	std::array<std::unique_ptr<RHIUploadBuffer>, RHIServices::kFrameCount> uploadBuffers{};
	std::array<std::unique_ptr<RHITransientDescriptorAllocator>, RHIServices::kFrameCount> transientAllocators{};
};

RHISubsystem::RHISubsystem()
	: m_impl(std::make_unique<RHISubsystemImpl>())
{
}

RHISubsystem::~RHISubsystem() = default;

std::unique_ptr<ISubsystem> CreateRHISubsystem()
{
	return std::make_unique<RHISubsystem>();
}

std::span<const char* const> RHISubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> RHISubsystem::Initialize(SubsystemContext& ctx)
{
	if (ctx.GetService<WindowServices>() == nullptr)
	{
		return MakeFail(ErrorCode::InvalidArgument, "RHISubsystem requires WindowServices");
	}

	m_impl->device = std::make_unique<DX12Device>();
	if (auto initResult = m_impl->device->Initialize(); !initResult)
	{
		return initResult;
	}

	auto queueResult = m_impl->device->CreateCommandQueue();
	if (!queueResult)
	{
		return MakeFail(queueResult.error.code, queueResult.error.message);
	}
	m_impl->graphicsQueue = std::move(queueResult.value);

	auto fenceResult = m_impl->device->CreateFence();
	if (!fenceResult)
	{
		return MakeFail(fenceResult.error.code, fenceResult.error.message);
	}
	m_impl->frameFence = std::move(fenceResult.value);

	if (auto frameResourcesResult = CreateFrameResources(); !frameResourcesResult)
	{
		return frameResourcesResult;
	}

	m_services.device = m_impl->device.get();
	m_services.graphicsQueue = m_impl->graphicsQueue.get();
	m_services.frameFence = m_impl->frameFence.get();
	m_services.currentFrameSlot = 0;

	ctx.RegisterService(&m_services);
	return MakeOk();
}

Result<void> RHISubsystem::CreateFrameResources()
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

		FrameContext& frameContext = m_services.frameContexts[frameIndex];
		frameContext.frameIndex = frameIndex;
		frameContext.fenceValue = 0;
		frameContext.graphicsCommandList = m_impl->commandLists[frameIndex].get();
		frameContext.uploadBuffer = m_impl->uploadBuffers[frameIndex].get();
		frameContext.transientDescriptors = m_impl->transientAllocators[frameIndex].get();

		m_services.commandLists[frameIndex] = frameContext.graphicsCommandList;
		m_services.uploadBuffers[frameIndex] = frameContext.uploadBuffer;
		m_services.transientAllocators[frameIndex] = frameContext.transientDescriptors;
	}

	return MakeOk();
}

void RHISubsystem::WaitForPendingFrames()
{
	if (m_impl->frameFence == nullptr)
	{
		return;
	}

	for (FrameContext& frameContext : m_services.frameContexts)
	{
		if (frameContext.fenceValue != 0)
		{
			m_impl->frameFence->WaitCPU(frameContext.fenceValue);
			frameContext.fenceValue = 0;
		}
	}
}

void RHISubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	WaitForPendingFrames();
	m_impl = std::make_unique<RHISubsystemImpl>();
	m_services = {};
}
