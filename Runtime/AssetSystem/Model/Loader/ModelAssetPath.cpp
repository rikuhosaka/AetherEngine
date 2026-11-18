#include "Runtime/AssetSystem/Model/Loader/ModelAssetPath.h"

#include "Runtime/Core/IO/Path.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"

#include <cctype>
#include <system_error>

namespace
{
[[nodiscard]] bool HasModelExtension(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	for (char& character : extension)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension == ".fbx" || extension == ".gltf" || extension == ".glb";
}
} // namespace

Result<std::filesystem::path> ResolveModelAssetPath(
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
			"Model relative path is empty");
	}

	if (relativePath.is_absolute())
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model path must be relative to Assets/: " + relativePath.string());
	}

	if (ContainsParentReference(relativePath))
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model path must not traverse outside Assets/: " + relativePath.string());
	}

	if (!HasModelExtension(relativePath))
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model path must use the .fbx, .gltf, or .glb extension: " + relativePath.string());
	}

	const std::filesystem::path canonicalRoot = CanonicalizeIfPossible(assetsRoot);
	const std::filesystem::path absolutePath =
		CanonicalizeIfPossible(canonicalRoot / relativePath);

	if (!IsPathUnderRoot(absolutePath, canonicalRoot))
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model path must stay under Assets/: " + relativePath.string());
	}

	std::error_code errorCode{};
	if (!std::filesystem::exists(absolutePath, errorCode) || errorCode)
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Model file not found: " + absolutePath.string());
	}

	return MakeOk(absolutePath);
}
