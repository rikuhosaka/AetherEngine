#include "Engine/Renderer/Texture/Loader/TextureAssetPath.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"

#include <system_error>

namespace
{
[[nodiscard]] std::filesystem::path CanonicalizeIfPossible(const std::filesystem::path& path)
{
	std::error_code errorCode{};
	const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
	return errorCode ? path : canonicalPath;
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

[[nodiscard]] bool IsPathUnderRoot(
	const std::filesystem::path& candidate,
	const std::filesystem::path& root)
{
	std::error_code errorCode{};
	const std::filesystem::path relative =
		std::filesystem::relative(candidate, root, errorCode);
	if (errorCode || relative.empty())
	{
		return candidate == root;
	}

	return !ContainsParentReference(relative);
}
} // namespace

Result<std::filesystem::path> ResolveTextureAssetPath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath)
{
	if (assetsRoot.empty())
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Assets root path is empty");
	}

	if (relativePath.empty())
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture relative path is empty");
	}

	if (relativePath.is_absolute())
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture path must be relative to Assets/: " + relativePath.string());
	}

	if (ContainsParentReference(relativePath))
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture path must not traverse outside Assets/: " + relativePath.string());
	}

	const std::filesystem::path canonicalRoot = CanonicalizeIfPossible(assetsRoot);
	const std::filesystem::path absolutePath =
		CanonicalizeIfPossible(canonicalRoot / relativePath);

	if (!IsPathUnderRoot(absolutePath, canonicalRoot))
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture path must stay under Assets/: " + relativePath.string());
	}

	std::error_code errorCode{};
	if (!std::filesystem::exists(absolutePath, errorCode) || errorCode)
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Texture file not found: " + absolutePath.string());
	}

	return MakeOk(absolutePath);
}
