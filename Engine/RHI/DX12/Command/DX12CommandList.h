#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHICommandList.h"

#include <memory>

class DX12Device;
class CommandListImpl;
class ResourceImpl;

class DX12CommandList : public RHICommandList
{
public:
	~DX12CommandList() override;
	void Reset() override;
	void Close() override;

	// Pipeline
	void SetPipelineState(const RHIPipelineState* pso) override;
	void SetRootSignature(const RHIRootSignature* rootSig) override;

	// Descriptor / Resource
	void SetDescriptorHeaps(std::span<const RHIDescriptorAllocator*> heaps) override;
	void SetTransientDescriptorHeap(RHITransientDescriptorAllocator* heap) override;
	void CopyBufferRegion(
		RHIBuffer* dstBuffer,
		size_t dstOffset,
		RHIUploadBuffer* srcUpload,
		size_t srcOffset,
		size_t numBytes) override;
	void CopyTextureRegion(
		RHITexture* dstTexture,
		uint32_t dstSubresource,
		RHIUploadBuffer* srcUpload,
		size_t srcOffset,
		uint32_t bytesPerRow,
		uint32_t numRows) override;
	void SetGraphicsRootDescriptorTable(uint32_t index, uint64_t baseDescriptor) override;
	void SetGraphicsRoot32BitConstants(
		uint32_t rootParameterIndex,
		uint32_t num32BitValues,
		const void* data,
		uint32_t destOffsetIn32BitValues = 0) override;

	// Input Assembler
	void IASetVertexBuffers(uint32_t startSlot, std::span<const RHIVertexBuffer*> views) override;
	void IASetIndexBuffer(const RHIIndexBuffer* view) override;
	void IASetPrimitiveTopology(uint32_t topology) override;

	// Rasterizer
	void RSSetViewports(float x, float y, float w, float h) override;
	void RSSetScissorRects(int l, int t, int r, int b) override;

	// Output Merger
	void OMSetRenderTargets(uint32_t numRTs, const RtvHandle rtvs, bool singleHandle, const DsvHandle dsv) override;

	// Clear
	void ClearRenderTargetView(const RtvHandle rtv, const float color[4]) override;
	void ClearDepthStencilView(const DsvHandle dsv, float depth, uint8_t stencil) override;

	// Draw
	void DrawIndexedInstanced(
		uint32_t indexCount,
		uint32_t instanceCount,
		uint32_t startIndex,
		int32_t baseVertex,
		uint32_t startInstance) override;

	// Barrier
	void ResourceBarrier(RHIResource* resource, ERHIResourceState stateAfter) override;

	void BeginDebugEvent(const char* name) override;
	void EndDebugEvent() override;

	[[nodiscard]] bool IsValid() const;

private:
	DX12CommandList(const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12CommandList>> Create(const DX12Device* dxDevice);

	std::unique_ptr<CommandListImpl> m_impl = nullptr;

	CommandListImpl* GetImpl() const;

	friend class DX12Device;
	friend class DX12CommandQueue;
	friend class RHIResource;
	friend class ResourceImpl;
};
