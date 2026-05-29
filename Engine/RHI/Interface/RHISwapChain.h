#pragma once

class RHITexture;

class RHISwapChain
{
public:
	virtual ~RHISwapChain() = default;
	[[nodiscard]] virtual uint32_t GetBufferCount() const = 0;
	[[nodiscard]] virtual RHITexture* GetBackBuffer(uint32_t index) = 0;
	[[nodiscard]] virtual uint32_t GetCurrentBackBufferIndex() = 0;
	[[nodiscard]] virtual uint32_t GetWidth() const = 0;
	[[nodiscard]] virtual uint32_t GetHeight() const = 0;

	virtual void Resize(uint32_t width, uint32_t height) = 0;
	virtual void Present(uint32_t syncInterval, uint32_t flags) = 0;
protected:
	RHISwapChain() = default;
};