#include "Runtime/Engine/Physics/Test/PhysicsLibraryTest.h"

#include "Runtime/Engine/Physics/PhysicsLibrary.h"

Result<void> RunPhysicsLibraryTests()
{
	if (auto result = InitializePhysicsLibrary(); !result)
	{
		return result;
	}

	ShutdownPhysicsLibrary();
	return InitializePhysicsLibrary();
}
