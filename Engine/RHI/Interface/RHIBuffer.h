#pragma once

#include "Engine/RHI/Interface/RHIResource.h"

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
	virtual IndexFormat GetIndexFormat() const = 0; // 例: DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R32_UINT
};

class RHIConstantBuffer : public RHIBuffer
{
public:
	// 定数バッファ特有の機能があればここに追加
};

class RHIStructuredBuffer : public RHIBuffer
{
public:
	// 構造化バッファ特有の機能があればここに追加
};