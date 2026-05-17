#include "Engine/Renderer/ShaderSystem/Internal/FileShaderDiskCache.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
	[[nodiscard]] std::string FormatKeyToken(std::uint64_t value)
	{
		std::ostringstream stream;
		stream << std::hex << std::setw(16) << std::setfill('0') << value;
		return stream.str();
	}
}

FileShaderDiskCache::FileShaderDiskCache(std::filesystem::path cacheRoot)
	: m_cacheRoot(std::move(cacheRoot))
{
	std::error_code errorCode{};
	std::filesystem::create_directories(m_cacheRoot, errorCode);
}

bool FileShaderDiskCache::TryRead(const ShaderCacheKey& key, std::vector<std::byte>& outBytecode) const
{
	const std::filesystem::path cachePath = BuildCachePath(key);
	std::ifstream stream(cachePath, std::ios::binary);
	if (!stream)
	{
		return false;
	}

	stream.seekg(0, std::ios::end);
	const std::streamoff size = stream.tellg();
	if (size <= 0)
	{
		return false;
	}

	outBytecode.resize(static_cast<std::size_t>(size));
	stream.seekg(0, std::ios::beg);
	stream.read(reinterpret_cast<char*>(outBytecode.data()), static_cast<std::streamsize>(outBytecode.size()));
	return static_cast<bool>(stream);
}

bool FileShaderDiskCache::Write(const ShaderCacheKey& key, std::span<const std::byte> bytecode)
{
	if (bytecode.empty())
	{
		return false;
	}

	const std::filesystem::path cachePath = BuildCachePath(key);
	std::error_code errorCode{};
	std::filesystem::create_directories(cachePath.parent_path(), errorCode);

	std::ofstream stream(cachePath, std::ios::binary | std::ios::trunc);
	if (!stream)
	{
		return false;
	}

	stream.write(reinterpret_cast<const char*>(bytecode.data()), static_cast<std::streamsize>(bytecode.size()));
	return static_cast<bool>(stream);
}

std::filesystem::path FileShaderDiskCache::BuildCachePath(const ShaderCacheKey& key) const
{
	std::ostringstream fileName;
	fileName << FormatKeyToken(key.SourceContentFingerprint) << '_'
		<< FormatKeyToken(key.PermutationValue) << '_'
		<< std::hex << std::setw(8) << std::setfill('0') << key.CompilerIdentityToken << '_'
		<< std::hex << std::setw(8) << std::setfill('0') << key.CompilerOptionsPacked << '_'
		<< std::hex << std::setw(8) << std::setfill('0') << key.PipelineSpecToken
		<< ".cso";

	return m_cacheRoot / fileName.str();
}
