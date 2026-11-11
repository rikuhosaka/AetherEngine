#pragma once

#include <cstdint>

enum class SessionMode : uint8_t
{
	Editor,
	Playing,
	Paused,
};

// 責任: 依頼されたセッションモードと、反映済みのモードを持つ。
// 役割: UI は requestMode に希望を書く。Engine がそれを開始と終了へ反映し、結果を mode に書く。
//       受け付けられない依頼は requestMode を Editor に戻す。
// 所有: Engine のメンバ。World には渡さない。再生と一時停止の判断は Engine が行う。
struct SessionServices
{
	SessionMode requestMode = SessionMode::Editor;
	SessionMode mode = SessionMode::Editor;
};
