#pragma once

#include "Engine/RHI/Common/RHIResource.h"
#include "Engine/RHI/Common/RHITexture.h"
#include "Engine/RHI/Common/RHIPipeline.h"
#include "Engine/RHI/Common/RHIDescriptor.h"
#include "Engine/RHI/Common/RHIShaderBinding.h"
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Common/RHIState.h"
#include "Engine/RHI/Common/RHIFormat.h"

class RHISwapChain;
class RHICommandList;
class RHICommandQueue;
class RHIVertexBuffer;
class RHIIndexBuffer;
class RHIConstantBuffer;
class RHIStructuredBuffer;
class RHITexture;
class RHIDescriptorAllocator;
class RHITransientDescriptorAllocator;
class RHIDSVAllocator;
class RHIRTVAllocator;
class RHIUploadBuffer;
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
	virtual std::unique_ptr<RHITransientDescriptorAllocator> CreateTransientDescriptorAllocator(uint32_t numDescriptors) = 0;
	virtual std::unique_ptr<RHIDSVAllocator> CreateDSVAllocator(uint32_t numDescriptors) = 0;
	virtual std::unique_ptr<RHIRTVAllocator> CreateRTVAllocator(uint32_t numDescriptors) = 0;

	// Upload / frame-local buffers
	virtual std::unique_ptr<RHIUploadBuffer> CreateUploadBuffer(size_t capacityInBytes) = 0;

	// Pipeline Creation
	virtual std::unique_ptr<RHIRootSignature> CreateRootSignature() = 0;
	virtual std::unique_ptr<RHIPipelineState> CreatePipelineState(const RHIPipelineDesc& pipelineDesc) = 0;

	// Sync Creation
	virtual std::unique_ptr<RHIFence> CreateFence() = 0;
};