#pragma once

#include "Engine/Application/Subsystem/IWorldSubsystem.h"

#include <memory>

class EntityPhysics;
class World;

// 責任: プレイ中の剛体シミュレーションを進める。
// 役割: 開始時に剛体を束縛し、固定 Tick で積分し、描画前に補間した姿勢をアクターへ戻す。
//       一時停止中は Engine が FixedTick を呼ばない。
// 所有: World が名前 "Game" で持つ。再生が開いている間だけ EntityPhysics を unique_ptr で所有する。
//       剛体コンポーネントはアクターの所有のまま。World のコピーはこの束縛を持っていかない。
class GameSubsystem final : public IWorldSubsystem
{
public:
	GameSubsystem();
	~GameSubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "Game"; }

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override
	{
		return SubsystemTickGroup::Game;
	}

	Result<void> OnBeginPlay(
		World& world,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward) override;
	void OnEndPlay(World& world) override;
	void FixedTick(World& world, float fixedDeltaSeconds) override;

	// 補間した位置と回転をアクターへ書く。スケールは文書の値のまま。
	// Extract がメッシュを集める直前に呼ぶ。Tick から呼ぶと固定ステップと描画の間が二重になる。
	void WriteTransforms(World& world, float alpha);

private:
	// このサブシステムが所有する束縛。セッションを閉じると破棄する。
	std::unique_ptr<EntityPhysics> m_physics{};
};
