#include "Engine/Renderer/Test/ShaderBytecodeLoaderTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"
#include "Engine/Renderer/ShaderSystem/Loader/ShaderBytecodeFileLoader.h"

Result<void> RunShaderBytecodeLoaderTests(const std::filesystem::path& compiledShaderRoot)
{
	const std::filesystem::path csoPath = compiledShaderRoot / "SimpleVS.cso";

	auto loadResult = LoadShaderBytecodeFromFile(csoPath);
	if (!loadResult)
	{
		return MakeFail(loadResult.error.code, loadResult.error.message);
	}
	if (loadResult.value.Data.empty())
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::ShaderCompileFailed,
			"LoadShaderBytecodeFromFile returned empty bytecode.");
	}

	ShaderBytecodeCache cache(nullptr);

	ShaderCompileDesc desc{};
	desc.FilePath = csoPath;
	desc.Stage = ShaderStage::Vertex;
	desc.EntryPoint = "SimpleVS";

	auto acquireResult = cache.Acquire(desc);
	if (!acquireResult)
	{
		return MakeFail(acquireResult.error.code, acquireResult.error.message);
	}

	const ShaderBytecode* bytecode = cache.GetBytecode(acquireResult.value);
	if (bytecode == nullptr || bytecode->Data.empty())
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::ShaderCompileFailed,
			"ShaderBytecodeCache::Acquire did not store bytecode.");
	}

	const ShaderBytecodeCache::Stats statsAfterFirstAcquire = cache.GetStats();
	if (statsAfterFirstAcquire.Loads != 1 || statsAfterFirstAcquire.Misses != 1)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::RuntimeError,
			"Expected one load and one miss after first Acquire.");
	}

	auto cachedAcquireResult = cache.Acquire(desc);
	if (!cachedAcquireResult || cachedAcquireResult.value != acquireResult.value)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::RuntimeError,
			"Second Acquire did not return cached bytecode handle.");
	}

	const ShaderBytecodeCache::Stats statsAfterSecondAcquire = cache.GetStats();
	if (statsAfterSecondAcquire.Hits != 1 || statsAfterSecondAcquire.Loads != 1)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::RuntimeError,
			"Expected cache hit on second Acquire.");
	}

	LOG_INFO(
		LogCategory::Renderer,
		"ShaderBytecodeLoaderTests passed (" + std::to_string(bytecode->Data.size()) + " bytes).");

	return MakeOk();
}
