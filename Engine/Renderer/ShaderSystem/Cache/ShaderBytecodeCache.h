#pragma once

#include "Engine/Core/Containers/ResourcePool.h"
#include "Engine/Core/Log/Result.h"

#include <filesystem>
#include <unordered_map>
#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheTypes.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"

class IShaderCompilerBackend;

class ShaderBytecodeCache
{
public:
	struct Stats
	{
		std::uint32_t Hits{ 0 };
		std::uint32_t Misses{ 0 };
		std::uint32_t Compiles{ 0 };
		std::uint32_t Loads{ 0 };
	};

	explicit ShaderBytecodeCache(IShaderCompilerBackend* backend);

	[[nodiscard]] Result<ShaderBytecodeHandle> Acquire(const ShaderCompileDesc& desc);

	[[nodiscard]] Result<ShaderBytecodeHandle> GetOrCompile(const ShaderCompileDesc& desc);

	[[nodiscard]] const ShaderBytecode* GetBytecode(ShaderBytecodeHandle handle);

	[[nodiscard]] bool TryGetBytecode(const ShaderCompileDesc& desc, ShaderBytecode& outBytecode);

	void Invalidate(const ShaderCompileDesc& desc);
	void InvalidateAll();

	[[nodiscard]] Stats GetStats() const { return m_stats; }

private:
	[[nodiscard]] ShaderBytecodeHandle FindCached(const ShaderCompileDesc& desc) const;

	[[nodiscard]] Result<ShaderBytecodeHandle> CompileAndCache(const ShaderCompileDesc& desc);

	[[nodiscard]] Result<ShaderBytecodeHandle> LoadAndCache(const ShaderCompileDesc& desc);

	[[nodiscard]] Result<ShaderBytecodeHandle> StoreBytecode(
		const ShaderCompileDesc& desc,
		ShaderBytecode bytecode);

	[[nodiscard]] static bool IsPrecompiledShaderPath(const std::filesystem::path& filePath);

	IShaderCompilerBackend* m_backend{};
	std::unordered_map<std::uint64_t, ShaderBytecodeHandle> m_lookup{};
	ResourcePool<ShaderBytecodeEntry> m_pool{};
	Stats m_stats{};
};
