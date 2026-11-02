#pragma once

#include <cstdint>

// 責任: 描画抽出の件数を UI へ渡す。
// 役割: Extract が extractedObjectCount を書き、Stats が読む。
// 所有: Engine のメンバ。Context へは非所有ポインタで登録する。
struct DebugStatsServices
{
	uint32_t extractedObjectCount = 0;
};
