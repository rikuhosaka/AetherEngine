#pragma once

#include "Engine/RHI/Interface/RHIDevice.h"

class DeviceImpl;

class DX12Device : public RHIDevice
{
public:
	DX12Device();
	~DX12Device() override;
	Result<void> Initialize() override;

	// Resource Command Creation
	Result<std::unique_ptr<RHICommandList>> CreateCommandList() override;
	Result<std::unique_ptr<RHICommandQueue>> CreateCommandQueue() override;
	Result<std::unique_ptr<RHISwapChain>> CreateSwapChain(
		HWND hwnd,
		uint32_t width,
		uint32_t height,
		uint32_t bufferCount,
		const RHICommandQueue* commandQueue) override;

	// Resource Creation
	Result<std::unique_ptr<RHIVertexBuffer>> CreateVertexBuffer(
		const RHIBufferDesc& bufferDesc,
		uint32_t stride) override;
	Result<std::unique_ptr<RHIIndexBuffer>> CreateIndexBuffer(
		const RHIBufferDesc& bufferDesc,
		IndexFormat indexFormat) override;
	Result<std::unique_ptr<RHIConstantBuffer>> CreateConstantBuffer(const RHIBufferDesc& bufferDesc) override;
	Result<std::unique_ptr<RHIStructuredBuffer>> CreateStructuredBuffer(const RHIBufferDesc& bufferDesc) override;
	Result<std::unique_ptr<RHITexture>> CreateTexture(const RHITextureDesc& desc) override;

	Result<std::unique_ptr<RHIVertexShader>> CreateVertexShader(std::span<const std::byte> bytecode) override;
	Result<std::unique_ptr<RHIPixelShader>> CreatePixelShader(std::span<const std::byte> bytecode) override;

	Result<CbvSrvUavHandle> CreateShaderResourceView(
		RHITexture* texture,
		RHIDescriptorAllocator* allocator) override;

	void WriteShaderResourceView(RHITexture* texture, CpuDescHandle destCpuHandle) override;
	void WriteRenderTargetView(RHITexture* texture, RtvHandle dest) override;
	void WriteDepthStencilView(RHITexture* texture, DsvHandle dest) override;
	void WriteConstantBufferView(
		RHIBuffer* buffer,
		CpuDescHandle destCpuHandle,
		uint32_t bufferSizeInBytes) override;

	void WriteConstantBufferView(
		RHIUploadBuffer* upload,
		size_t offset,
		uint32_t sizeInBytes,
		CpuDescHandle destCpuHandle) override;

	// Descriptor Creation
	Result<std::unique_ptr<RHIDescriptorAllocator>> CreateDescriptorAllocator(uint32_t numDescriptors) override;
	Result<std::unique_ptr<RHITransientDescriptorAllocator>> CreateTransientDescriptorAllocator(
		uint32_t numDescriptors) override;
	Result<std::unique_ptr<RHIDSVAllocator>> CreateDSVAllocator(uint32_t numDescriptors) override;
	Result<std::unique_ptr<RHIRTVAllocator>> CreateRTVAllocator(uint32_t numDescriptors) override;

	Result<std::unique_ptr<RHIUploadBuffer>> CreateUploadBuffer(size_t capacityInBytes) override;

	// Pipeline Creation
	Result<std::unique_ptr<RHIRootSignature>> CreateRootSignature(const RHIRootSignatureLayout& layout) override;
	Result<std::unique_ptr<RHIPipelineState>> CreatePipelineState(const RHIPipelineDesc& pipelineDesc) override;

	// Sync Creation
	Result<std::unique_ptr<RHIFence>> CreateFence() override;

protected:
	std::unique_ptr<DeviceImpl> m_impl = nullptr;

	DeviceImpl* GetImpl() const;

	friend class DX12CommandList;
	friend class DX12CommandQueue;
	friend class DX12SwapChain;
	friend class DX12Buffer;
	friend class DX12Texture;
	friend class DX12DescriptorAllocator;
	friend class DX12TransientDescriptorAllocator;
	friend class DX12DSVAllocator;
	friend class DX12RTVAllocator;
	friend class DX12RootSignature;
	friend class DX12PipelineState;
	friend class DX12Fence;
	friend class DX12UploadBuffer;
};
