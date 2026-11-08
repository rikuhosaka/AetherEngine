#pragma once

#include <cstdint>

enum class SessionMode : uint8_t
{
	Editor,
	Playing,
	Paused,
};

// 責任: 現在のセッションモードを持つ。
// 役割: UI が希望モードを書き、Engine がそれを World の開始と終了へ反映する。
// 所有: Engine のメンバ。World は参照を受け取るだけで所有しない。
struct SessionServices
{
	SessionMode mode = SessionMode::Editor;
};
