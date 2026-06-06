#include "Engine/Renderer/Test/TextureFileLoaderTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Texture/Loader/TextureAssetPath.h"
#include "Engine/Renderer/Texture/Loader/TextureFileLoader.h"
#include "Engine/Renderer/Texture/Loader/TextureImageToUpload.h"

namespace
{
[[nodiscard]] Result<void> ValidateImageData(const TextureImageData& image)
{
	if (image.width == 0 || image.height == 0 || image.mipCount == 0)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture image data has invalid dimensions");
	}

	if (image.mips.size() != image.mipCount)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture image mip count does not match mip data");
	}

	const TextureUploadDesc uploadDesc = BuildTextureUploadDesc(image, "LoaderTest");
	if (uploadDesc.mips.size() != image.mipCount)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture upload desc mip count mismatch");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> RunLoaderTestCase(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	TextureColorSpace colorSpace,
	ERHIFormat expectedFormat)
{
	auto resolveResult = ResolveTextureAssetPath(assetsRoot, relativePath);
	if (!resolveResult)
	{
		if (resolveResult.error.code == ErrorCode::FileNotFound)
		{
			return MakeOk();
		}

		return MakeFail(resolveResult.error.code, resolveResult.error.message);
	}

	const std::filesystem::path& absolutePath = resolveResult.value;

	TextureFileLoadOptions options{};
	options.colorSpace = colorSpace;
	options.generateMips = true;

	auto loadResult = LoadTextureImageFromFile(absolutePath, options);
	if (!loadResult)
	{
		return MakeFail(loadResult.error.code, loadResult.error.message);
	}

	if (loadResult.value.format != expectedFormat)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture file loader produced unexpected format for " + absolutePath.string());
	}

	if (loadResult.value.mipCount <= 1)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture file loader did not generate mipmaps for " + absolutePath.string());
	}

	return ValidateImageData(loadResult.value);
}
} // namespace

Result<void> RunTextureFileLoaderTests(const std::filesystem::path& assetsRoot)
{
	if (assetsRoot.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture file loader tests require a valid assets root");
	}

	if (auto gameIconResult = RunLoaderTestCase(
			assetsRoot,
			"Textures/GameIcon.png",
			TextureColorSpace::Srgb,
			ERHIFormat::R8G8B8A8_UNORM_SRGB);
		!gameIconResult)
	{
		return gameIconResult;
	}

	if (auto traversalResult = ResolveTextureAssetPath(assetsRoot, "../CMakeLists.txt"); traversalResult)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"ResolveTextureAssetPath should reject parent traversal");
	}

	LOG_INFO(LogCategory::Renderer, "TextureFileLoaderTests passed");
	return MakeOk();
}
