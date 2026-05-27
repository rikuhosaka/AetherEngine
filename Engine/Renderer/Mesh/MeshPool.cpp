#include "Engine/Renderer/Mesh/MeshPool.h"

MeshHandle MeshPool::Add(std::unique_ptr<Mesh> mesh)
{
	return m_pool.Add(std::move(mesh));
}

Mesh* MeshPool::Get(MeshHandle handle)
{
	return m_pool.Get(handle);
}

void MeshPool::Remove(MeshHandle handle)
{
	m_pool.Remove(handle);
}
