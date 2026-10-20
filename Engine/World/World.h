#pragma once

#include "Engine/World/EntityId.h"
#include "Engine/World/Transform.h"

#include <DirectXMath.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

struct WorldSpawnDesc
{
	std::string name{};
	Transform transform{};
	EntityId parent{};
};

class World
{
public:
	World() = default;
	World(const World& other);
	World& operator=(const World& other);
	World(World&& other) noexcept = default;
	World& operator=(World&& other) noexcept = default;

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
	// Empty when the entity is dead or was spawned without a name.
	[[nodiscard]] std::string_view GetName(EntityId id) const;
	[[nodiscard]] Transform* GetTransform(EntityId id);
	[[nodiscard]] const Transform* GetTransform(EntityId id) const;

	// Fails when the entity is dead or already has this component. T must be copyable.
	template<typename T>
	[[nodiscard]] bool Add(EntityId id, T value)
	{
		if (FindSlot(id) == nullptr)
		{
			return false;
		}

		TypedPool<T>& pool = EnsurePool<T>();
		if (pool.items.size() <= id.Index)
		{
			pool.items.resize(static_cast<size_t>(id.Index) + 1);
		}

		std::optional<T>& slot = pool.items[id.Index];
		if (slot.has_value())
		{
			return false;
		}

		slot = std::move(value);
		return true;
	}

	template<typename T>
	[[nodiscard]] T* Get(EntityId id)
	{
		return const_cast<T*>(std::as_const(*this).Get<T>(id));
	}

	template<typename T>
	[[nodiscard]] const T* Get(EntityId id) const
	{
		if (FindSlot(id) == nullptr)
		{
			return nullptr;
		}

		const TypedPool<T>* pool = FindPool<T>();
		if (pool == nullptr || id.Index >= pool->items.size() || !pool->items[id.Index].has_value())
		{
			return nullptr;
		}

		return &(*pool->items[id.Index]);
	}

	template<typename T>
	[[nodiscard]] bool Remove(EntityId id)
	{
		if (FindSlot(id) == nullptr)
		{
			return false;
		}

		TypedPool<T>* pool = FindPool<T>();
		if (pool == nullptr || id.Index >= pool->items.size() || !pool->items[id.Index].has_value())
		{
			return false;
		}

		pool->items[id.Index].reset();
		return true;
	}

	// Alive entities that have T, in slot order.
	template<typename T, typename Fn>
	void ForEach(Fn&& fn) const
	{
		const TypedPool<T>* pool = FindPool<T>();
		if (pool == nullptr)
		{
			return;
		}

		const uint32_t count = static_cast<uint32_t>(m_slots.size() < pool->items.size() ? m_slots.size() : pool->items.size());
		for (uint32_t index = 0; index < count; ++index)
		{
			if (!m_slots[index].alive || !pool->items[index].has_value())
			{
				continue;
			}

			fn(EntityId{ index, m_slots[index].generation }, *pool->items[index]);
		}
	}

	// Copies slots, generations, parents, names, transforms, free indices, and components.
	// Dead slots stay dead, so entity ids match. A missing component stays missing.
	[[nodiscard]] World Clone() const
	{
		return *this;
	}

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
			fn(id, slot.transform);
		}
	}

	// Alive entities whose parent is missing or dead, in slot order.
	template<typename Fn>
	void ForEachRoot(Fn&& fn) const
	{
		for (uint32_t index = 0; index < m_slots.size(); ++index)
		{
			const Slot& slot = m_slots[index];
			if (!slot.alive || FindSlot(slot.parent) != nullptr)
			{
				continue;
			}

			fn(EntityId{ index, slot.generation });
		}
	}

	// Direct children in slot order. A dead or missing parent yields no children.
	template<typename Fn>
	void ForEachChild(EntityId parent, Fn&& fn) const
	{
		if (FindSlot(parent) == nullptr)
		{
			return;
		}

		for (uint32_t index = 0; index < m_slots.size(); ++index)
		{
			const Slot& slot = m_slots[index];
			if (!slot.alive || slot.parent != parent)
			{
				continue;
			}

			fn(EntityId{ index, slot.generation });
		}
	}

private:
	struct Slot
	{
		bool alive = false;
		uint32_t generation = 0;
		EntityId parent{};
		std::string name{};
		Transform transform{};
	};

	struct ComponentPool
	{
		virtual ~ComponentPool() = default;
		virtual void Clear(uint32_t index) = 0;
		[[nodiscard]] virtual std::unique_ptr<ComponentPool> Clone() const = 0;
	};

	template<typename T>
	struct TypedPool final : ComponentPool
	{
		std::vector<std::optional<T>> items{};

		void Clear(uint32_t index) override
		{
			if (index < items.size())
			{
				items[index].reset();
			}
		}

		[[nodiscard]] std::unique_ptr<ComponentPool> Clone() const override
		{
			return std::make_unique<TypedPool<T>>(*this);
		}
	};

	static constexpr int kMaxParentDepth = 64;

	[[nodiscard]] Slot* FindSlot(EntityId id);
	[[nodiscard]] const Slot* FindSlot(EntityId id) const;
	[[nodiscard]] bool IsUnder(EntityId entity, EntityId ancestor) const;
	void ReleaseSlot(EntityId id);

	template<typename T>
	[[nodiscard]] TypedPool<T>* FindPool()
	{
		const auto it = m_pools.find(std::type_index(typeid(T)));
		if (it == m_pools.end())
		{
			return nullptr;
		}

		return static_cast<TypedPool<T>*>(it->second.get());
	}

	template<typename T>
	[[nodiscard]] const TypedPool<T>* FindPool() const
	{
		const auto it = m_pools.find(std::type_index(typeid(T)));
		if (it == m_pools.end())
		{
			return nullptr;
		}

		return static_cast<const TypedPool<T>*>(it->second.get());
	}

	template<typename T>
	[[nodiscard]] TypedPool<T>& EnsurePool()
	{
		if (TypedPool<T>* existing = FindPool<T>())
		{
			return *existing;
		}

		auto created = std::make_unique<TypedPool<T>>();
		TypedPool<T>* raw = created.get();
		m_pools.emplace(std::type_index(typeid(T)), std::move(created));
		return *raw;
	}

	std::vector<Slot> m_slots{};
	std::vector<uint32_t> m_freeIndices{};
	std::unordered_map<std::type_index, std::unique_ptr<ComponentPool>> m_pools{};
};
