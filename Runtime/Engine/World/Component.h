#pragma once

#include "Runtime/Engine/World/EntityId.h"

class World;

// 責任: アクターに付くデータ1種類の基底になる。
// 役割: 型ごとのコンポーネントを Actor が同一の所有口で保持できるようにする。
//       付いているアクターの EntityId を持つ。Tick の既定は何もしない。
// 所有: 付いている Actor が持つ。アクターのコピーとともに複製される。
//       EntityId は World::Add が書く。World の複製はスロットの ID を保つので、写した値をそのまま使う。
class Component
{
	friend class World;

public:
	virtual ~Component() = default;

	[[nodiscard]] EntityId GetOwner() const noexcept { return m_owner; }

	virtual void Tick(World& /*world*/, float /*deltaSeconds*/)
	{
	}

protected:
	Component() = default;
	Component(const Component&) = default;
	Component& operator=(const Component&) = default;
	Component(Component&&) = default;
	Component& operator=(Component&&) = default;

private:
	void BindOwner(EntityId owner) noexcept { m_owner = owner; }

	EntityId m_owner{};
};
