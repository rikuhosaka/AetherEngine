#include "Engine/Renderer/Test/TextureFileLoaderTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/AssetSystem/Texture/Loader/TextureAssetPath.h"
#include "Engine/AssetSystem/Texture/Loader/TextureFileLoader.h"
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
	TextureImageFormat expectedFormat)
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

	const TextureUploadDesc uploadDesc = BuildTextureUploadDesc(loadResult.value, "LoaderTest");
	const ERHIFormat expectedUploadFormat = expectedFormat == TextureImageFormat::R8G8B8A8_UNORM_SRGB
		? ERHIFormat::R8G8B8A8_UNORM_SRGB
		: ERHIFormat::R8G8B8A8_UNORM;
	if (uploadDesc.format != expectedUploadFormat)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture upload desc format does not match the loaded image for " + absolutePath.string());
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

[[nodiscard]] Result<TextureImageData> LoadRequiredTexture(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	TextureColorSpace colorSpace)
{
	auto resolveResult = ResolveTextureAssetPath(assetsRoot, relativePath);
	if (!resolveResult)
	{
		return MakeFail<TextureImageData>(resolveResult.error.code, resolveResult.error.message);
	}

	TextureFileLoadOptions options{};
	options.colorSpace = colorSpace;
	options.generateMips = true;
	return LoadTextureImageFromFile(resolveResult.value, options);
}

[[nodiscard]] Result<void> ExpectUploadFormat(
	const TextureImageData& image,
	ERHIFormat expectedFormat,
	const char* label)
{
	const TextureUploadDesc uploadDesc = BuildTextureUploadDesc(image, label);
	if (uploadDesc.format != expectedFormat)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture upload desc format does not match the loaded image for " + image.sourcePath.string());
	}

	return MakeOk();
}

[[nodiscard]] Result<void> RunColorSpaceViewTest(const std::filesystem::path& assetsRoot)
{
	constexpr const char* kRelativePath = "Textures/GameIcon.png";

	auto srgbResult = LoadRequiredTexture(assetsRoot, kRelativePath, TextureColorSpace::Srgb);
	if (!srgbResult)
	{
		return MakeFail(srgbResult.error.code, srgbResult.error.message);
	}

	auto linearResult = LoadRequiredTexture(assetsRoot, kRelativePath, TextureColorSpace::Linear);
	if (!linearResult)
	{
		return MakeFail(linearResult.error.code, linearResult.error.message);
	}

	const TextureImageData& srgbImage = srgbResult.value;
	const TextureImageData& linearImage = linearResult.value;
	if (srgbImage.format != TextureImageFormat::R8G8B8A8_UNORM_SRGB
		|| linearImage.format != TextureImageFormat::R8G8B8A8_UNORM)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture color space view produced unexpected formats for " + srgbImage.sourcePath.string());
	}

	if (auto srgbUpload = ExpectUploadFormat(srgbImage, ERHIFormat::R8G8B8A8_UNORM_SRGB, "SrgbView");
		!srgbUpload)
	{
		return srgbUpload;
	}

	if (auto linearUpload = ExpectUploadFormat(linearImage, ERHIFormat::R8G8B8A8_UNORM, "LinearView");
		!linearUpload)
	{
		return linearUpload;
	}

	if (srgbImage.mipCount <= 1 || linearImage.mipCount <= 1 || srgbImage.mips.empty() || linearImage.mips.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture color space view did not generate mipmaps for " + srgbImage.sourcePath.string());
	}

	const TextureImageMip& srgbMip = srgbImage.mips.front();
	const TextureImageMip& linearMip = linearImage.mips.front();
	if (srgbMip.width != linearMip.width
		|| srgbMip.height != linearMip.height
		|| srgbMip.rowPitch != linearMip.rowPitch
		|| srgbMip.pixels != linearMip.pixels)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture color space view changed base mip pixels for " + srgbImage.sourcePath.string());
	}

	return MakeOk();
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

	if (auto colorSpaceResult = RunColorSpaceViewTest(assetsRoot); !colorSpaceResult)
	{
		return colorSpaceResult;
	}

	if (auto gameIconResult = RunLoaderTestCase(
			assetsRoot,
			"Textures/GameIcon.png",
			TextureColorSpace::Srgb,
			TextureImageFormat::R8G8B8A8_UNORM_SRGB);
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
