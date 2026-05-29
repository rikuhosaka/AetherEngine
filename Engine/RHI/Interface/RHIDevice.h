#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Common/RHIResource.h"
#include "Engine/RHI/Common/RHIDescriptor.h"
#include "Engine/RHI/Common/RHITexture.h"
#include "Engine/RHI/Common/RHIPipeline.h"
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Common/RHIRootSignatureLayout.h"

#include <cstddef>
#include <memory>
#include <span>

class RHISwapChain;
class RHICommandList;
class RHICommandQueue;
class RHIVertexShader;
class RHIPixelShader;
class RHIBuffer;
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
	virtual Result<void> Initialize() = 0;

	// Resource Command Creation
	virtual Result<std::unique_ptr<RHICommandList>> CreateCommandList() = 0;
	virtual Result<std::unique_ptr<RHICommandQueue>> CreateCommandQueue() = 0;
	virtual Result<std::unique_ptr<RHISwapChain>> CreateSwapChain(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		const RHICommandQueue* commandQueue) = 0;

	// Resource Creation
	virtual Result<std::unique_ptr<RHIVertexBuffer>> CreateVertexBuffer(
		const RHIBufferDesc& bufferDesc,
		uint32_t stride) = 0;
	virtual Result<std::unique_ptr<RHIIndexBuffer>> CreateIndexBuffer(
		const RHIBufferDesc& bufferDesc,
		IndexFormat indexFormat) = 0;
	virtual Result<std::unique_ptr<RHIConstantBuffer>> CreateConstantBuffer(
		const RHIBufferDesc& bufferDesc) = 0;
	virtual Result<std::unique_ptr<RHIStructuredBuffer>> CreateStructuredBuffer(
		const RHIBufferDesc& bufferDesc) = 0;
	virtual Result<std::unique_ptr<RHITexture>> CreateTexture(const RHITextureDesc& desc) = 0;

	virtual Result<std::unique_ptr<RHIVertexShader>> CreateVertexShader(
		std::span<const std::byte> bytecode) = 0;
	virtual Result<std::unique_ptr<RHIPixelShader>> CreatePixelShader(
		std::span<const std::byte> bytecode) = 0;

	virtual Result<CbvSrvUavHandle> CreateShaderResourceView(
		RHITexture* texture,
		RHIDescriptorAllocator* allocator) = 0;

	virtual void WriteShaderResourceView(RHITexture* texture, CpuDescHandle destCpuHandle) = 0;
	virtual void WriteRenderTargetView(RHITexture* texture, RtvHandle dest) = 0;
	virtual void WriteDepthStencilView(RHITexture* texture, DsvHandle dest) = 0;
	virtual void WriteConstantBufferView(
		RHIBuffer* buffer,
		CpuDescHandle destCpuHandle,
		uint32_t bufferSizeInBytes) = 0;

	virtual void WriteConstantBufferView(
		RHIUploadBuffer* upload,
		size_t offset,
		uint32_t sizeInBytes,
		CpuDescHandle destCpuHandle) = 0;

	// Descriptor Creation
	virtual Result<std::unique_ptr<RHIDescriptorAllocator>> CreateDescriptorAllocator(
		uint32_t numDescriptors) = 0;
	virtual Result<std::unique_ptr<RHITransientDescriptorAllocator>> CreateTransientDescriptorAllocator(
		uint32_t numDescriptors) = 0;
	virtual Result<std::unique_ptr<RHIDSVAllocator>> CreateDSVAllocator(uint32_t numDescriptors) = 0;
	virtual Result<std::unique_ptr<RHIRTVAllocator>> CreateRTVAllocator(uint32_t numDescriptors) = 0;

	// Upload / frame-local buffers
	virtual Result<std::unique_ptr<RHIUploadBuffer>> CreateUploadBuffer(size_t capacityInBytes) = 0;

	// Pipeline Creation
	virtual Result<std::unique_ptr<RHIRootSignature>> CreateRootSignature(
		const RHIRootSignatureLayout& layout) = 0;
	virtual Result<std::unique_ptr<RHIPipelineState>> CreatePipelineState(
		const RHIPipelineDesc& pipelineDesc) = 0;

	// Sync Creation
	virtual Result<std::unique_ptr<RHIFence>> CreateFence() = 0;
};
