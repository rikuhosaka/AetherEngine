#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/Result.h"
#include "Runtime/RuntimeSubsystemSetup.h"
#include "Runtime/Subsystems/WindowSubsystem.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	EngineLoopConfig config{};
	config.hInstance = hInstance;

	SubsystemRegistry registry;
	RegisterRuntimeSubsystems(registry, config);

	SubsystemContext context;
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
	if (windowSubsystem == nullptr)
	{
		registry.ShutdownAll(context);
		return -1;
	}

	while (windowSubsystem->ProcessMessages())
	{
	}

	registry.ShutdownAll(context);
	return 0;
}
