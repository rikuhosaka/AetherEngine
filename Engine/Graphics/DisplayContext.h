#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Graphics/DisplayConfig.h"

#include <memory>
#include <vector>

class RHIDevice;
class RHICommandQueue;
class RHISwapChain;
class RHIRTVAllocator;
class RHIDSVAllocator;
class RHITexture;
class RHICommandList;

class DisplayContext
{
public:
	~DisplayContext();

	static Result<std::unique_ptr<DisplayContext>> Create(
		RHIDevice* device,
		RHICommandQueue* graphicsQueue,
		HWND hwnd,
		const DisplayConfig& config);

	Result<void> Resize(uint32_t width, uint32_t height);

	void BeginFrame(FrameContext& frameContext);
	void BeginMainRenderPass(FrameContext& frameContext, RHICommandList* commandList);
	void EndMainRenderPass(FrameContext& frameContext, RHICommandList* commandList);
	void Present(uint32_t syncInterval = 1, uint32_t flags = 0);

	[[nodiscard]] RHISwapChain* GetSwapChain() noexcept { return m_swapChain.get(); }
	[[nodiscard]] const DisplayConfig& GetConfig() const noexcept { return m_config; }

private:
	DisplayContext() = default;

	Result<void> CreateDepthResources();
	Result<void> CreateBackBufferViews();
	void DestroyRenderTargets();

	DisplayConfig m_config{};
	RHIDevice* m_device = nullptr;

	std::unique_ptr<RHISwapChain> m_swapChain{};
	std::unique_ptr<RHIRTVAllocator> m_rtvAllocator{};
	std::unique_ptr<RHIDSVAllocator> m_dsvAllocator{};

	std::vector<RtvHandle> m_backBufferRtvs{};
	std::unique_ptr<RHITexture> m_depthTexture{};
	DsvHandle m_depthDsv{};
};
