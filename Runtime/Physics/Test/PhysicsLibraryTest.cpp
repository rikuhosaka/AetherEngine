#include "Runtime/Physics/Test/PhysicsLibraryTest.h"

#include "Runtime/Physics/PhysicsLibrary.h"

Result<void> RunPhysicsLibraryTests()
{
	if (auto result = InitializePhysicsLibrary(); !result)
	{
		return result;
	}

	ShutdownPhysicsLibrary();
	return InitializePhysicsLibrary();
}
