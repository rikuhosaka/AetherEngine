#include "Runtime/Engine/Launch/EngineLoop.h"
#include "Runtime/Core/Log/Log.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Physics/PhysicsLibrary.h"
#include "Runtime/Engine/Launch/DebugStartupTests.h"
#include "Runtime/Engine/Launch/RuntimePaths.h"
#include "Runtime/Engine/Launch/RuntimeSubsystemSetup.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	Logger::Instance().Initialize(ResolveEngineLogPath().string());

	if (auto physicsResult = InitializePhysicsLibrary(); !physicsResult)
	{
		LogResult(physicsResult, LogCategory::Physics);
		return -1;
	}

	if (auto testResult = RunDebugStartupTests(ResolveCompiledShaderRoot(), ResolveAssetsDirectory()); !testResult)
	{
		LogResult(testResult, LogCategory::Core);
		ShutdownPhysicsLibrary();
		return -1;
	}

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
