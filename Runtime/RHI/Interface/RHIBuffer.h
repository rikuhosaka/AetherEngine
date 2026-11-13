#pragma once

#include "Runtime/RHI/Interface/RHIResource.h"
#include "Runtime/RHI/Common/RHIInput.h"

class RHIBuffer : public RHIResource
{
public:
	virtual void* Map() = 0;
	virtual void Unmap() = 0;
	virtual size_t GetSize() const = 0;
	virtual uint64_t GetGPUAddress() const = 0;
};


class RHIVertexBuffer : public RHIBuffer
{
public:
	virtual uint32_t GetStride() const = 0;
};

class RHIIndexBuffer : public RHIBuffer
{
public:
	virtual IndexFormat GetIndexFormat() const = 0; // ��: DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R32_UINT
};

class RHIConstantBuffer : public RHIBuffer
{
public:
	// �萔�o�b�t�@���L�̋@�\������΂����ɒǉ�
};

class RHIStructuredBuffer : public RHIBuffer
{
public:
	// �\�����o�b�t�@���L�̋@�\������΂����ɒǉ�
};