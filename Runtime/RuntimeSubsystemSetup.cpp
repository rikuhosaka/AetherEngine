#include "Runtime/RuntimeSubsystemSetup.h"

#include "Engine/Application/Services/DisplayServices.h"
#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Game/GameSubsystem.h"
#include "Runtime/Subsystems/DisplaySubsystem.h"
#include "Runtime/Subsystems/InputSubsystem.h"
#include "Runtime/Subsystems/RenderSubsystem.h"
#include "Runtime/Subsystems/RHISubsystem.h"
#include "Runtime/Subsystems/WindowSubsystem.h"

#include <cstring>
#include <memory>

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineLoopConfig& config)
{
	registry.Register(std::make_unique<WindowSubsystem>(config));
	registry.Register(CreateRHISubsystem());
	registry.Register(CreateDisplaySubsystem(config));
	registry.Register(CreateRenderSubsystem(config));
	registry.Register(CreateInputSubsystem());
	registry.Register(CreateGameSubsystem());
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

RenderSubsystem* FindRenderSubsystem(const SubsystemRegistry& registry)
{
	for (ISubsystem* subsystem : registry.GetInitOrder())
	{
		if (subsystem != nullptr && std::strcmp(subsystem->GetName(), "Render") == 0)
		{
			return static_cast<RenderSubsystem*>(subsystem);
		}
	}

	return nullptr;
}

RHIServices* FindRHIServices(const SubsystemContext& context)
{
	return context.GetService<RHIServices>();
}

DisplayServices* FindDisplayServices(const SubsystemContext& context)
{
	return context.GetService<DisplayServices>();
}
