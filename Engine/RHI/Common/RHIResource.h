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
	PixelShaderResource,
	Present
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

	// Optional label for GPU debug tools (PIX, RenderDoc). UTF-8.
	const char* DebugName = nullptr;
};
