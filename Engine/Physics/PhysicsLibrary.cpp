#include "Engine/Physics/PhysicsLibrary.h"

#include "Engine/Core/Log/Result.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

namespace
{
bool g_initialized = false;
} // namespace

Result<void> InitializePhysicsLibrary()
{
	if (g_initialized)
	{
		return MakeOk();
	}

	JPH::RegisterDefaultAllocator();

	if (!JPH::VerifyJoltVersionID())
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Jolt headers do not match the linked library");
	}

	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();
	g_initialized = true;
	return MakeOk();
}

void ShutdownPhysicsLibrary()
{
	if (!g_initialized)
	{
		return;
	}

	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
	g_initialized = false;
}
