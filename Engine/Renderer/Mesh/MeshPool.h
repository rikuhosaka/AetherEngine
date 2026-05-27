#pragma once

#include "Engine/Core/Containers/ResourcePool.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"

class MeshPool
{
public:
	[[nodiscard]] MeshHandle Add(std::unique_ptr<Mesh> mesh);
	[[nodiscard]] Mesh* Get(MeshHandle handle);
	void Remove(MeshHandle handle);

private:
	ResourcePool<Mesh> m_pool{};
};
