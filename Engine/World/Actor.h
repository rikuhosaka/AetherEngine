#pragma once

#include "Engine/World/Component.h"
#include "Engine/World/EntityId.h"
#include "Engine/World/Transform.h"

#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

class World;

// Live scene node stored in a world slot. Identity and lifetime stay on the slot.
struct Actor
{
	Actor() = default;
	Actor(const Actor& other);
	Actor& operator=(const Actor& other);
	Actor(Actor&&) noexcept = default;
	Actor& operator=(Actor&&) noexcept = default;

	EntityId parent{};
	std::string name{};
	Transform transform{};

private:
	friend class World;

	struct ComponentHolder
	{
		virtual ~ComponentHolder() = default;
		[[nodiscard]] virtual std::unique_ptr<ComponentHolder> Clone() const = 0;
	};

	template<typename T>
	struct TypedComponentHolder final : ComponentHolder
	{
		explicit TypedComponentHolder(T stored)
			: value(std::move(stored))
		{
		}

		[[nodiscard]] std::unique_ptr<ComponentHolder> Clone() const override
		{
			return std::make_unique<TypedComponentHolder<T>>(value);
		}

		T value;
	};

	// Fails when this actor already has T. T must derive from Component and be copyable.
	template<typename T>
	[[nodiscard]] bool Add(T value)
	{
		static_assert(std::is_base_of_v<Component, T>);
		static_assert(std::is_copy_constructible_v<T>);

		const std::type_index type = std::type_index(typeid(T));
		if (components.find(type) != components.end())
		{
			return false;
		}

		components.emplace(type, std::make_unique<TypedComponentHolder<T>>(std::move(value)));
		return true;
	}

	template<typename T>
	[[nodiscard]] T* Get()
	{
		return const_cast<T*>(std::as_const(*this).Get<T>());
	}

	template<typename T>
	[[nodiscard]] const T* Get() const
	{
		const auto it = components.find(std::type_index(typeid(T)));
		if (it == components.end())
		{
			return nullptr;
		}

		return &static_cast<const TypedComponentHolder<T>*>(it->second.get())->value;
	}

	template<typename T>
	[[nodiscard]] bool Remove()
	{
		return components.erase(std::type_index(typeid(T))) > 0;
	}

	std::unordered_map<std::type_index, std::unique_ptr<ComponentHolder>> components{};
};

inline Actor::Actor(const Actor& other)
	: parent(other.parent)
	, name(other.name)
	, transform(other.transform)
{
	for (const auto& entry : other.components)
	{
		components.emplace(entry.first, entry.second->Clone());
	}
}

inline Actor& Actor::operator=(const Actor& other)
{
	if (this == &other)
	{
		return *this;
	}

	Actor copy(other);
	*this = std::move(copy);
	return *this;
}
