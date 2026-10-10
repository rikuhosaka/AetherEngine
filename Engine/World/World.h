#pragma once

#include "Engine/World/EntityId.h"
#include "Engine/World/Renderable.h"
#include "Engine/World/Transform.h"

#include <DirectXMath.h>

#include <cstdint>
#include <vector>

struct WorldSpawnDesc
{
	Transform transform{};
	Renderable renderable{};
	EntityId parent{};
};

class World
{
public:
	EntityId Spawn(WorldSpawnDesc desc);
	void Destroy(EntityId id);

	// Parent must be alive. An invalid parent detaches the child to the root.
	// Local TRS is left unchanged. Returns false for a missing child, a dead parent,
	// a self parent, or a parent that would create a cycle.
	[[nodiscard]] bool SetParent(EntityId child, EntityId parent);

	[[nodiscard]] EntityId GetParent(EntityId id) const;
	// Row-vector product local * parent * ... * root. A dead parent breaks the chain.
	[[nodiscard]] DirectX::XMMATRIX GetWorldMatrix(EntityId id) const;

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
		EntityId parent{};
		Transform transform{};
		Renderable renderable{};
	};

	static constexpr int kMaxParentDepth = 64;

	[[nodiscard]] Slot* FindSlot(EntityId id);
	[[nodiscard]] const Slot* FindSlot(EntityId id) const;
	[[nodiscard]] bool IsUnder(EntityId entity, EntityId ancestor) const;
	void ReleaseSlot(EntityId id);

	std::vector<Slot> m_slots{};
	std::vector<uint32_t> m_freeIndices{};
};
