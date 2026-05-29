#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHISwapChain.h"

#include <memory>

class DX12Device;
class DX12CommandQueue;
class DX12Texture;

class DX12SwapChain : public RHISwapChain
{
public:
	~DX12SwapChain() override;

	uint32_t GetBufferCount() const override;
	RHITexture* GetBackBuffer(uint32_t index) override;
	uint32_t GetCurrentBackBufferIndex() override;
	uint32_t GetWidth() const override;
	uint32_t GetHeight() const override;

	Result<void> Resize(uint32_t width, uint32_t height) override;
	void Present(uint32_t syncInterval, uint32_t flags) override;

	[[nodiscard]] bool IsValid() const;

protected:
	DX12SwapChain(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		uint32_t bufferCount,
		const DX12CommandQueue* commandQueue,
		const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12SwapChain>> Create(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		const DX12CommandQueue* commandQueue,
		const DX12Device* dxDevice);

	class Impl;
	std::unique_ptr<Impl> m_impl{};

	Result<void> CreateBackBuffers();

	friend class DX12Device;
};
