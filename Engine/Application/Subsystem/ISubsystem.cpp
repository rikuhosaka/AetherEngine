#include "Engine/Application/Subsystem/ISubsystem.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/Result.h"

Result<void> ISubsystem::PostInitialize(SubsystemContext& /*ctx*/)
{
	return MakeOk();
}

void ISubsystem::Tick(SubsystemContext& /*ctx*/, float /*deltaSeconds*/)
{
}

void ISubsystem::FixedTick(SubsystemContext& /*ctx*/, float /*fixedDeltaSeconds*/)
{
}

Result<void> ISubsystem::OnResize(SubsystemContext& /*ctx*/, uint32_t /*width*/, uint32_t /*height*/)
{
	return MakeOk();
}
