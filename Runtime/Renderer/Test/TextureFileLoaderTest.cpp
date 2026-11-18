#include "Runtime/Renderer/Test/TextureFileLoaderTest.h"

#include "Runtime/Core/Hash/Fnv.h"
#include "Runtime/Core/IO/BinaryFile.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/AssetSystem/Texture/Loader/CookedTexture.h"
#include "Runtime/AssetSystem/Texture/Loader/TextureAssetPath.h"
#include "Runtime/AssetSystem/Texture/Loader/TextureFileLoader.h"
#include "Runtime/Renderer/Texture/Loader/TextureImageToUpload.h"

#include <fstream>
#include <system_error>

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

[[nodiscard]] bool SameImage(const TextureImageData& left, const TextureImageData& right)
{
	if (left.sourcePath != right.sourcePath || left.width != right.width || left.height != right.height
		|| left.mipCount != right.mipCount || left.format != right.format || left.mips.size() != right.mips.size())
	{
		return false;
	}

	for (size_t mipIndex = 0; mipIndex < left.mips.size(); ++mipIndex)
	{
		const TextureImageMip& leftMip = left.mips[mipIndex];
		const TextureImageMip& rightMip = right.mips[mipIndex];
		if (leftMip.width != rightMip.width || leftMip.height != rightMip.height || leftMip.rowPitch != rightMip.rowPitch
			|| leftMip.pixels != rightMip.pixels)
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool CookedFileStartsWithMagic(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	char magic[4] = {};
	file.read(magic, 4);
	return file.gcount() == 4 && magic[0] == 'A' && magic[1] == 'E' && magic[2] == 'T' && magic[3] == 'X';
}

[[nodiscard]] Result<void> TestCookedTextureCache(const std::filesystem::path& assetsRoot)
{
	const std::filesystem::path tempRoot = std::filesystem::temp_directory_path() / "AetherTextureCook" / "Assets";
	std::error_code errorCode{};
	std::filesystem::remove_all(tempRoot.parent_path(), errorCode);
	std::filesystem::create_directories(tempRoot / "Textures", errorCode);
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked texture test directory could not be created");
	}

	{
		std::ofstream badFile(tempRoot / "Textures" / "bad.png", std::ios::binary | std::ios::trunc);
		if (!badFile)
		{
			std::filesystem::remove_all(tempRoot.parent_path(), errorCode);
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::FileNotFound,
				"Invalid texture test file could not be written");
		}
		badFile << "not a png";
	}

	TextureFileLoadOptions badOptions{};
	const Result<TextureImageData> badLoad = LoadTextureImage(tempRoot, "Textures/bad.png", badOptions);
	if (badLoad || std::filesystem::exists(CookedTexturePath(tempRoot, "Textures/bad.png", badOptions), errorCode))
	{
		std::filesystem::remove_all(tempRoot.parent_path(), errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"An invalid texture should fail before a cooked texture is written");
	}
	std::filesystem::remove_all(tempRoot.parent_path(), errorCode);

	constexpr const char* kRelativePath = "Textures/GameIcon.png";
	const Result<std::filesystem::path> resolved = ResolveTextureAssetPath(assetsRoot, kRelativePath);
	if (!resolved)
	{
		return MakeOk();
	}

	const Result<std::string> sourceBytes = ReadBinaryFile(resolved.value);
	if (!sourceBytes)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Texture file could not be opened");
	}

	TextureFileLoadOptions srgbOptions{};
	srgbOptions.colorSpace = TextureColorSpace::Srgb;
	srgbOptions.generateMips = true;
	TextureFileLoadOptions linearOptions{};
	linearOptions.colorSpace = TextureColorSpace::Linear;
	linearOptions.generateMips = true;

	const Result<TextureImageData> imported = LoadTextureImageFromFile(resolved.value, srgbOptions);
	const Result<TextureImageData> cooked = imported ? LoadTextureImage(assetsRoot, kRelativePath, srgbOptions) : imported;
	const Result<TextureImageData> cookedAgain = cooked
		? LoadTextureImage(assetsRoot, kRelativePath, srgbOptions)
		: cooked;
	const Result<TextureImageData> linearCooked = cookedAgain
		? LoadTextureImage(assetsRoot, kRelativePath, linearOptions)
		: cookedAgain;
	const std::filesystem::path srgbPath = CookedTexturePath(assetsRoot, kRelativePath, srgbOptions);
	const std::filesystem::path linearPath = CookedTexturePath(assetsRoot, kRelativePath, linearOptions);
	if (!imported || !cooked || !cookedAgain || !linearCooked || srgbPath == linearPath
		|| !std::filesystem::exists(srgbPath, errorCode) || !std::filesystem::exists(linearPath, errorCode)
		|| !SameImage(imported.value, cooked.value) || !SameImage(cooked.value, cookedAgain.value)
		|| linearCooked.value.format != TextureImageFormat::R8G8B8A8_UNORM
		|| cooked.value.format != TextureImageFormat::R8G8B8A8_UNORM_SRGB)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			imported && cooked && cookedAgain && linearCooked
				? "Cooked textures should match the decode and keep separate color-space files"
				: (imported
					? (cooked ? (cookedAgain ? linearCooked.error.message : cookedAgain.error.message) : cooked.error.message)
					: imported.error.message));
	}

	TextureImageData patched{};
	const bool readPatched = TryReadCookedTexture(
		srgbPath,
		Fnv1a64(sourceBytes.value),
		srgbOptions,
		kRelativePath,
		patched) && !patched.mips.empty() && !patched.mips.front().pixels.empty();
	std::byte patchedByte{};
	if (readPatched)
	{
		patched.mips.front().pixels.front() = static_cast<std::byte>(
			static_cast<uint8_t>(patched.mips.front().pixels.front()) ^ 0x5Au);
		patchedByte = patched.mips.front().pixels.front();
	}
	const Result<void> writtenPatch = readPatched
		? WriteCookedTexture(srgbPath, Fnv1a64(sourceBytes.value), srgbOptions, kRelativePath, patched)
		: FailRuntime(LogCategory::Asset, ErrorCode::InvalidArgument, "Cooked texture could not be read back");
	const Result<TextureImageData> patchedLoad = writtenPatch
		? LoadTextureImage(assetsRoot, kRelativePath, srgbOptions)
		: MakeFail<TextureImageData>(writtenPatch.error.code, writtenPatch.error.message);
	if (!writtenPatch || !patchedLoad || patchedLoad.value.mips.empty() || patchedLoad.value.mips.front().pixels.empty()
		|| patchedLoad.value.mips.front().pixels.front() != patchedByte)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			writtenPatch && patchedLoad
				? "A matching cooked texture should be reused"
				: (writtenPatch ? patchedLoad.error.message : writtenPatch.error.message));
	}

	{
		std::ofstream corrupt(srgbPath, std::ios::binary | std::ios::trunc);
		corrupt << "XXXX";
	}
	const Result<TextureImageData> recovered = LoadTextureImage(assetsRoot, kRelativePath, srgbOptions);
	if (!recovered || !SameImage(imported.value, recovered.value) || !CookedFileStartsWithMagic(srgbPath))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			recovered
				? "A corrupt cooked texture should be rebuilt from the source image"
				: recovered.error.message);
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

	if (auto cookedResult = TestCookedTextureCache(assetsRoot); !cookedResult)
	{
		return cookedResult;
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
