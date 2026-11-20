#pragma once

#include <algorithm>
#include <limits>

struct MeshBounds
{
	float minX = 0.0f;
	float minY = 0.0f;
	float minZ = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	float maxZ = 0.0f;
};

inline void ResetBounds(MeshBounds& bounds)
{
	const float maxValue = (std::numeric_limits<float>::max)();
	bounds.minX = bounds.minY = bounds.minZ = maxValue;
	bounds.maxX = bounds.maxY = bounds.maxZ = std::numeric_limits<float>::lowest();
}

inline void ExpandBounds(MeshBounds& bounds, const float position[3])
{
	bounds.minX = (std::min)(bounds.minX, position[0]);
	bounds.minY = (std::min)(bounds.minY, position[1]);
	bounds.minZ = (std::min)(bounds.minZ, position[2]);
	bounds.maxX = (std::max)(bounds.maxX, position[0]);
	bounds.maxY = (std::max)(bounds.maxY, position[1]);
	bounds.maxZ = (std::max)(bounds.maxZ, position[2]);
}

[[nodiscard]] inline bool BoundsAreValid(const MeshBounds& bounds)
{
	return bounds.minX <= bounds.maxX && bounds.minY <= bounds.maxY && bounds.minZ <= bounds.maxZ;
}
