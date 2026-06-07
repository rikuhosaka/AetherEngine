#include "Engine/Application/Subsystem/EngineLoop.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Test/ShaderBytecodeLoaderTest.h"
#include "Engine/Renderer/Test/RendererResourceSmokeTest.h"
#include "Engine/Renderer/Test/FbxModelFileLoaderTest.h"
#include "Engine/Renderer/Test/TextureFileLoaderTest.h"
#include "Runtime/RuntimePaths.h"
#include "Runtime/RuntimeSubsystemSetup.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	Logger::Instance().Initialize(ResolveEngineLogPath().string());

#ifdef _DEBUG
	if (auto testResult = RunShaderBytecodeLoaderTests(ResolveCompiledShaderRoot()); !testResult)
	{
		LogResult(testResult, LogCategory::Core);
		return -1;
	}

	if (auto textureTestResult = RunTextureFileLoaderTests(ResolveAssetsDirectory()); !textureTestResult)
	{
		LogResult(textureTestResult, LogCategory::Core);
		return -1;
	}

	if (auto modelTestResult = RunFbxModelFileLoaderTests(ResolveAssetsDirectory()); !modelTestResult)
	{
		LogResult(modelTestResult, LogCategory::Core);
		return -1;
	}

	if (auto smokeLayoutResult = RunRendererResourceSmokeLayoutTests(); !smokeLayoutResult)
	{
		LogResult(smokeLayoutResult, LogCategory::Core);
		return -1;
	}
#endif

	EngineLoopConfig config{};
	config.hInstance = hInstance;
	config.shaderRoot = ResolveShaderRoot();
	config.compiledShaderRoot = ResolveCompiledShaderRoot();
	config.assetsRoot = ResolveAssetsDirectory();
	config.dx12DebugConfigPath = ResolveDx12DebugConfigPath();

	SubsystemRegistry registry;
	RegisterRuntimeSubsystems(registry, config);

	EngineLoop loop(std::move(config), std::move(registry));
	if (auto initResult = loop.Initialize(); !initResult)
	{
		LogResult(initResult, LogCategory::Core);
		return -1;
	}

	return loop.Run();
}
