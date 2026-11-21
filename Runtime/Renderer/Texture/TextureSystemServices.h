#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/Texture/Cache/TextureLoadCache.h"
#include "Runtime/Renderer/Texture/TexturePool.h"
#include "Runtime/Renderer/Texture/TextureUpload.h"

#include <memory>
#include <string_view>

class RHICommandList;
class RHIDevice;
class RHIDescriptorAllocator;

class TextureSystemServices
{
public:
	static Result<std::unique_ptr<TextureSystemServices>> Create(
		RHIDevice* device,
		RHIDescriptorAllocator* descriptorAllocator);

	[[nodiscard]] TexturePool& GetPool() noexcept { return m_pool; }
	[[nodiscard]] TextureUpload& GetUpload() noexcept { return m_upload; }

	// cacheKey reuses a previous upload when non-empty. An empty key always creates a new texture.
	[[nodiscard]] Result<TextureHandle> UploadTexture(
		const TextureUploadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList,
		std::string_view cacheKey = {});

	[[nodiscard]] Texture* GetTexture(TextureHandle handle) { return m_pool.Get(handle); }

private:
	TextureSystemServices(
		RHIDevice* device,
		RHIDescriptorAllocator* descriptorAllocator);

	TexturePool m_pool{};
	TextureUpload m_upload;
	TextureLoadCache m_loadCache{};
};
