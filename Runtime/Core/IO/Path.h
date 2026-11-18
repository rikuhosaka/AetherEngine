#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

[[nodiscard]] inline std::filesystem::path CanonicalizeIfPossible(const std::filesystem::path& path)
{
	std::error_code errorCode{};
	const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
	return errorCode ? path : canonicalPath;
}

// Drop a trailing separator, then weakly_canonical when that succeeds.
[[nodiscard]] inline std::filesystem::path CanonicalDirectory(const std::filesystem::path& path)
{
	std::filesystem::path directory = path.lexically_normal();
	if (directory.filename().empty())
	{
		directory = directory.parent_path();
	}

	return CanonicalizeIfPossible(directory);
}

[[nodiscard]] inline bool ContainsParentReference(const std::filesystem::path& path)
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

[[nodiscard]] inline bool IsPathUnderRoot(
	const std::filesystem::path& candidate,
	const std::filesystem::path& root)
{
	std::error_code errorCode{};
	const std::filesystem::path relative = std::filesystem::relative(candidate, root, errorCode);
	if (errorCode || relative.empty())
	{
		return candidate == root;
	}

	return !ContainsParentReference(relative);
}

[[nodiscard]] inline std::string PathToUtf8(const std::filesystem::path& path)
{
	const std::u8string text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

[[nodiscard]] inline std::filesystem::path PathFromUtf8(std::string_view text)
{
	const std::u8string bytes(reinterpret_cast<const char8_t*>(text.data()), text.size());
	return std::filesystem::path(bytes);
}
