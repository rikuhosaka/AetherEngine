#pragma once

#include <cstdint>

// Document id for an entity spawned from a world document. 0 means the entity has no document id.
struct DocumentEntityId
{
	uint32_t id = 0;
};
