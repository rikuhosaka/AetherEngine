#include "Runtime/Engine/Launch/EngineLoop.h"
#include "Runtime/Core/Log/Log.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Physics/PhysicsLibrary.h"
#include "Runtime/Engine/Launch/DebugStartupTests.h"
#include "Runtime/Engine/Launch/RuntimePaths.h"
#include "Runtime/Engine/Launch/RuntimeSubsystemSetup.h"

#include <shellapi.h>

#include <string_view>

namespace
{
#ifdef _DEBUG
[[nodiscard]] bool CommandLineHasFlag(const wchar_t* flag)
{
	int argc = 0;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == nullptr)
	{
		return false;
	}

	bool found = false;
	for (int index = 1; index < argc; ++index)
	{
		if (std::wstring_view(argv[index]) == flag)
		{
			found = true;
			break;
		}
	}

	LocalFree(argv);
	return found;
}
#endif

[[nodiscard]] bool ShouldRunStartupTests()
{
#ifdef _DEBUG
	return !CommandLineHasFlag(L"-skip-tests");
#else
	return false;
#endif
}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	Logger::Instance().Initialize(ResolveEngineLogPath().string());

	if (auto physicsResult = InitializePhysicsLibrary(); !physicsResult)
	{
		LogResult(physicsResult, LogCategory::Physics);
		return -1;
	}

	if (ShouldRunStartupTests())
	{
		if (auto testResult = RunDebugStartupTests(ResolveCompiledShaderRoot(), ResolveAssetsDirectory()); !testResult)
		{
			LogResult(testResult, LogCategory::Core);
			ShutdownPhysicsLibrary();
			return -1;
		}
	}
#ifdef _DEBUG
	else
	{
		LOG_INFO(LogCategory::Core, "Startup tests skipped (-skip-tests)");
	}
#endif

	EngineLoopConfig config{};
	config.hInstance = hInstance;
	config.shaderRoot = ResolveShaderRoot();
	config.compiledShaderRoot = ResolveCompiledShaderRoot();
	config.assetsRoot = ResolveAssetsDirectory();
	config.scenePath = config.assetsRoot / "Scenes" / "PhysicsDrop.json";
	config.dx12DebugConfigPath = ResolveDx12DebugConfigPath();

	SubsystemRegistry registry;
	RegisterRuntimeSubsystems(registry, config);

	EngineLoop loop(std::move(config), std::move(registry));
	if (auto initResult = loop.Initialize(); !initResult)
	{
		LogResult(initResult, LogCategory::Core);
		ShutdownPhysicsLibrary();
		return -1;
	}

	const int exitCode = loop.Run();
	ShutdownPhysicsLibrary();
	return exitCode;
}
