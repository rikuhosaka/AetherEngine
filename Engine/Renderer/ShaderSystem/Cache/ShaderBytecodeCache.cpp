#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"

#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheHash.h"
#include "Engine/Renderer/ShaderSystem/Compiler/IShaderCompilerBackend.h"

ShaderBytecodeCache::ShaderBytecodeCache(IShaderCompilerBackend* backend)
	: m_backend(backend)
{
}

ShaderBytecodeHandle ShaderBytecodeCache::FindCached(const ShaderCompileDesc& desc) const
{
	const std::uint64_t lookupKey =
		HashShaderCompileCacheLookupKey(BuildShaderCompileCacheKey(desc));

	const auto found = m_lookup.find(lookupKey);
	if (found == m_lookup.end())
	{
		return {};
	}

	return found->second;
}

Result<ShaderBytecodeHandle> ShaderBytecodeCache::GetOrCompile(const ShaderCompileDesc& desc)
{
	if (ShaderBytecodeHandle cached = FindCached(desc); cached.IsValid())
	{
		++m_stats.Hits;
		return MakeOk(cached);
	}

	++m_stats.Misses;

	if (m_backend == nullptr)
	{
		return MakeFail<ShaderBytecodeHandle>(
			ErrorCode::InvalidArgument,
			"Shader compiler backend is not available.");
	}

	auto compileResult = m_backend->Compile(desc);
	if (!compileResult || compileResult.value.Data.empty())
	{
		if (!compileResult)
		{
			return MakeFail<ShaderBytecodeHandle>(
				compileResult.error.code,
				compileResult.error.message);
		}

		return MakeFail<ShaderBytecodeHandle>(
			ErrorCode::ShaderCompileFailed,
			"Shader compile produced empty bytecode.");
	}

	auto entry = std::make_unique<ShaderBytecodeEntry>();
	entry->Desc = desc;
	entry->Bytecode = std::move(compileResult.value);

	const ShaderBytecodeHandle handle = m_pool.Add(std::move(entry));
	const std::uint64_t lookupKey =
		HashShaderCompileCacheLookupKey(BuildShaderCompileCacheKey(desc));
	m_lookup.emplace(lookupKey, handle);

	++m_stats.Compiles;
	return MakeOk(handle);
}

const ShaderBytecode* ShaderBytecodeCache::GetBytecode(ShaderBytecodeHandle handle)
{
	const ShaderBytecodeEntry* entry = m_pool.Get(handle);
	if (entry == nullptr)
	{
		return nullptr;
	}

	return &entry->Bytecode;
}

bool ShaderBytecodeCache::TryGetBytecode(const ShaderCompileDesc& desc, ShaderBytecode& outBytecode)
{
	const ShaderBytecodeHandle handle = FindCached(desc);
	if (!handle.IsValid())
	{
		return false;
	}

	const ShaderBytecode* bytecode = GetBytecode(handle);
	if (bytecode == nullptr)
	{
		return false;
	}

	outBytecode = *bytecode;
	++m_stats.Hits;
	return true;
}

void ShaderBytecodeCache::Invalidate(const ShaderCompileDesc& desc)
{
	const std::uint64_t lookupKey =
		HashShaderCompileCacheLookupKey(BuildShaderCompileCacheKey(desc));

	const auto found = m_lookup.find(lookupKey);
	if (found == m_lookup.end())
	{
		return;
	}

	m_pool.Remove(found->second);
	m_lookup.erase(found);
}

void ShaderBytecodeCache::InvalidateAll()
{
	for (const auto& [lookupKey, handle] : m_lookup)
	{
		(void)lookupKey;
		m_pool.Remove(handle);
	}
	m_lookup.clear();
}
