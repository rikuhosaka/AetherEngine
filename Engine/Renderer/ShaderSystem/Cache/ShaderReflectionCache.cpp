#include "Engine/Renderer/ShaderSystem/Cache/ShaderReflectionCache.h"

#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheHash.h"
#include "Engine/Renderer/ShaderSystem/Reflection/IShaderReflectionBackend.h"

ShaderReflectionCache::ShaderReflectionCache(IShaderReflectionBackend* backend)
	: m_backend(backend)
{
}

ShaderReflectionHandle ShaderReflectionCache::FindCached(
	const ShaderBytecode& bytecode,
	ShaderStage stage) const
{
	const std::uint64_t lookupKey = HashShaderReflectionCacheLookupKey(
		BuildShaderReflectionCacheKey(bytecode, stage));

	const auto found = m_lookup.find(lookupKey);
	if (found == m_lookup.end())
	{
		return {};
	}

	return found->second;
}

Result<ShaderReflectionHandle> ShaderReflectionCache::GetOrReflect(
	const ShaderBytecode& bytecode,
	ShaderStage stage)
{
	if (ShaderReflectionHandle cached = FindCached(bytecode, stage); cached.IsValid())
	{
		++m_stats.Hits;
		return MakeOk(cached);
	}

	++m_stats.Misses;

	if (m_backend == nullptr)
	{
		return MakeFail<ShaderReflectionHandle>(
			ErrorCode::InvalidArgument,
			"Shader reflection backend is not available.");
	}

	auto reflectResult = m_backend->Reflect(bytecode, stage);
	if (!reflectResult)
	{
		return MakeFail<ShaderReflectionHandle>(
			reflectResult.error.code,
			reflectResult.error.message);
	}

	auto entry = std::make_unique<ShaderReflectionEntry>();
	entry->Data = std::move(reflectResult.value);

	const ShaderReflectionHandle handle = m_pool.Add(std::move(entry));
	const std::uint64_t lookupKey = HashShaderReflectionCacheLookupKey(
		BuildShaderReflectionCacheKey(bytecode, stage));
	m_lookup.emplace(lookupKey, handle);

	++m_stats.Reflects;
	return MakeOk(handle);
}

Result<ShaderReflectionHandle> ShaderReflectionCache::GetOrReflect(
	ShaderBytecodeHandle bytecodeHandle,
	ShaderBytecodeCache& bytecodeCache,
	ShaderStage stage)
{
	const ShaderBytecode* bytecode = bytecodeCache.GetBytecode(bytecodeHandle);
	if (bytecode == nullptr)
	{
		return MakeFail<ShaderReflectionHandle>(
			ErrorCode::InvalidArgument,
			"Invalid shader bytecode handle for reflection.");
	}

	return GetOrReflect(*bytecode, stage);
}

const ShaderReflectionData* ShaderReflectionCache::GetData(ShaderReflectionHandle handle)
{
	const ShaderReflectionEntry* entry = m_pool.Get(handle);
	if (entry == nullptr)
	{
		return nullptr;
	}

	return &entry->Data;
}

void ShaderReflectionCache::InvalidateBytecode(std::uint64_t bytecodeFingerprint)
{
	std::vector<std::uint64_t> keysToRemove{};
	keysToRemove.reserve(m_lookup.size());

	for (const auto& [lookupKey, handle] : m_lookup)
	{
		(void)handle;

		const ShaderStage stages[] = {
			ShaderStage::Vertex,
			ShaderStage::Pixel,
			ShaderStage::Compute,
			ShaderStage::Geometry,
			ShaderStage::Hull,
			ShaderStage::Domain,
		};

		for (const ShaderStage stage : stages)
		{
			const ShaderReflectionCacheKey key{
				.BytecodeFingerprint = bytecodeFingerprint,
				.Stage = stage,
			};
			if (lookupKey == HashShaderReflectionCacheLookupKey(key))
			{
				keysToRemove.push_back(lookupKey);
				break;
			}
		}
	}

	for (const std::uint64_t lookupKey : keysToRemove)
	{
		const auto found = m_lookup.find(lookupKey);
		if (found != m_lookup.end())
		{
			m_pool.Remove(found->second);
			m_lookup.erase(found);
		}
	}
}

void ShaderReflectionCache::InvalidateAll()
{
	for (const auto& [lookupKey, handle] : m_lookup)
	{
		(void)lookupKey;
		m_pool.Remove(handle);
	}
	m_lookup.clear();
}
