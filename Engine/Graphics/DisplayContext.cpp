#include "Engine/Graphics/DisplayContext.h"

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

std::unique_ptr<DisplayContext> DisplayContext::Create(
	RHIDevice* device,
	RHICommandQueue* graphicsQueue,
	HWND hwnd,
	const DisplayConfig& config)
{
	assert(device != nullptr);
	assert(graphicsQueue != nullptr);
	assert(hwnd != nullptr);

	auto display = std::unique_ptr<DisplayContext>(new DisplayContext());
	display->m_device = device;
	display->m_config = config;

	display->m_swapChain = device->CreateSwapChain(hwnd, config.width, config.height, graphicsQueue);
	assert(display->m_swapChain != nullptr);

	const uint32_t bufferCount = display->m_swapChain->GetBufferCount();
	assert(bufferCount > 0);

	display->m_rtvAllocator = device->CreateRTVAllocator(bufferCount);
	display->m_dsvAllocator = device->CreateDSVAllocator(1);
	assert(display->m_rtvAllocator != nullptr);
	assert(display->m_dsvAllocator != nullptr);

	display->CreateDepthResources();
	display->CreateBackBufferViews();
	return display;
}

void DisplayContext::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	if (width == m_config.width && height == m_config.height)
	{
		return;
	}

	m_config.width = width;
	m_config.height = height;

	DestroyRenderTargets();
	m_swapChain->Resize(width, height);

	const uint32_t bufferCount = m_swapChain->GetBufferCount();
	m_rtvAllocator = m_device->CreateRTVAllocator(bufferCount);
	m_dsvAllocator = m_device->CreateDSVAllocator(1);
	assert(m_rtvAllocator != nullptr);
	assert(m_dsvAllocator != nullptr);

	CreateDepthResources();
	CreateBackBufferViews();
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

void DisplayContext::CreateDepthResources()
{
	RHITextureDesc depthDesc{};
	depthDesc.Width = m_config.width;
	depthDesc.Height = m_config.height;
	depthDesc.Usage = ERHITextureUsage::DepthStencil;
	depthDesc.Format = ERHIFormat::D32_FLOAT;

	m_depthTexture = m_device->CreateTexture(depthDesc);
	assert(m_depthTexture != nullptr);

	m_depthDsv = m_dsvAllocator->Allocate();
	m_device->WriteDepthStencilView(m_depthTexture.get(), m_depthDsv);
}

void DisplayContext::CreateBackBufferViews()
{
	assert(m_swapChain != nullptr);
	assert(m_rtvAllocator != nullptr);

	const uint32_t bufferCount = m_swapChain->GetBufferCount();
	m_backBufferRtvs.resize(bufferCount);

	for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex)
	{
		RHITexture* backBuffer = m_swapChain->GetBackBuffer(bufferIndex);
		assert(backBuffer != nullptr);

		m_backBufferRtvs[bufferIndex] = m_rtvAllocator->Allocate();
		m_device->WriteRenderTargetView(backBuffer, m_backBufferRtvs[bufferIndex]);
	}
}

void DisplayContext::DestroyRenderTargets()
{
	m_backBufferRtvs.clear();
	m_depthTexture.reset();
	m_depthDsv = {};
}
