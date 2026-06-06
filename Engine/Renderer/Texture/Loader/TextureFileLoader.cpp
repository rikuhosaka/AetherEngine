#include "Engine/Renderer/Texture/Loader/TextureFileLoader.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"

#include <DirectXTex.h>

#include <Windows.h>
#include <objbase.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <mutex>
#include <string>

namespace
{
enum class TextureFileKind
{
	Wic,
	Dds,
	Unsupported,
};

void EnsureComInitializedForWic()
{
	static std::once_flag comInitFlag;
	std::call_once(comInitFlag, []() {
		const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		{
			LOG_ERROR(LogCategory::Asset,
				"CoInitializeEx failed for WIC texture loading (HRESULT=0x"
				+ std::format("{:08X}", static_cast<unsigned long>(hr))
				+ ")");
		}
	});
}

[[nodiscard]] std::string ToLowerAscii(std::string value)
{
	std::ranges::transform(value, value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

[[nodiscard]] TextureFileKind ClassifyFileKind(const std::filesystem::path& path)
{
	const std::string extension = ToLowerAscii(path.extension().string());
	if (extension == ".dds")
	{
		return TextureFileKind::Dds;
	}

	if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp")
	{
		return TextureFileKind::Wic;
	}

	return TextureFileKind::Unsupported;
}

[[nodiscard]] bool ContainsLinearTextureHint(std::string_view stem)
{
	static constexpr std::string_view kLinearHints[] = {
		"normal",
		"norm",
		"spec",
		"specular",
		"rough",
		"roughness",
		"metal",
		"metallic",
		"ao",
		"orm",
		"height",
		"disp",
		"bump",
	};

	const std::string lowerStem = ToLowerAscii(std::string(stem));
	for (const std::string_view hint : kLinearHints)
	{
		if (lowerStem.find(hint) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] TextureColorSpace ResolveColorSpace(
	const std::filesystem::path& path,
	TextureColorSpace requested)
{
	switch (requested)
	{
	case TextureColorSpace::Linear:
		return TextureColorSpace::Linear;
	case TextureColorSpace::Srgb:
		return TextureColorSpace::Srgb;
	case TextureColorSpace::Auto:
		return ContainsLinearTextureHint(path.stem().string())
			? TextureColorSpace::Linear
			: TextureColorSpace::Srgb;
	}

	return TextureColorSpace::Srgb;
}

[[nodiscard]] DXGI_FORMAT ToDxgiTargetFormat(TextureColorSpace colorSpace)
{
	return colorSpace == TextureColorSpace::Srgb
		? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
		: DXGI_FORMAT_R8G8B8A8_UNORM;
}

[[nodiscard]] ERHIFormat ToRhiTargetFormat(TextureColorSpace colorSpace)
{
	return colorSpace == TextureColorSpace::Srgb
		? ERHIFormat::R8G8B8A8_UNORM_SRGB
		: ERHIFormat::R8G8B8A8_UNORM;
}

[[nodiscard]] Result<void> ValidateLoadedMetadata(const DirectX::TexMetadata& metadata)
{
	if (metadata.width == 0 || metadata.height == 0)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture file has invalid dimensions");
	}

	if (metadata.IsCubemap())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Cubemap textures are not supported");
	}

	if (metadata.IsVolumemap() || metadata.depth > 1)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Volume textures are not supported");
	}

	if (metadata.arraySize != 1)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture arrays are not supported");
	}

	return MakeOk();
}

[[nodiscard]] Result<DirectX::ScratchImage> LoadScratchImageFromFile(
	const std::filesystem::path& absolutePath,
	TextureFileKind fileKind)
{
	DirectX::ScratchImage scratch{};
	const std::wstring widePath = absolutePath.wstring();
	HRESULT hr = E_FAIL;

	switch (fileKind)
	{
	case TextureFileKind::Wic:
		EnsureComInitializedForWic();
		hr = DirectX::LoadFromWICFile(
			widePath.c_str(),
			DirectX::WIC_FLAGS_FORCE_RGB,
			nullptr,
			scratch);
		break;

	case TextureFileKind::Dds:
		hr = DirectX::LoadFromDDSFile(
			widePath.c_str(),
			DirectX::DDS_FLAGS_NONE,
			nullptr,
			scratch);
		break;

	default:
		return FailRuntime<DirectX::ScratchImage>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Unsupported texture file extension: " + absolutePath.extension().string());
	}

	if (FAILED(hr))
	{
		return FailRuntime<DirectX::ScratchImage>(
			LogCategory::Asset,
			ErrorCode::ResourceCreationFailed,
			"Failed to decode texture file (HRESULT=0x"
			+ std::format("{:08X}", static_cast<unsigned long>(hr))
			+ "): "
			+ absolutePath.string());
	}

	if (auto validation = ValidateLoadedMetadata(scratch.GetMetadata()); !validation)
	{
		return MakeFail<DirectX::ScratchImage>(validation.error.code, validation.error.message);
	}

	return MakeOk(std::move(scratch));
}

[[nodiscard]] Result<DirectX::ScratchImage> ConvertToTargetFormat(
	const DirectX::ScratchImage& source,
	DXGI_FORMAT targetFormat)
{
	DirectX::ScratchImage converted{};
	const HRESULT hr = DirectX::Convert(
		source.GetImages(),
		source.GetImageCount(),
		source.GetMetadata(),
		targetFormat,
		DirectX::TEX_FILTER_DEFAULT,
		0.5f,
		converted);
	if (FAILED(hr))
	{
		return FailRuntime<DirectX::ScratchImage>(
			LogCategory::Asset,
			ErrorCode::ResourceCreationFailed,
			"Failed to convert texture to target format");
	}

	return MakeOk(std::move(converted));
}

[[nodiscard]] Result<DirectX::ScratchImage> GenerateMipChain(
	DirectX::ScratchImage source,
	bool generateMips)
{
	if (!generateMips)
	{
		return MakeOk(std::move(source));
	}

	DirectX::ScratchImage mipChain{};
	const HRESULT hr = DirectX::GenerateMipMaps(
		source.GetImages(),
		source.GetImageCount(),
		source.GetMetadata(),
		DirectX::TEX_FILTER_DEFAULT,
		0,
		mipChain);
	if (FAILED(hr))
	{
		return FailRuntime<DirectX::ScratchImage>(
			LogCategory::Asset,
			ErrorCode::ResourceCreationFailed,
			"Failed to generate texture mipmaps");
	}

	return MakeOk(std::move(mipChain));
}

[[nodiscard]] Result<TextureImageData> ScratchImageToTextureImageData(
	DirectX::ScratchImage&& scratch,
	const std::filesystem::path& sourcePath,
	ERHIFormat format)
{
	const DirectX::TexMetadata& metadata = scratch.GetMetadata();
	if (metadata.mipLevels == 0)
	{
		return FailRuntime<TextureImageData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture file produced no mip levels");
	}

	TextureImageData image{};
	image.width = static_cast<uint32_t>(metadata.width);
	image.height = static_cast<uint32_t>(metadata.height);
	image.mipCount = static_cast<uint32_t>(metadata.mipLevels);
	image.format = format;
	image.sourcePath = sourcePath;
	image.mips.resize(image.mipCount);

	for (uint32_t mipIndex = 0; mipIndex < image.mipCount; ++mipIndex)
	{
		const DirectX::Image* mipImage = scratch.GetImage(mipIndex, 0, 0);
		if (mipImage == nullptr || mipImage->pixels == nullptr)
		{
			return FailRuntime<TextureImageData>(
				LogCategory::Asset,
				ErrorCode::ResourceCreationFailed,
				"Failed to read decoded texture mip data");
		}

		TextureImageMip& mip = image.mips[mipIndex];
		mip.width = static_cast<uint32_t>(mipImage->width);
		mip.height = static_cast<uint32_t>(mipImage->height);
		mip.rowPitch = static_cast<uint32_t>(mipImage->rowPitch);
		mip.pixels.resize(mipImage->slicePitch);
		std::memcpy(mip.pixels.data(), mipImage->pixels, mipImage->slicePitch);
	}

	return MakeOk(std::move(image));
}
} // namespace

Result<TextureImageData> LoadTextureImageFromFile(
	const std::filesystem::path& absolutePath,
	const TextureFileLoadOptions& options)
{
	if (absolutePath.empty())
	{
		return FailRuntime<TextureImageData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture file path is empty");
	}

	std::error_code errorCode{};
	if (!std::filesystem::exists(absolutePath, errorCode) || errorCode)
	{
		return FailRuntime<TextureImageData>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Texture file not found: " + absolutePath.string());
	}

	const TextureFileKind fileKind = ClassifyFileKind(absolutePath);
	if (fileKind == TextureFileKind::Unsupported)
	{
		return FailRuntime<TextureImageData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Unsupported texture file extension: " + absolutePath.extension().string());
	}

	auto loadedResult = LoadScratchImageFromFile(absolutePath, fileKind);
	if (!loadedResult)
	{
		return MakeFail<TextureImageData>(loadedResult.error.code, loadedResult.error.message);
	}

	const TextureColorSpace resolvedColorSpace =
		ResolveColorSpace(absolutePath, options.colorSpace);
	const DXGI_FORMAT targetFormat = ToDxgiTargetFormat(resolvedColorSpace);
	const ERHIFormat rhiFormat = ToRhiTargetFormat(resolvedColorSpace);

	auto convertedResult = ConvertToTargetFormat(loadedResult.value, targetFormat);
	if (!convertedResult)
	{
		return MakeFail<TextureImageData>(convertedResult.error.code, convertedResult.error.message);
	}

	auto mipResult = GenerateMipChain(std::move(convertedResult.value), options.generateMips);
	if (!mipResult)
	{
		return MakeFail<TextureImageData>(mipResult.error.code, mipResult.error.message);
	}

	return ScratchImageToTextureImageData(
		std::move(mipResult.value),
		absolutePath,
		rhiFormat);
}
