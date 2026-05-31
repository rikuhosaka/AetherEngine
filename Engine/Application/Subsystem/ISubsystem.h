#pragma once

#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Core/Log/Result.h"

#include <cstdint>
#include <span>

class SubsystemContext;

class ISubsystem
{
public:
	virtual ~ISubsystem() = default;

	[[nodiscard]] virtual const char* GetName() const = 0;

	[[nodiscard]] virtual std::span<const char* const> GetDependencies() const
	{
		return {};
	}

	[[nodiscard]] virtual SubsystemTickGroup GetTickGroup() const
	{
		return SubsystemTickGroup::Game;
	}

	[[nodiscard]] virtual bool WantsTick() const
	{
		return false;
	}

	virtual Result<void> Initialize(SubsystemContext& ctx) = 0;
	virtual Result<void> PostInitialize(SubsystemContext& ctx);
	virtual void Tick(SubsystemContext& ctx, float deltaSeconds);
	virtual Result<void> OnResize(SubsystemContext& ctx, uint32_t width, uint32_t height);
	virtual void Shutdown(SubsystemContext& ctx) = 0;
};
