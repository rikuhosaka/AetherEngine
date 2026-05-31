#include "Runtime/RuntimeSubsystemSetup.h"

#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Subsystems/WindowSubsystem.h"

#include <cstring>
#include <memory>

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineLoopConfig& config)
{
	registry.Register(std::make_unique<WindowSubsystem>(config));
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
