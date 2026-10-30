#include "Runtime/RuntimeSubsystemSetup.h"

#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Engine/Engine.h"
#include "Engine/SceneDocument/SceneDocumentSubsystem.h"
#include "Engine/Platform/InputSubsystem.h"
#include "Engine/Platform/WindowSubsystem.h"
#include "Engine/Renderer/UiSubsystem.h"

#include <cstring>
#include <memory>

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineLoopConfig& config)
{
	registry.Register(std::make_unique<WindowSubsystem>(config));
	registry.Register(CreateUiSubsystem());
	registry.Register(CreateUiPresentSubsystem());
	registry.Register(CreateInputSubsystem());
	Engine(config).RegisterSubsystems(registry);
	registry.Register(CreateSceneDocumentSubsystem());
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
