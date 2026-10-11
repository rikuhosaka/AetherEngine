#include "Engine/Physics/Test/PhysicsLibraryTest.h"

#include "Engine/Physics/PhysicsLibrary.h"

Result<void> RunPhysicsLibraryTests()
{
	if (auto result = InitializePhysicsLibrary(); !result)
	{
		return result;
	}

	ShutdownPhysicsLibrary();
	return InitializePhysicsLibrary();
}
