#pragma once
#include "Engine/RHI/Interface/RHISwapChain.h"


class DX12Device;
class DX12CommandQueue;
class DX12Texture;
class DX12SwapChain : public RHISwapChain
{
public:

	~DX12SwapChain() override;

	std::unique_ptr<RHITexture> GetCurrentBackBuffer() override;
	uint32_t GetCurrentBackBufferIndex() override;


	void Present(uint32_t syncInterval, uint32_t flags) override;
protected:

	DX12SwapChain(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		const DX12CommandQueue* commandQueue,
		const DX12Device* dxDevice);

	static std::unique_ptr<DX12SwapChain> Create(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		const DX12CommandQueue* commandQueue,
		const DX12Device* dxDevice)
	{
		return std::unique_ptr<DX12SwapChain>(new DX12SwapChain(hwnd, width, height, commandQueue, dxDevice));
	}

	class Impl;
	std::unique_ptr<Impl> m_impl = nullptr;

	Impl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
};