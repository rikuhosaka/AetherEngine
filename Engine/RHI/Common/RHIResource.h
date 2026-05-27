#pragma once


enum class ERHIResourceState
{
	Common,
	CopyDest,
	CopySource,
	VertexAndConstantBuffer,
	IndexBuffer,
	RenderTarget,
	UnorderedAccess,
	DepthWrite,
	DepthRead,
	NonPixelShaderResource,
	PixelShaderResource
};

enum class ERHIMemoryType
{
	Default,
	Upload,
	Readback
};

struct RHIBufferDesc
{
	size_t Size = 0;
	ERHIMemoryType MemoryType;
};
