#include "Engine/World/World.h"

#include "Engine/Math/Transform.h"

#include <array>
#include <utility>
#include <vector>

namespace
{
[[nodiscard]] DirectX::XMMATRIX ToLocalMatrix(const Transform& transform)
{
	return Aether::Math::TRS(
		DirectX::XMLoadFloat3(&transform.position),
		DirectX::XMLoadFloat4(&transform.rotation),
		DirectX::XMLoadFloat3(&transform.scale));
}
} // namespace

World::Slot* World::FindSlot(EntityId id)
{
	return const_cast<Slot*>(std::as_const(*this).FindSlot(id));
}

const World::Slot* World::FindSlot(EntityId id) const
{
	if (!id.IsValid() || id.Index >= m_slots.size())
	{
		return nullptr;
	}

	const Slot& slot = m_slots[id.Index];
	if (!slot.alive || slot.generation != id.Generation)
	{
		return nullptr;
	}

	return &slot;
}

EntityId World::Spawn(WorldSpawnDesc desc)
{
	uint32_t index = 0;
	if (!m_freeIndices.empty())
	{
		index = m_freeIndices.back();
		m_freeIndices.pop_back();
	}
	else
	{
		index = static_cast<uint32_t>(m_slots.size());
		Slot slot{};
		slot.generation = 1;
		m_slots.push_back(std::move(slot));
	}

	Slot& slot = m_slots[index];
	slot.alive = true;
	slot.parent = desc.parent;
	slot.transform = desc.transform;
	slot.renderable = std::move(desc.renderable);
	return EntityId{ index, slot.generation };
}

bool World::IsUnder(EntityId entity, EntityId ancestor) const
{
	EntityId current = entity;
	for (int depth = 0; depth < kMaxParentDepth; ++depth)
	{
		const Slot* slot = FindSlot(current);
		if (slot == nullptr || !slot->parent.IsValid())
		{
			return false;
		}

		if (slot->parent == ancestor)
		{
			return true;
		}

		current = slot->parent;
	}

	return false;
}

void World::ReleaseSlot(EntityId id)
{
	Slot* slot = FindSlot(id);
	if (slot == nullptr)
	{
		return;
	}

	slot->alive = false;
	slot->parent = {};
	slot->transform = Transform{};
	slot->renderable = Renderable{};
	++slot->generation;
	m_freeIndices.push_back(id.Index);
}

void World::Destroy(EntityId id)
{
	if (FindSlot(id) == nullptr)
	{
		return;
	}

	std::vector<EntityId> toDestroy;
	toDestroy.push_back(id);
	for (uint32_t index = 0; index < m_slots.size(); ++index)
	{
		const Slot& slot = m_slots[index];
		if (!slot.alive)
		{
			continue;
		}

		const EntityId entity{ index, slot.generation };
		if (entity == id)
		{
			continue;
		}

		if (IsUnder(entity, id))
		{
			toDestroy.push_back(entity);
		}
	}

	for (const EntityId entity : toDestroy)
	{
		ReleaseSlot(entity);
	}
}

bool World::SetParent(EntityId child, EntityId parent)
{
	Slot* childSlot = FindSlot(child);
	if (childSlot == nullptr)
	{
		return false;
	}

	if (!parent.IsValid())
	{
		childSlot->parent = {};
		return true;
	}

	if (parent == child || FindSlot(parent) == nullptr)
	{
		return false;
	}

	EntityId current = parent;
	for (int depth = 0; depth < kMaxParentDepth; ++depth)
	{
		if (current == child)
		{
			return false;
		}

		const Slot* slot = FindSlot(current);
		if (slot == nullptr || !slot->parent.IsValid())
		{
			childSlot->parent = parent;
			return true;
		}

		current = slot->parent;
	}

	return false;
}

EntityId World::GetParent(EntityId id) const
{
	const Slot* slot = FindSlot(id);
	return slot == nullptr ? EntityId{} : slot->parent;
}

DirectX::XMMATRIX World::GetWorldMatrix(EntityId id) const
{
	std::array<DirectX::XMMATRIX, kMaxParentDepth> localMatrices{};
	int count = 0;
	EntityId current = id;
	for (; count < kMaxParentDepth; ++count)
	{
		const Slot* slot = FindSlot(current);
		if (slot == nullptr)
		{
			break;
		}

		localMatrices[static_cast<size_t>(count)] = ToLocalMatrix(slot->transform);
		if (!slot->parent.IsValid())
		{
			++count;
			break;
		}

		current = slot->parent;
	}

	DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
	for (int index = count - 1; index >= 0; --index)
	{
		world = DirectX::XMMatrixMultiply(localMatrices[static_cast<size_t>(index)], world);
	}

	return world;
}

bool World::IsAlive(EntityId id) const
{
	return FindSlot(id) != nullptr;
}

Transform* World::GetTransform(EntityId id)
{
	Slot* slot = FindSlot(id);
	return slot == nullptr ? nullptr : &slot->transform;
}

const Transform* World::GetTransform(EntityId id) const
{
	const Slot* slot = FindSlot(id);
	return slot == nullptr ? nullptr : &slot->transform;
}

Renderable* World::GetRenderable(EntityId id)
{
	Slot* slot = FindSlot(id);
	return slot == nullptr ? nullptr : &slot->renderable;
}

const Renderable* World::GetRenderable(EntityId id) const
{
	const Slot* slot = FindSlot(id);
	return slot == nullptr ? nullptr : &slot->renderable;
}
