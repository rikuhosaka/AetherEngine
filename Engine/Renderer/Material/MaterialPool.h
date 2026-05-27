#pragma once

#include "Engine/Core/Containers/ResourcePool.h"
#include "Engine/Renderer/Material/MaterialTypes.h"

class MaterialPool
{
public:
	[[nodiscard]] MaterialHandle AddMaterial(std::unique_ptr<Material> material);
	[[nodiscard]] MaterialInstanceHandle AddInstance(std::unique_ptr<MaterialInstance> instance);

	[[nodiscard]] Material* GetMaterial(MaterialHandle handle);
	[[nodiscard]] MaterialInstance* GetInstance(MaterialInstanceHandle handle);

	void RemoveMaterial(MaterialHandle handle);
	void RemoveInstance(MaterialInstanceHandle handle);

private:
	ResourcePool<Material> m_materials{};
	ResourcePool<MaterialInstance> m_instances{};
};
