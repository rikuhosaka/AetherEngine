#include "Runtime/AssetSystem/Texture/Loader/TextureAssetPath.h"

#include "Runtime/Core/IO/Path.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"

#include <system_error>

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
