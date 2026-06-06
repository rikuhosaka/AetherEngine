#include "Engine/Renderer/Texture/TextureSystemServices.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Texture/Loader/TextureAssetPath.h"
#include "Engine/Renderer/Texture/Loader/TextureFileLoader.h"
#include "Engine/Renderer/Texture/Loader/TextureImageToUpload.h"
#include "Engine/RHI/Interface/RHICommandList.h"

namespace
{
[[nodiscard]] const char* ResolveDebugName(
	const TextureLoadDesc& desc,
	std::string& fallbackStorage)
{
	if (desc.debugName != nullptr && desc.debugName[0] != '\0')
	{
		return desc.debugName;
	}

	fallbackStorage = desc.relativePath.stem().string();
	return fallbackStorage.c_str();
}
} // namespace

Result<std::unique_ptr<TextureSystemServices>> TextureSystemServices::Create(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator,
	const std::filesystem::path& assetsRoot)
{
	if (device == nullptr)
	{
		return FailInternal<std::unique_ptr<TextureSystemServices>>(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"TextureSystemServices requires a valid RHIDevice");
	}
	return MakeOk(std::unique_ptr<TextureSystemServices>(
		new TextureSystemServices(device, descriptorAllocator, assetsRoot)));
}

TextureSystemServices::TextureSystemServices(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator,
	std::filesystem::path assetsRoot)
	: m_upload(device, descriptorAllocator)
	, m_assetsRoot(std::move(assetsRoot))
{
}

Result<std::filesystem::path> TextureSystemServices::ResolveTexturePath(
	const std::filesystem::path& relativePath) const
{
	return ResolveTextureAssetPath(m_assetsRoot, relativePath);
}

Result<TextureHandle> TextureSystemServices::UploadTexture(
	const TextureUploadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	auto textureResult = m_upload.CreateTexture(desc, frameContext, commandList);
	if (!textureResult)
	{
		return MakeFail<TextureHandle>(textureResult.error.code, textureResult.error.message);
	}
	return MakeOk(m_pool.Add(std::move(textureResult.value)));
}

Result<TextureHandle> TextureSystemServices::LoadTexture(
	const TextureLoadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	if (desc.relativePath.empty())
	{
		return FailRuntime<TextureHandle>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture load requires a relative asset path");
	}

	auto pathResult = ResolveTexturePath(desc.relativePath);
	if (!pathResult)
	{
		return MakeFail<TextureHandle>(pathResult.error.code, pathResult.error.message);
	}

	TextureFileLoadOptions options{};
	options.colorSpace = desc.colorSpace;
	options.generateMips = desc.generateMips;

	auto imageResult = LoadTextureImageFromFile(pathResult.value, options);
	if (!imageResult)
	{
		return MakeFail<TextureHandle>(imageResult.error.code, imageResult.error.message);
	}

	std::string debugNameStorage{};
	const char* debugName = ResolveDebugName(desc, debugNameStorage);
	const TextureUploadDesc uploadDesc =
		BuildTextureUploadDesc(imageResult.value, debugName);

	return UploadTexture(uploadDesc, frameContext, commandList);
}

Result<TextureHandle> TextureSystemServices::GetOrLoadTexture(
	const TextureLoadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	if (desc.relativePath.empty())
	{
		return FailRuntime<TextureHandle>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture load requires a relative asset path");
	}

	auto pathResult = ResolveTexturePath(desc.relativePath);
	if (!pathResult)
	{
		return MakeFail<TextureHandle>(pathResult.error.code, pathResult.error.message);
	}

	const std::filesystem::path& absolutePath = pathResult.value;
	if (TextureHandle cached = m_loadCache.Find(absolutePath); cached.IsValid())
	{
		return MakeOk(cached);
	}

	auto loadResult = LoadTexture(desc, frameContext, commandList);
	if (!loadResult)
	{
		return loadResult;
	}

	m_loadCache.Store(absolutePath, loadResult.value);
	return loadResult;
}
