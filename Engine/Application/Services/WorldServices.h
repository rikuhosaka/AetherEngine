#pragma once

#include <cstdint>

class World;

// Published by World. The editor world, or the play clone while a session is open.
// generation changes only when world starts pointing at a different object.
struct WorldServices
{
	World* world = nullptr;
	uint32_t generation = 0;

	void Publish(World* next)
	{
		if (world == next)
		{
			return;
		}

		world = next;
		++generation;
	}
};
