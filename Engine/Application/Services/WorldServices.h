#pragma once

class World;

// Published by Scene. The editor world, or the play clone while a session is open.
struct WorldServices
{
	World* world = nullptr;
};
