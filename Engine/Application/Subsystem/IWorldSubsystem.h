#pragma once

#include "Engine/World/SessionServices.h"
#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Core/Log/Result.h"

#include <DirectXMath.h>

class InputState;
class World;

// 責任: 1つの World の、プレイセッション中だけ存在する処理を定義する。
// 役割: 開始、終了、固定 Tick、可変 Tick の口。呼び出し順は Tick グループの昇順、同順は登録順。
// 所有: その World が unique_ptr で持つ。World 自体とシーン文書は所有しない。
//       初期化と Tick は所有元の World が行う。コピーでは写さず、ムーブで所有が移る。
class IWorldSubsystem
{
public:
	virtual ~IWorldSubsystem() = default;

	[[nodiscard]] virtual const char* GetName() const = 0;

	[[nodiscard]] virtual SubsystemTickGroup GetTickGroup() const
	{
		return SubsystemTickGroup::Game;
	}

	virtual Result<void> OnBeginPlay(
		World& /*world*/,
		const DirectX::XMFLOAT3* /*cameraPosition*/,
		const DirectX::XMFLOAT3* /*cameraForward*/)
	{
		return MakeOk();
	}

	virtual void OnEndPlay(World& /*world*/)
	{
	}

	virtual void FixedTick(World& /*world*/, const SessionServices& /*session*/, float /*fixedDeltaSeconds*/)
	{
	}

	// 相対マウスが必要なら true。World は各サブシステムの結果を合わせる。
	virtual bool Tick(
		World& /*world*/,
		const SessionServices& /*session*/,
		const InputState* /*input*/,
		float /*deltaSeconds*/,
		bool /*mouseCaptured*/,
		bool /*keyboardCaptured*/)
	{
		return false;
	}
};
