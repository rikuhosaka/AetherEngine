#include "Runtime/Engine/Launch/RuntimeSubsystemSetup.h"

#include "Runtime/Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Engine/Engine/Subsystem/Engine.h"
#include "Runtime/Engine/Engine/Subsystem/SceneDocumentSubsystem.h"
#include "Runtime/Engine/Engine/Subsystem/InputSubsystem.h"
#include "Runtime/Engine/Engine/Subsystem/WindowSubsystem.h"
#include "Runtime/Engine/Engine/Subsystem/UiSubsystem.h"

#include <cstring>
#include <memory>

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineLoopConfig& config)
{
	registry.Register(std::make_unique<WindowSubsystem>(config));
	registry.Register(CreateUiSubsystem());
	registry.Register(CreateUiPresentSubsystem());
	registry.Register(CreateInputSubsystem());
	Engine(config).RegisterSubsystems(registry);
	registry.Register(CreateSceneDocumentSubsystem(config.assetsRoot, config.scenePath));
}

WindowSubsystem* FindWindowSubsystem(const SubsystemRegistry& registry)
{
	for (ISubsystem* subsystem : registry.GetInitOrder())
	{
		if (subsystem != nullptr && std::strcmp(subsystem->GetName(), "Window") == 0)
		{
			return static_cast<WindowSubsystem*>(subsystem);
		}
	}

	return nullptr;
}
