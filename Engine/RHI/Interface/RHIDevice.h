#pragma once

#include <Engine/RHI/Interface/RHICommon.h>

class RHISwapChain;
class RHICommandList;
class RHICommandQueue;
class RHIVertexBuffer;
class RHIIndexBuffer;
class RHIConstantBuffer;
class RHIStructuredBuffer;
class RHITexture;
class RHIDescriptorAllocator;
class RHIDSVAllocator;
class RHIRTVAllocator;
class RHIRootSignature;
class RHIPipelineState;
class RHIFence;

class RHIDevice
{
public:
	virtual ~RHIDevice() = default;
	virtual void Initialize() = 0;

	
	// Resource Command Creation
	virtual std::unique_ptr<RHICommandList> CreateCommandList() = 0;
	virtual std::unique_ptr<RHICommandQueue> CreateCommandQueue() = 0;
	virtual std::unique_ptr<RHISwapChain> CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height, const RHICommandQueue* commandQueue) = 0;

	// Resource Creation
	virtual std::unique_ptr<RHIVertexBuffer> CreateVertexBuffer(const RHIBufferDesc& bufferDesc, uint32_t stride) = 0;
	virtual std::unique_ptr<RHIIndexBuffer> CreateIndexBuffer(const RHIBufferDesc& bufferDesc, IndexFormat indexFormat) = 0;
	virtual std::unique_ptr<RHIConstantBuffer> CreateConstantBuffer(const RHIBufferDesc& bufferDesc) = 0;
	virtual std::unique_ptr<RHIStructuredBuffer> CreateStructuredBuffer(const RHIBufferDesc& bufferDesc) = 0;
	virtual std::unique_ptr<RHITexture> CreateTexture(const RHITextureDesc& desc) = 0;

	// Descriptor Creation
	virtual std::unique_ptr<RHIDescriptorAllocator> CreateDescriptorAllocator(uint32_t numDescriptors) = 0;
	virtual std::unique_ptr<RHIDSVAllocator> CreateDSVAllocator(uint32_t numDescriptors) = 0;
	virtual std::unique_ptr<RHIRTVAllocator> CreateRTVAllocator(uint32_t numDescriptors) = 0;

	// Pipeline Creation
	virtual std::unique_ptr<RHIRootSignature> CreateRootSignature() = 0;
	virtual std::unique_ptr<RHIPipelineState> CreatePipelineState(const RHIPipelineDesc& pipelineDesc) = 0;

	// Sync Creation
	virtual std::unique_ptr<RHIFence> CreateFence() = 0;
};