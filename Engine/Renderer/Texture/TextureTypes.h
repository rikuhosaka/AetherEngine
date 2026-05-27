#pragma once

#include "Engine/Core/Handle/Handle.h"
#include "Engine/RHI/Common/RHIDescriptor.h"
#include "Engine/RHI/Common/RHIFormat.h"
#include "Engine/RHI/Common/RHITexture.h"
#include "Engine/RHI/Interface/RHITexture.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

struct TextureMipData
{
	std::span<const std::byte> pixels{};
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t rowPitch = 0;
};

struct TextureUploadDesc
{
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mipLevels = 1;
	ERHITextureUsage usage = ERHITextureUsage::ShaderResource;
	ERHIFormat format = ERHIFormat::R8G8B8A8_UNORM;
	std::vector<TextureMipData> mips{};
};

struct Texture
{
	std::unique_ptr<RHITexture> resource{};
	RHITextureDesc desc{};
	CbvSrvUavHandle srv{};
	bool srvValid = false;
};

using TextureHandle = Handle<Texture>;
