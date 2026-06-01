#pragma once

#include "Engine/RHI/Common/RHIFormat.h"

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
	ERHITextureUsage Usage = ERHITextureUsage::ShaderResource;
	ERHIFormat Format = ERHIFormat::R8G8B8A8_UNORM;

	// Optional label for GPU debug tools (PIX, RenderDoc). UTF-8.
	const char* DebugName = nullptr;
};