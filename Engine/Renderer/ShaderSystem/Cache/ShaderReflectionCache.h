#pragma once

#include "Engine/Core/Containers/ResourcePool.h"

#include <unordered_map>
#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheTypes.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"

class IShaderReflectionBackend;
class ShaderBytecodeCache;

class ShaderReflectionCache
{
public:
	struct Stats
	{
		std::uint32_t Hits{ 0 };
		std::uint32_t Misses{ 0 };
		std::uint32_t Reflects{ 0 };
	};

	explicit ShaderReflectionCache(IShaderReflectionBackend* backend);

	[[nodiscard]] ShaderReflectionHandle GetOrReflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage);

	[[nodiscard]] ShaderReflectionHandle GetOrReflect(
		ShaderBytecodeHandle bytecodeHandle,
		ShaderBytecodeCache& bytecodeCache,
		ShaderStage stage);

	[[nodiscard]] const ShaderReflectionData* GetData(ShaderReflectionHandle handle);

	void InvalidateBytecode(std::uint64_t bytecodeFingerprint);
	void InvalidateAll();

	[[nodiscard]] Stats GetStats() const { return m_stats; }

private:
	[[nodiscard]] ShaderReflectionHandle FindCached(
		const ShaderBytecode& bytecode,
		ShaderStage stage) const;

	IShaderReflectionBackend* m_backend{};
	std::unordered_map<std::uint64_t, ShaderReflectionHandle> m_lookup{};
	ResourcePool<ShaderReflectionEntry> m_pool{};
	Stats m_stats{};
};
