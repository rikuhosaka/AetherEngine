#include "Engine/Renderer/Material/MaterialPool.h"

MaterialHandle MaterialPool::AddMaterial(std::unique_ptr<Material> material)
{
	return m_materials.Add(std::move(material));
}

MaterialInstanceHandle MaterialPool::AddInstance(std::unique_ptr<MaterialInstance> instance)
{
	return m_instances.Add(std::move(instance));
}

Material* MaterialPool::GetMaterial(MaterialHandle handle)
{
	return m_materials.Get(handle);
}

MaterialInstance* MaterialPool::GetInstance(MaterialInstanceHandle handle)
{
	return m_instances.Get(handle);
}

void MaterialPool::RemoveMaterial(MaterialHandle handle)
{
	m_materials.Remove(handle);
}

void MaterialPool::RemoveInstance(MaterialInstanceHandle handle)
{
	m_instances.Remove(handle);
}
