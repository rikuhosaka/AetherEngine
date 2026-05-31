#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/FrameClock.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/Result.h"
#include "Runtime/RuntimePaths.h"
#include "Runtime/RuntimeSubsystemSetup.h"
#include "Runtime/Subsystems/RenderSubsystem.h"
#include "Runtime/Subsystems/WindowSubsystem.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	EngineLoopConfig config{};
	config.hInstance = hInstance;
	config.shaderRoot = ResolveCompiledShaderRoot();

	SubsystemRegistry registry;
	RegisterRuntimeSubsystems(registry, config);

	SubsystemContext context;
	context.RegisterService(&registry);

	if (auto initResult = registry.InitializeAll(context); !initResult)
	{
		LogResult(initResult, LogCategory::Core);
		return -1;
	}

	if (auto postInitResult = registry.PostInitializeAll(context); !postInitResult)
	{
		LogResult(postInitResult, LogCategory::Core);
		registry.ShutdownAll(context);
		return -1;
	}

	WindowSubsystem* windowSubsystem = FindWindowSubsystem(registry);
	RenderSubsystem* renderSubsystem = FindRenderSubsystem(registry);
	if (windowSubsystem == nullptr || renderSubsystem == nullptr)
	{
		registry.ShutdownAll(context);
		return -1;
	}

	FrameClock clock;
	clock.Reset();

	while (windowSubsystem->ProcessMessages())
	{
		(void)clock.Tick(config.maxDeltaSeconds);

		if (auto renderResult = renderSubsystem->RenderFrame(context); !renderResult)
		{
			LogResult(renderResult, LogCategory::Renderer);
			break;
		}
	}

	registry.ShutdownAll(context);
	return 0;
}
