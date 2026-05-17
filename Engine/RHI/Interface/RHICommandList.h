#pragma once

#include "Engine/RHI/Common/RHIDescriptor.h"
#include "Engine/RHI/Common/RHIResource.h"
class RHIResource;
class RHIVertexBuffer;
class RHIIndexBuffer;
class RHITexture;
class RHIPipelineState;
class RHIRootSignature;
class RHIDescriptorAllocator;
class RHIDSVAllocator;
class RHIRTVAllocator;

class RHICommandList
{
public:
    virtual ~RHICommandList() = default;

    virtual void Reset() = 0;
    virtual void Close() = 0;

    // Pipeline
    virtual void SetPipelineState(const RHIPipelineState* pso) = 0;
    virtual void SetRootSignature(const RHIRootSignature* rootSig) = 0;

    // Descriptor / Resource
    virtual void SetDescriptorHeaps(std::span<const RHIDescriptorAllocator*> heaps) = 0;
	virtual void SetGraphicsRootDescriptorTable(uint32_t index, uint64_t baseDescriptor) = 0;

    // Input Assembler
    virtual void IASetVertexBuffers(uint32_t startSlot, std::span<const RHIVertexBuffer*> views) = 0;
    virtual void IASetIndexBuffer(const RHIIndexBuffer* view) = 0;
    virtual void IASetPrimitiveTopology(uint32_t topology) = 0;

    // Rasterizer
    virtual void RSSetViewports(float x, float y, float w, float h) = 0;
    virtual void RSSetScissorRects(int l, int t, int r, int b) = 0;

    // Output Merger
    virtual void OMSetRenderTargets(uint32_t numRTs, const RtvHandle rtvs, bool singleHandle, const DsvHandle dsv) = 0;

    // Clear
    virtual void ClearRenderTargetView(const RtvHandle rtv, const float color[4]) = 0;
    virtual void ClearDepthStencilView(const DsvHandle dsv, float depth, uint8_t stencil) = 0;

    // Draw
    virtual void DrawIndexedInstanced(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t startIndex,
        int32_t baseVertex,
        uint32_t startInstance) = 0;

    // Barrier
	virtual void ResourceBarrier(RHIResource* resource, ERHIResourceState stateAfter) = 0;

protected:
	RHICommandList() = default;
};
