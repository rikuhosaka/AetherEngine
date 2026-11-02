#pragma once

// 責任: アクターに付くデータ1種類の基底になる。
// 役割: 型ごとのコンポーネントを Actor が同一の所有口で保持できるようにする。
// 所有: 付いている Actor が持つ。アクターのコピーとともに複製される。
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
