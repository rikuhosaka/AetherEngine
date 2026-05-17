#pragma once


enum class ERHITextureUsage
{
	RenderTarget,
	DepthStencil,
	ShaderResource,
	UnorderedAccess
};

struct RHITextureDesc
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t MipLevels = 1;
	uint32_t ArraySize = 1;
	uint32_t SampleCount = 1;
	uint32_t SampleQuality = 0;
	ERHITextureUsage Usage;
};