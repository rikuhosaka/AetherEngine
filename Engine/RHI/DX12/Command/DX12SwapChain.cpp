#include "DX12SwapChain.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Command/DX12CommandQueue.h"
#include "Engine/RHI/DX12/Command/CommandImpl.h"
#include "Engine/RHI/DX12/Resource/DX12Texture.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"

class DX12SwapChain::Impl
{
public:
	ComPtr<IDXGISwapChain4> swapChain{};
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t bufferCount = 2;
	std::vector<std::unique_ptr<DX12Texture>> backBuffers{};
};

bool DX12SwapChain::IsValid() const
{
	return m_impl != nullptr
		&& m_impl->swapChain != nullptr
		&& m_impl->backBuffers.size() == m_impl->bufferCount;
}

Result<std::unique_ptr<DX12SwapChain>> DX12SwapChain::Create(
	HWND hwnd,
	uint32_t width,
	uint32_t height,
	uint32_t bufferCount,
	const DX12CommandQueue* commandQueue,
	const DX12Device* dxDevice)
{
	if (bufferCount == 0)
	{
		return FailInternal<std::unique_ptr<DX12SwapChain>>(
			LogCategory::RHI,
			ErrorCode::InvalidArgument,
			"Swap chain buffer count must be non-zero");
	}

	return MakeResourceResult(
		std::unique_ptr<DX12SwapChain>(new DX12SwapChain(
			hwnd,
			width,
			height,
			bufferCount,
			commandQueue,
			dxDevice)),
		"Failed to create swap chain");
}

DX12SwapChain::DX12SwapChain(
	HWND hwnd,
	uint32_t width,
	uint32_t height,
	uint32_t bufferCount,
	const DX12CommandQueue* dxCommandQueue,
	const DX12Device* dxDevice)
	: m_impl(std::make_unique<Impl>())
{
	m_impl->width = width;
	m_impl->height = height;
	m_impl->bufferCount = bufferCount;

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = bufferCount;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	IDXGIFactory6* dxgiFactory = dxDevice->GetImpl()->factory.Get();
	ID3D12CommandQueue* commandQueue = dxCommandQueue->GetImpl()->commandQueue.Get();

	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT result = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&desc,
		nullptr,
		nullptr,
		&swapChain1);
	if (FAILED(result))
	{
		return;
	}

	dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	ComPtr<IDXGISwapChain4> swapChain4;
	result = swapChain1.As(&swapChain4);
	if (FAILED(result))
	{
		return;
	}

	m_impl->swapChain = swapChain4;
	(void)CreateBackBuffers();
}

DX12SwapChain::~DX12SwapChain() = default;

Result<void> DX12SwapChain::CreateBackBuffers()
{
	m_impl->backBuffers.clear();
	m_impl->backBuffers.reserve(m_impl->bufferCount);

	for (uint32_t bufferIndex = 0; bufferIndex < m_impl->bufferCount; ++bufferIndex)
	{
		ComPtr<ID3D12Resource> backBuffer;
		const HRESULT result = m_impl->swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&backBuffer));
		if (FAILED(result))
		{
			m_impl->backBuffers.clear();
			return FailRuntime(LogCategory::RHI, ErrorCode::ResourceCreationFailed,
				"Failed to get swap chain back buffer");
		}

		const D3D12_RESOURCE_DESC resourceDesc = backBuffer->GetDesc();
		RHITextureDesc textureDesc = {};
		textureDesc.Width = static_cast<uint32_t>(resourceDesc.Width);
		textureDesc.Height = resourceDesc.Height;
		textureDesc.MipLevels = resourceDesc.MipLevels;
		textureDesc.ArraySize = resourceDesc.DepthOrArraySize;
		textureDesc.SampleCount = resourceDesc.SampleDesc.Count;
		textureDesc.SampleQuality = resourceDesc.SampleDesc.Quality;
		textureDesc.Usage = ERHITextureUsage::RenderTarget;
		textureDesc.Format = ERHIFormat::R8G8B8A8_UNORM;

		auto resourceImpl = std::make_unique<ResourceImpl>();
		resourceImpl->resource = backBuffer;
		resourceImpl->SetInitialState(ERHIResourceState::Present);

		auto textureResult = DX12Texture::Create(textureDesc, std::move(resourceImpl));
		if (!textureResult)
		{
			m_impl->backBuffers.clear();
			return MakeFail(textureResult.error.code, textureResult.error.message);
		}

		m_impl->backBuffers.push_back(std::move(textureResult.value));
	}

	return MakeOk();
}

uint32_t DX12SwapChain::GetBufferCount() const
{
	return m_impl->bufferCount;
}

RHITexture* DX12SwapChain::GetBackBuffer(uint32_t index)
{
	if (index >= m_impl->backBuffers.size())
	{
		return nullptr;
	}

	return m_impl->backBuffers[index].get();
}

uint32_t DX12SwapChain::GetCurrentBackBufferIndex()
{
	return m_impl->swapChain->GetCurrentBackBufferIndex();
}

uint32_t DX12SwapChain::GetWidth() const
{
	return m_impl->width;
}

uint32_t DX12SwapChain::GetHeight() const
{
	return m_impl->height;
}

Result<void> DX12SwapChain::Resize(uint32_t width, uint32_t height)
{
	if (width == m_impl->width && height == m_impl->height)
	{
		return MakeOk();
	}

	m_impl->width = width;
	m_impl->height = height;
	m_impl->backBuffers.clear();

	const HRESULT result = m_impl->swapChain->ResizeBuffers(
		m_impl->bufferCount,
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0);
	if (FAILED(result))
	{
		return FailRuntime(LogCategory::RHI, ErrorCode::ResourceCreationFailed,
			"Failed to resize swap chain buffers");
	}

	return CreateBackBuffers();
}

void DX12SwapChain::Present(uint32_t syncInterval, uint32_t flags)
{
	m_impl->swapChain->Present(syncInterval, flags);
}
