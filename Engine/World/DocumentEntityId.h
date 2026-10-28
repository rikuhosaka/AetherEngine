#pragma once

#include "Engine/World/Component.h"

#include <cstdint>

// Document id for an entity spawned from a world document. 0 means the entity has no document id.
struct DocumentEntityId : Component
{
	DocumentEntityId() = default;
	explicit DocumentEntityId(uint32_t documentId)
		: id(documentId)
	{
	}

	uint32_t id = 0;
};
