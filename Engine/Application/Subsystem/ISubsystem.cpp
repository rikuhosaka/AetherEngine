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

Result<void> ISubsystem::OnResize(SubsystemContext& /*ctx*/, uint32_t /*width*/, uint32_t /*height*/)
{
	return MakeOk();
}
