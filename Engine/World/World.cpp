#include "Engine/World/World.h"

#include <utility>

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
	slot.transform = desc.transform;
	slot.renderable = std::move(desc.renderable);
	return EntityId{ index, slot.generation };
}

void World::Destroy(EntityId id)
{
	Slot* slot = FindSlot(id);
	if (slot == nullptr)
	{
		return;
	}

	slot->alive = false;
	slot->transform = Transform{};
	slot->renderable = Renderable{};
	++slot->generation;
	m_freeIndices.push_back(id.Index);
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
