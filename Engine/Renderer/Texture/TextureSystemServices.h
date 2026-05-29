#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Texture/TexturePool.h"
#include "Engine/Renderer/Texture/TextureUpload.h"

#include <memory>

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

	[[nodiscard]] Result<TextureHandle> UploadTexture(
		const TextureUploadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList);

	[[nodiscard]] Texture* GetTexture(TextureHandle handle) { return m_pool.Get(handle); }

private:
	TextureSystemServices(RHIDevice* device, RHIDescriptorAllocator* descriptorAllocator);

	TexturePool m_pool{};
	TextureUpload m_upload;
};
