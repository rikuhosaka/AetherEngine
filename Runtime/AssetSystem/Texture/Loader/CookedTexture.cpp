#include "Runtime/AssetSystem/Texture/Loader/CookedTexture.h"

#include "Runtime/Core/IO/BinaryCursor.h"
#include "Runtime/AssetSystem/Texture/Loader/TextureAssetPath.h"
#include "Runtime/Core/Log/LogMacros.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace
{
constexpr uint32_t kMaxMipCount = 16;
constexpr uint32_t kMaxMipBytes = 64u * 1024u * 1024u;
constexpr uint32_t kMaxStringBytes = 65536;
constexpr char kMagic[4] = { 'A', 'E', 'T', 'X' };

[[nodiscard]] std::filesystem::path AssetRoot(const std::filesystem::path& assetsRoot)
{
	std::filesystem::path root = assetsRoot.lexically_normal();
	if (root.filename().empty())
	{
		root = root.parent_path();
	}

	std::error_code errorCode{};
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(root, errorCode);
	return errorCode ? root : canonical;
}

[[nodiscard]] std::string ToUtf8(const std::filesystem::path& path)
{
	const std::u8string text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

[[nodiscard]] bool ContainsParentReference(const std::filesystem::path& path)
{
	for (const std::filesystem::path& part : path)
	{
		if (part == "..")
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] bool IsColorSpace(uint8_t value)
{
	switch (static_cast<TextureColorSpace>(value))
	{
	case TextureColorSpace::Linear:
	case TextureColorSpace::Srgb:
	case TextureColorSpace::Auto:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] bool IsImageFormat(uint8_t value)
{
	switch (static_cast<TextureImageFormat>(value))
	{
	case TextureImageFormat::R8G8B8A8_UNORM:
	case TextureImageFormat::R8G8B8A8_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] const char* ColorSpaceToken(TextureColorSpace colorSpace)
{
	switch (colorSpace)
	{
	case TextureColorSpace::Linear:
		return "linear";
	case TextureColorSpace::Srgb:
		return "srgb";
	case TextureColorSpace::Auto:
		return "auto";
	default:
		return "auto";
	}
}

[[nodiscard]] bool ImageFits(const TextureImageData& image)
{
	if (image.width == 0 || image.height == 0 || image.mipCount == 0 || image.mipCount > kMaxMipCount
		|| image.mips.size() != image.mipCount)
	{
		return false;
	}

	if (image.mips.front().width != image.width || image.mips.front().height != image.height)
	{
		return false;
	}

	for (const TextureImageMip& mip : image.mips)
	{
		if (mip.width == 0 || mip.height == 0 || mip.rowPitch == 0 || mip.pixels.empty()
			|| mip.pixels.size() > kMaxMipBytes)
		{
			return false;
		}
	}

	return true;
}

void WriteImage(
	BinaryWriter& writer,
	uint64_t sourceHash,
	const TextureFileLoadOptions& options,
	const std::filesystem::path& relativePath,
	const TextureImageData& image)
{
	writer.Bytes(kMagic, 4);
	writer.U32(kCookedTextureVersion);
	writer.U64(sourceHash);
	writer.U8(static_cast<uint8_t>(options.colorSpace));
	writer.U8(options.generateMips ? 1 : 0);
	writer.String(ToUtf8(relativePath.lexically_normal()));
	writer.U32(image.width);
	writer.U32(image.height);
	writer.U32(image.mipCount);
	writer.U8(static_cast<uint8_t>(image.format));

	for (const TextureImageMip& mip : image.mips)
	{
		writer.U32(mip.width);
		writer.U32(mip.height);
		writer.U32(mip.rowPitch);
		writer.U32(static_cast<uint32_t>(mip.pixels.size()));
		writer.Bytes(mip.pixels.data(), mip.pixels.size());
	}
}

[[nodiscard]] bool ReadImage(
	BinaryReader& reader,
	uint64_t sourceHash,
	const TextureFileLoadOptions& options,
	const std::filesystem::path& relativePath,
	TextureImageData& outImage)
{
	char magic[4] = {};
	for (char& character : magic)
	{
		character = static_cast<char>(reader.U8());
	}
	if (!reader.Reading() || std::memcmp(magic, kMagic, 4) != 0 || reader.U32() != kCookedTextureVersion
		|| reader.U64() != sourceHash)
	{
		return false;
	}

	const uint8_t colorSpace = reader.U8();
	const bool generateMips = reader.U8() != 0;
	if (!reader.Reading() || !IsColorSpace(colorSpace)
		|| static_cast<TextureColorSpace>(colorSpace) != options.colorSpace || generateMips != options.generateMips)
	{
		return false;
	}

	const std::string storedRelative = reader.String(kMaxStringBytes);
	if (!reader.Reading() || storedRelative != ToUtf8(relativePath.lexically_normal()))
	{
		return false;
	}

	TextureImageData image{};
	image.width = reader.U32();
	image.height = reader.U32();
	image.mipCount = reader.U32();
	const uint8_t format = reader.U8();
	if (!reader.Reading() || image.width == 0 || image.height == 0 || image.mipCount == 0
		|| image.mipCount > kMaxMipCount || !IsImageFormat(format))
	{
		return false;
	}

	image.format = static_cast<TextureImageFormat>(format);
	image.mips.resize(image.mipCount);
	for (TextureImageMip& mip : image.mips)
	{
		mip.width = reader.U32();
		mip.height = reader.U32();
		mip.rowPitch = reader.U32();
		const uint32_t pixelBytes = reader.U32();
		mip.pixels = reader.Bytes(pixelBytes, kMaxMipBytes);
		if (!reader.Reading() || mip.pixels.empty() || mip.width == 0 || mip.height == 0 || mip.rowPitch == 0)
		{
			return false;
		}
	}

	if (!reader.Exhausted() || image.mips.front().width != image.width || image.mips.front().height != image.height)
	{
		return false;
	}

	outImage = std::move(image);
	return true;
}

[[nodiscard]] Result<std::string> ReadFileBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return FailRuntime<std::string>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Texture file could not be opened");
	}

	return MakeOk(std::string{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() });
}
} // namespace

std::filesystem::path CookedTexturePath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const TextureFileLoadOptions& options)
{
	const std::string extension = std::string(".") + ColorSpaceToken(options.colorSpace)
		+ (options.generateMips ? ".mips" : ".nomips") + ".aetex";
	std::filesystem::path cookedRelative = relativePath.lexically_normal();
	cookedRelative.replace_extension(extension);
	return AssetRoot(assetsRoot).parent_path() / "Intermediate" / "Cooked" / "Textures" / cookedRelative;
}

uint64_t HashTextureSource(std::string_view bytes)
{
	uint64_t hash = 14695981039346656037ull;
	for (const char character : bytes)
	{
		hash ^= static_cast<uint8_t>(character);
		hash *= 1099511628211ull;
	}

	return hash;
}

bool TryReadCookedTexture(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const TextureFileLoadOptions& options,
	const std::filesystem::path& relativePath,
	TextureImageData& outImage)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return false;
	}

	const std::vector<uint8_t> bytes{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>() };
	if (bytes.empty())
	{
		return false;
	}

	BinaryReader reader(bytes.data(), bytes.size());
	return ReadImage(reader, sourceHash, options, relativePath, outImage);
}

Result<void> WriteCookedTexture(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const TextureFileLoadOptions& options,
	const std::filesystem::path& relativePath,
	const TextureImageData& image)
{
	if (relativePath.empty() || ContainsParentReference(relativePath) || !ImageFits(image)
		|| !IsColorSpace(static_cast<uint8_t>(options.colorSpace)))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Cooked texture is missing a relative path or decoded mips");
	}

	std::error_code errorCode{};
	std::filesystem::create_directories(path.parent_path(), errorCode);
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked texture directory could not be created");
	}

	BinaryWriter writer{};
	WriteImage(writer, sourceHash, options, relativePath, image);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked texture file could not be opened");
	}

	file.write(
		reinterpret_cast<const char*>(writer.Data().data()),
		static_cast<std::streamsize>(writer.Data().size()));
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked texture file could not be written");
	}

	return MakeOk();
}

Result<TextureImageData> LoadTextureImage(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const TextureFileLoadOptions& options)
{
	const Result<std::filesystem::path> resolved = ResolveTextureAssetPath(assetsRoot, relativePath);
	if (!resolved)
	{
		return MakeFail<TextureImageData>(resolved.error.code, resolved.error.message);
	}

	const Result<std::string> bytes = ReadFileBytes(resolved.value);
	if (!bytes)
	{
		return MakeFail<TextureImageData>(bytes.error.code, bytes.error.message);
	}

	const uint64_t sourceHash = HashTextureSource(bytes.value);
	const std::filesystem::path cookedPath = CookedTexturePath(assetsRoot, relativePath, options);
	TextureImageData cooked{};
	if (TryReadCookedTexture(cookedPath, sourceHash, options, relativePath, cooked))
	{
		cooked.sourcePath = resolved.value;
		return MakeOk(std::move(cooked));
	}

	Result<TextureImageData> loaded = LoadTextureImageFromFile(resolved.value, options);
	if (!loaded)
	{
		return MakeFail<TextureImageData>(loaded.error.code, loaded.error.message);
	}

	const Result<void> written = WriteCookedTexture(cookedPath, sourceHash, options, relativePath, loaded.value);
	if (!written)
	{
		return MakeFail<TextureImageData>(written.error.code, written.error.message);
	}

	return loaded;
}
