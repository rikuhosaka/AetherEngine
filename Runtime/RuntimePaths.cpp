#include "Runtime/RuntimePaths.h"

#include <Windows.h>

namespace
{
[[nodiscard]] std::filesystem::path FindAncestorContainingRelativePath(
	const std::filesystem::path& start,
	const std::filesystem::path& relativePath)
{
	std::error_code errorCode{};
	std::filesystem::path current = start;
	for (int depth = 0; depth < 16; ++depth)
	{
		if (std::filesystem::exists(current / relativePath, errorCode) && !errorCode)
		{
			return current;
		}

		if (!current.has_parent_path())
		{
			break;
		}

		const std::filesystem::path parent = current.parent_path();
		if (parent == current)
		{
			break;
		}

		current = parent;
	}

	return {};
}

[[nodiscard]] std::filesystem::path CanonicalizeIfPossible(const std::filesystem::path& path)
{
	std::error_code errorCode{};
	const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
	return errorCode ? path : canonicalPath;
}
}

std::filesystem::path ResolveExecutableDirectory()
{
	wchar_t modulePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
	return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path ResolveAssetsDirectory()
{
	const auto projectRoot =
		FindAncestorContainingRelativePath(ResolveExecutableDirectory(), "Assets/Shaders/SimpleVS.hlsl");
	if (projectRoot.empty())
	{
		return {};
	}

	return CanonicalizeIfPossible(projectRoot / "Assets");
}

std::filesystem::path ResolveCompiledShaderRoot()
{
	const auto exeDir = ResolveExecutableDirectory();

	auto tryCompiledShaderRoot = [](const std::filesystem::path& root) -> std::filesystem::path
	{
		std::error_code errorCode{};
		const std::filesystem::path canonicalRoot = CanonicalizeIfPossible(root);
		if (std::filesystem::exists(canonicalRoot / "SimpleVS.cso", errorCode) && !errorCode)
		{
			return canonicalRoot;
		}

		return {};
	};

	if (std::filesystem::path exeAdjacentRoot = tryCompiledShaderRoot(exeDir / "CompiledShaders");
		!exeAdjacentRoot.empty())
	{
		return exeAdjacentRoot;
	}

	const auto buildRoot = FindAncestorContainingRelativePath(
		exeDir,
		"CompiledShaders/SimpleVS.cso");
	if (!buildRoot.empty())
	{
		return CanonicalizeIfPossible(buildRoot / "CompiledShaders");
	}

	return CanonicalizeIfPossible(exeDir / "CompiledShaders");
}

std::filesystem::path ResolveShaderRoot()
{
	const auto projectRoot =
		FindAncestorContainingRelativePath(ResolveExecutableDirectory(), "Assets/Shaders/SimpleVS.hlsl");
	if (!projectRoot.empty())
	{
		return CanonicalizeIfPossible(projectRoot / "Assets" / "Shaders");
	}

	std::error_code errorCode{};
	const std::filesystem::path cwdCandidate = std::filesystem::current_path() / "Assets" / "Shaders";
	if (std::filesystem::exists(cwdCandidate / "SimpleVS.hlsl", errorCode) && !errorCode)
	{
		return CanonicalizeIfPossible(cwdCandidate);
	}

	return {};
}

std::filesystem::path ResolveEngineLogPath()
{
	return ResolveExecutableDirectory() / "Engine.log";
}

std::filesystem::path ResolveDx12DebugConfigPath()
{
	const auto exeDir = ResolveExecutableDirectory();
	const std::filesystem::path candidates[] = {
		exeDir / "dx12_debug.json",
	};
	for (const auto& candidate : candidates)
	{
		std::error_code errorCode{};
		if (std::filesystem::exists(candidate, errorCode) && !errorCode)
		{
			return CanonicalizeIfPossible(candidate);
		}
	}

	const auto projectRoot =
		FindAncestorContainingRelativePath(exeDir, "dx12_debug.json");
	if (!projectRoot.empty())
	{
		return CanonicalizeIfPossible(projectRoot / "dx12_debug.json");
	}

	return exeDir / "dx12_debug.json";
}

std::filesystem::path ResolveConfigRoot()
{
	const auto projectRoot =
		FindAncestorContainingRelativePath(ResolveExecutableDirectory(), "Assets/Shaders/SimpleVS.hlsl");
	if (projectRoot.empty())
	{
		return {};
	}

	return CanonicalizeIfPossible(projectRoot / "Config");
}
