#pragma once

#include "Engine/World/EntityId.h"
#include "Engine/World/Renderable.h"
#include "Engine/World/Transform.h"

#include <cstdint>
#include <vector>

struct WorldSpawnDesc
{
	Transform transform{};
	Renderable renderable{};
};

class World
{
public:
	EntityId Spawn(WorldSpawnDesc desc);
	void Destroy(EntityId id);

	[[nodiscard]] bool IsAlive(EntityId id) const;
	[[nodiscard]] Transform* GetTransform(EntityId id);
	[[nodiscard]] const Transform* GetTransform(EntityId id) const;
	[[nodiscard]] Renderable* GetRenderable(EntityId id);
	[[nodiscard]] const Renderable* GetRenderable(EntityId id) const;

	template<typename Fn>
	void ForEachAlive(Fn&& fn) const
	{
		for (uint32_t index = 0; index < m_slots.size(); ++index)
		{
			const Slot& slot = m_slots[index];
			if (!slot.alive)
			{
				continue;
			}

			const EntityId id{ index, slot.generation };
			fn(id, slot.transform, slot.renderable);
		}
	}

private:
	struct Slot
	{
		bool alive = false;
		uint32_t generation = 0;
		Transform transform{};
		Renderable renderable{};
	};

	[[nodiscard]] Slot* FindSlot(EntityId id);
	[[nodiscard]] const Slot* FindSlot(EntityId id) const;

	std::vector<Slot> m_slots{};
	std::vector<uint32_t> m_freeIndices{};
};
