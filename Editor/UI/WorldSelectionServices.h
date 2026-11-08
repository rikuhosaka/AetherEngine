#pragma once

#include "Engine/World/EntityId.h"

#include <cstdint>

struct WorldSelectionServices
{
	EntityId selected{};
	uint32_t worldGeneration = 0;
};
