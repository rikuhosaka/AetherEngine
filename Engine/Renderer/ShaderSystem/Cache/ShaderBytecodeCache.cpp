#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheHash.h"
#include "Engine/Renderer/ShaderSystem/Compiler/IShaderCompilerBackend.h"
#include "Engine/Renderer/ShaderSystem/Loader/ShaderBytecodeFileLoader.h"

#include <algorithm>
#include <cctype>

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

bool ShaderBytecodeCache::IsPrecompiledShaderPath(const std::filesystem::path& filePath)
{
	std::string extension = filePath.extension().string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return extension == ".cso";
}

Result<ShaderBytecodeHandle> ShaderBytecodeCache::StoreBytecode(
	const ShaderCompileDesc& desc,
	ShaderBytecode bytecode)
{
	if (bytecode.Data.empty())
	{
		return FailRuntime<ShaderBytecodeHandle>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"Shader bytecode is empty.");
	}

	auto entry = std::make_unique<ShaderBytecodeEntry>();
	entry->Desc = desc;
	entry->Bytecode = std::move(bytecode);

	const ShaderBytecodeHandle handle = m_pool.Add(std::move(entry));
	const std::uint64_t lookupKey =
		HashShaderCompileCacheLookupKey(BuildShaderCompileCacheKey(desc));
	m_lookup.emplace(lookupKey, handle);

	return MakeOk(handle);
}

Result<ShaderBytecodeHandle> ShaderBytecodeCache::LoadAndCache(const ShaderCompileDesc& desc)
{
	auto loadResult = LoadShaderBytecodeFromFile(desc.FilePath);
	if (!loadResult)
	{
		return MakeFail<ShaderBytecodeHandle>(
			loadResult.error.code,
			loadResult.error.message);
	}

	auto storeResult = StoreBytecode(desc, std::move(loadResult.value));
	if (!storeResult)
	{
		return storeResult;
	}

	++m_stats.Loads;
	return storeResult;
}

Result<ShaderBytecodeHandle> ShaderBytecodeCache::CompileAndCache(const ShaderCompileDesc& desc)
{
	if (m_backend == nullptr)
	{
		return FailInternal<ShaderBytecodeHandle>(
			LogCategory::Renderer,
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

		return FailRuntime<ShaderBytecodeHandle>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"Shader compile produced empty bytecode.");
	}

	auto storeResult = StoreBytecode(desc, std::move(compileResult.value));
	if (!storeResult)
	{
		return storeResult;
	}

	++m_stats.Compiles;
	return storeResult;
}

Result<ShaderBytecodeHandle> ShaderBytecodeCache::Acquire(const ShaderCompileDesc& desc)
{
	if (ShaderBytecodeHandle cached = FindCached(desc); cached.IsValid())
	{
		++m_stats.Hits;
		return MakeOk(cached);
	}

	++m_stats.Misses;

	if (IsPrecompiledShaderPath(desc.FilePath))
	{
		return LoadAndCache(desc);
	}

	return CompileAndCache(desc);
}

Result<ShaderBytecodeHandle> ShaderBytecodeCache::GetOrCompile(const ShaderCompileDesc& desc)
{
	return Acquire(desc);
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
