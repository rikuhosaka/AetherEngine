#pragma once

// Data owned by one actor. Copied with that actor.
class Component
{
public:
	virtual ~Component() = default;

protected:
	Component() = default;
	Component(const Component&) = default;
	Component& operator=(const Component&) = default;
	Component(Component&&) = default;
	Component& operator=(Component&&) = default;
};
