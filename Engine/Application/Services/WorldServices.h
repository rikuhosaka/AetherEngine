#pragma once

class World;

// Published by the game module that owns the world. Null before that module registers it.
struct WorldServices
{
	World* world = nullptr;
};
