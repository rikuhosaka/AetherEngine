#pragma once


class RHICommandList;
class RHIUploadBuffer;
class RHITransientDescriptorAllocator;

struct FrameContext
{
    uint32_t frameIndex;

    uint64_t fenceValue;

    RHICommandList* graphicsCommandList;

    RHIUploadBuffer* uploadBuffer;

    RHITransientDescriptorAllocator* transientDescriptors;
};