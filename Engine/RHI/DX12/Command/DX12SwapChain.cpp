#include "DX12SwapChain.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Command/DX12CommandQueue.h"
#include "Engine/RHI/DX12/Command/CommandImpl.h"
#include "Engine/RHI/DX12/Resource/DX12Texture.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"


class DX12SwapChain::Impl
{
public:
	ComPtr<IDXGISwapChain4> swapChain;
};

DX12SwapChain::DX12SwapChain(
	HWND hwnd,
	uint32_t width,
	uint32_t height,
	const DX12CommandQueue* dxCommandQueue,
	const DX12Device* dxDevice)
{
	// SwapChain�ݒ�
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 2;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> swapChain1;
	ComPtr<IDXGISwapChain4> swapChain;

	// DXGI Factory�̎擾
	IDXGIFactory6* dxgiFactory = dxDevice->GetImpl()->factory.Get();
	ID3D12CommandQueue* commandQueue = dxCommandQueue->GetImpl()->commandQueue.Get();
	// �쐬
	dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&desc,
		nullptr,
		nullptr,
		&swapChain1
	);

	// �ϊ�
	swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain));
	m_impl->swapChain = swapChain;
}

DX12SwapChain::~DX12SwapChain()
{
	if (m_impl->swapChain)
	{
		m_impl->swapChain->Release();
		m_impl->swapChain = nullptr;
	}
}

std::unique_ptr<RHITexture>
DX12SwapChain::GetCurrentBackBuffer()
{
	// ���݂̃o�b�N�o�b�t�@���擾
	uint32_t backBufferIndex = m_impl->swapChain->GetCurrentBackBufferIndex();
	ComPtr<ID3D12Resource> backBuffer;
	m_impl->swapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(&backBuffer));
	std::unique_ptr<ResourceImpl> resourceImpl = std::make_unique<ResourceImpl>();
	resourceImpl->resource = backBuffer;
	// RHITexture�Ƀ��b�v���ĕԂ�
	D3D12_RESOURCE_DESC resourceDesc = backBuffer->GetDesc();
	RHITextureDesc desc = {};
	desc.Width = static_cast<uint32_t>(resourceDesc.Width);
	desc.Height = resourceDesc.Height;
	desc.MipLevels = resourceDesc.MipLevels;
	desc.ArraySize = resourceDesc.DepthOrArraySize;
	desc.SampleCount = resourceDesc.SampleDesc.Count;
	desc.SampleQuality = resourceDesc.SampleDesc.Quality;
	desc.Usage = ERHITextureUsage::RenderTarget; // �o�b�N�o�b�t�@�̓����_�[�^�[�Q�b�g�Ƃ��Ďg�p����邽�߁A�K�؂Ȏg�p�t���O��ݒ肵�܂��B

	return DX12Texture::Create(desc, std::move(resourceImpl));
}

uint32_t DX12SwapChain::GetCurrentBackBufferIndex()
{
	return m_impl->swapChain->GetCurrentBackBufferIndex();
}

void DX12SwapChain::Present(uint32_t syncInterval, uint32_t flags)
{
	m_impl->swapChain->Present(syncInterval, flags);
}