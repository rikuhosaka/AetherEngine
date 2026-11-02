#pragma once

#include <cstdint>

class World;

// 責任: 現在の World を他サブシステムから参照できるようにする。
// 役割: ポインタと世代を公開する。世代はポインタの変更と、プレイセッションの開始と終了で進む。一時停止では進まない。
// 所有: Engine のメンバ。指している World は所有しない。World::Tick が自分自身のアドレスを書き込む。
struct WorldServices
{
	World* world = nullptr;
	uint32_t generation = 0;

	void Publish(World* next)
	{
		if (world == next)
		{
			return;
		}

		world = next;
		++generation;
	}

	void Advance()
	{
		++generation;
	}
};
