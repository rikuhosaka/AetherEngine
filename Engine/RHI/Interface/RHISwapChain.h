#pragma once

class RHITexture;

class RHISwapChain
{
public:
	virtual ~RHISwapChain() = default;
	virtual std::unique_ptr<RHITexture> GetCurrentBackBuffer() = 0;
	virtual uint32_t GetCurrentBackBufferIndex() = 0;

	virtual void Present(uint32_t syncInterval, uint32_t flags) = 0;
protected:
	RHISwapChain() = default;
};