#include "Engine/Renderer/ShaderSystem/ShaderSystemFactory.h"

#include "Engine/Renderer/ShaderSystem/Internal/DxcShaderCompilerBackend.h"
#include "Engine/Renderer/ShaderSystem/Internal/FileShaderDiskCache.h"
#include "Engine/Renderer/ShaderSystem/Internal/ShaderSystemImpl.h"
#include "Engine/Renderer/ShaderSystem/ShaderCatalog.h"

#include <cstdlib>

namespace
{
	[[nodiscard]] std::filesystem::path ResolveDxcFromSdk()
	{
		std::vector<std::filesystem::path> candidates{};

#if defined(AETHER_DXC_EXECUTABLE)
		candidates.emplace_back(AETHER_DXC_EXECUTABLE);
#endif

		char* pathFromEnvironment = nullptr;
		size_t environmentLength = 0;
		if (_dupenv_s(&pathFromEnvironment, &environmentLength, "AETHER_DXC_EXECUTABLE") == 0 && pathFromEnvironment != nullptr)
		{
			candidates.emplace_back(pathFromEnvironment);
			free(pathFromEnvironment);
		}

		const std::filesystem::path sdkRoot = "C:/Program Files (x86)/Windows Kits/10/bin";
		if (std::filesystem::exists(sdkRoot))
		{
			for (const std::filesystem::directory_entry& versionDirectory :
				std::filesystem::directory_iterator(sdkRoot))
			{
				const std::filesystem::path dxcPath = versionDirectory.path() / "x64/dxc.exe";
				if (std::filesystem::exists(dxcPath))
				{
					candidates.push_back(dxcPath);
				}
			}
		}

		candidates.emplace_back("dxc.exe");

		for (const std::filesystem::path& candidate : candidates)
		{
			if (!candidate.empty() && std::filesystem::exists(candidate))
			{
				return std::filesystem::weakly_canonical(candidate);
			}
		}

		return {};
	}
}

std::unique_ptr<ShaderCatalog> CreateDefaultShaderCatalog(const ShaderSystemSettings& settings)
{
	return std::make_unique<ShaderCatalog>(settings.AssetsRoot);
}

std::unique_ptr<IShaderSystem> CreateShaderSystem(
	const ShaderSystemSettings& settings,
	const ShaderCatalog& catalog)
{
	ShaderSystemSettings resolvedSettings = settings;
	if (resolvedSettings.DxcExecutable.empty())
	{
		resolvedSettings.DxcExecutable = ResolveDefaultDxcExecutable();
	}

	if (resolvedSettings.ScratchDirectory.empty())
	{
		resolvedSettings.ScratchDirectory = resolvedSettings.DiskCacheDirectory / "Scratch";
	}

	auto compiler = std::make_unique<DxcShaderCompilerBackend>(
		resolvedSettings.DxcExecutable,
		resolvedSettings.ScratchDirectory);
	auto diskCache = std::make_unique<FileShaderDiskCache>(resolvedSettings.DiskCacheDirectory);

	return std::make_unique<ShaderSystemImpl>(
		std::move(resolvedSettings),
		catalog,
		std::move(compiler),
		std::move(diskCache));
}

std::filesystem::path ResolveDefaultDxcExecutable()
{
	return ResolveDxcFromSdk();
}
