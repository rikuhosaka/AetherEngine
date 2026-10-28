#pragma once

#include <cstdint>

class World;

// Published by World. The same world is published while editing and while playing.
// generation advances when the pointer changes, and when a play session starts or stops.
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

	void Advance()
	{
		++generation;
	}
};
