#pragma once

#include "Engine/World/EntityId.h"

class InputState;
class World;
struct SessionServices;

// 責任: アクターに付くデータ1種類の基底になる。
// 役割: 型ごとのコンポーネントを Actor が同一の所有口で保持できるようにする。
//       Tick の既定は何もせず、相対マウスも要求しない。
// 所有: 付いている Actor が持つ。アクターのコピーとともに複製される。
class Component
{
public:
	virtual ~Component() = default;

	// 戻り値は、このコンポーネントが相対マウスを要求しているか。
	virtual bool Tick(
		World& /*world*/,
		EntityId /*self*/,
		const SessionServices& /*session*/,
		const InputState* /*input*/,
		float /*deltaSeconds*/,
		bool /*mouseCaptured*/,
		bool /*keyboardCaptured*/)
	{
		return false;
	}

protected:
	Component() = default;
	Component(const Component&) = default;
	Component& operator=(const Component&) = default;
	Component(Component&&) = default;
	Component& operator=(Component&&) = default;
};
