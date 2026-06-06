#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Texture/Cache/TextureLoadCache.h"
#include "Engine/Renderer/Texture/Loader/TextureLoadTypes.h"
#include "Engine/Renderer/Texture/TexturePool.h"
#include "Engine/Renderer/Texture/TextureUpload.h"

#include <filesystem>
#include <memory>

class RHICommandList;
class RHIDevice;
class RHIDescriptorAllocator;

class TextureSystemServices
{
public:
	static Result<std::unique_ptr<TextureSystemServices>> Create(
		RHIDevice* device,
		RHIDescriptorAllocator* descriptorAllocator,
		const std::filesystem::path& assetsRoot);

	[[nodiscard]] const std::filesystem::path& GetAssetsRoot() const noexcept { return m_assetsRoot; }

	[[nodiscard]] Result<std::filesystem::path> ResolveTexturePath(
		const std::filesystem::path& relativePath) const;

	[[nodiscard]] TexturePool& GetPool() noexcept { return m_pool; }
	[[nodiscard]] TextureUpload& GetUpload() noexcept { return m_upload; }

	[[nodiscard]] Result<TextureHandle> UploadTexture(
		const TextureUploadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList);

	[[nodiscard]] Result<TextureHandle> LoadTexture(
		const TextureLoadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList);

	[[nodiscard]] Result<TextureHandle> GetOrLoadTexture(
		const TextureLoadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList);

	[[nodiscard]] Texture* GetTexture(TextureHandle handle) { return m_pool.Get(handle); }

private:
	TextureSystemServices(
		RHIDevice* device,
		RHIDescriptorAllocator* descriptorAllocator,
		std::filesystem::path assetsRoot);

	TexturePool m_pool{};
	TextureUpload m_upload;
	TextureLoadCache m_loadCache{};
	std::filesystem::path m_assetsRoot{};
};
