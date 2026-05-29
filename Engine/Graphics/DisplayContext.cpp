#include "Engine/Graphics/DisplayContext.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Common/RHITexture.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHITexture.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIDSVAllocator.h"
#include "Engine/RHI/Interface/RHIRTVAllocator.h"
#include "Engine/RHI/Interface/RHIResource.h"
#include "Engine/RHI/Interface/RHISwapChain.h"

#include <cassert>

DisplayContext::~DisplayContext() = default;

Result<std::unique_ptr<DisplayContext>> DisplayContext::Create(
	RHIDevice* device,
	RHICommandQueue* graphicsQueue,
	HWND hwnd,
	const DisplayConfig& config)
{
	if (device == nullptr || graphicsQueue == nullptr || hwnd == nullptr)
	{
		return MakeFail<std::unique_ptr<DisplayContext>>(
			ErrorCode::InvalidArgument,
			"DisplayContext::Create received null argument");
	}

	auto display = std::unique_ptr<DisplayContext>(new DisplayContext());
	display->m_device = device;
	display->m_config = config;

	auto swapChainResult = device->CreateSwapChain(hwnd, config.width, config.height, graphicsQueue);
	if (!swapChainResult)
	{
		return MakeFail<std::unique_ptr<DisplayContext>>(
			swapChainResult.error.code,
			swapChainResult.error.message);
	}
	display->m_swapChain = std::move(swapChainResult.value);

	const uint32_t bufferCount = display->m_swapChain->GetBufferCount();
	if (bufferCount == 0)
	{
		return MakeFail<std::unique_ptr<DisplayContext>>(
			ErrorCode::ResourceCreationFailed,
			"Swap chain returned zero back buffers");
	}

	auto rtvAllocatorResult = device->CreateRTVAllocator(bufferCount);
	if (!rtvAllocatorResult)
	{
		return MakeFail<std::unique_ptr<DisplayContext>>(
			rtvAllocatorResult.error.code,
			rtvAllocatorResult.error.message);
	}

	auto dsvAllocatorResult = device->CreateDSVAllocator(1);
	if (!dsvAllocatorResult)
	{
		return MakeFail<std::unique_ptr<DisplayContext>>(
			dsvAllocatorResult.error.code,
			dsvAllocatorResult.error.message);
	}
	display->m_rtvAllocator = std::move(rtvAllocatorResult.value);
	display->m_dsvAllocator = std::move(dsvAllocatorResult.value);

	if (auto depthResult = display->CreateDepthResources(); !depthResult)
	{
		return MakeFail<std::unique_ptr<DisplayContext>>(
			depthResult.error.code,
			depthResult.error.message);
	}

	if (auto backBufferResult = display->CreateBackBufferViews(); !backBufferResult)
	{
		return MakeFail<std::unique_ptr<DisplayContext>>(
			backBufferResult.error.code,
			backBufferResult.error.message);
	}

	return MakeOk(std::move(display));
}

Result<void> DisplayContext::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
	{
		return MakeFail(ErrorCode::InvalidArgument, "DisplayContext::Resize requires non-zero dimensions");
	}

	if (width == m_config.width && height == m_config.height)
	{
		return MakeOk();
	}

	m_config.width = width;
	m_config.height = height;

	DestroyRenderTargets();

	auto resizeResult = m_swapChain->Resize(width, height);
	if (!resizeResult)
	{
		return resizeResult;
	}

	const uint32_t bufferCount = m_swapChain->GetBufferCount();
	auto rtvAllocatorResult = m_device->CreateRTVAllocator(bufferCount);
	if (!rtvAllocatorResult)
	{
		return MakeFail(rtvAllocatorResult.error.code, rtvAllocatorResult.error.message);
	}

	auto dsvAllocatorResult = m_device->CreateDSVAllocator(1);
	if (!dsvAllocatorResult)
	{
		return MakeFail(dsvAllocatorResult.error.code, dsvAllocatorResult.error.message);
	}
	m_rtvAllocator = std::move(rtvAllocatorResult.value);
	m_dsvAllocator = std::move(dsvAllocatorResult.value);

	if (auto depthResult = CreateDepthResources(); !depthResult)
	{
		return depthResult;
	}

	return CreateBackBufferViews();
}

void DisplayContext::BeginFrame(FrameContext& frameContext)
{
	assert(m_swapChain != nullptr);

	const uint32_t backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
	frameContext.backBufferIndex = backBufferIndex;
	frameContext.backBuffer = m_swapChain->GetBackBuffer(backBufferIndex);
	frameContext.backBufferRtv = m_backBufferRtvs[backBufferIndex];
	frameContext.depthDsv = m_depthDsv;
	frameContext.depthTexture = m_depthTexture.get();
	frameContext.renderWidth = m_config.width;
	frameContext.renderHeight = m_config.height;
}

void DisplayContext::BeginMainRenderPass(FrameContext& frameContext, RHICommandList* commandList)
{
	assert(commandList != nullptr);
	assert(frameContext.backBuffer != nullptr);
	assert(frameContext.depthTexture != nullptr);
	assert(frameContext.backBufferRtv.cpu.ptr != 0);
	assert(frameContext.depthDsv.cpu.ptr != 0);

	frameContext.backBuffer->TransitionResource(ERHIResourceState::RenderTarget, commandList);
	frameContext.depthTexture->TransitionResource(ERHIResourceState::DepthWrite, commandList);

	commandList->OMSetRenderTargets(1, frameContext.backBufferRtv, true, frameContext.depthDsv);
	commandList->ClearRenderTargetView(frameContext.backBufferRtv, m_config.clearColor);
	commandList->ClearDepthStencilView(frameContext.depthDsv, m_config.clearDepth, m_config.clearStencil);

	commandList->RSSetViewports(
		0.0f,
		0.0f,
		static_cast<float>(frameContext.renderWidth),
		static_cast<float>(frameContext.renderHeight));
	commandList->RSSetScissorRects(
		0,
		0,
		static_cast<int>(frameContext.renderWidth),
		static_cast<int>(frameContext.renderHeight));
}

void DisplayContext::EndMainRenderPass(FrameContext& frameContext, RHICommandList* commandList)
{
	assert(commandList != nullptr);
	assert(frameContext.backBuffer != nullptr);

	frameContext.backBuffer->TransitionResource(ERHIResourceState::Present, commandList);
}

void DisplayContext::Present(uint32_t syncInterval, uint32_t flags)
{
	assert(m_swapChain != nullptr);
	m_swapChain->Present(syncInterval, flags);
}

Result<void> DisplayContext::CreateDepthResources()
{
	RHITextureDesc depthDesc{};
	depthDesc.Width = m_config.width;
	depthDesc.Height = m_config.height;
	depthDesc.Usage = ERHITextureUsage::DepthStencil;
	depthDesc.Format = ERHIFormat::D32_FLOAT;

	auto depthTextureResult = m_device->CreateTexture(depthDesc);
	if (!depthTextureResult)
	{
		return MakeFail(depthTextureResult.error.code, depthTextureResult.error.message);
	}
	m_depthTexture = std::move(depthTextureResult.value);

	m_depthDsv = m_dsvAllocator->Allocate();
	m_device->WriteDepthStencilView(m_depthTexture.get(), m_depthDsv);
	return MakeOk();
}

Result<void> DisplayContext::CreateBackBufferViews()
{
	if (m_swapChain == nullptr || m_rtvAllocator == nullptr)
	{
		return MakeFail(ErrorCode::InvalidArgument, "DisplayContext render target dependencies are null");
	}

	const uint32_t bufferCount = m_swapChain->GetBufferCount();
	m_backBufferRtvs.resize(bufferCount);

	for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
	{
		RHITexture* backBuffer = m_swapChain->GetBackBuffer(bufferIndex);
		if (backBuffer == nullptr)
		{
			return MakeFail(ErrorCode::ResourceCreationFailed, "Swap chain back buffer is null");
		}

		m_backBufferRtvs[bufferIndex] = m_rtvAllocator->Allocate();
		m_device->WriteRenderTargetView(backBuffer, m_backBufferRtvs[bufferIndex]);
	}
	return MakeOk();
}

void DisplayContext::DestroyRenderTargets()
{
	m_backBufferRtvs.clear();
	m_depthTexture.reset();
	m_depthDsv = {};
}
