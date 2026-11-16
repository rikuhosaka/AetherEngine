#pragma once

#include "Runtime/Engine/Application/Subsystem/IWorldSubsystem.h"
#include "Runtime/Engine/World/RenderPose.h"

#include <memory>
#include <vector>

class PhysicsScene;
class World;

// 責任: プレイ中の剛体シミュレーションを進める。
// 役割: 開始時に剛体を束縛し、固定 Tick で積分し、ステップ後の姿勢をアクターへ戻す。
//       補間した姿勢は SamplePoses がフレーム限りの配列で返す。
//       一時停止中は Engine が FixedTick を呼ばない。
// 所有: World が名前 "Physics" で持つ。再生が開いている間だけ PhysicsScene を unique_ptr で所有する。
//       剛体コンポーネントはアクターの所有のまま。World のコピーはこの束縛を持っていかない。
class PhysicsSubsystem final : public IWorldSubsystem
{
public:
	PhysicsSubsystem();
	~PhysicsSubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "Physics"; }

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

	// 補間した位置と回転を out へ書く。物理が無ければ out を空にする。
	void SamplePoses(float alpha, std::vector<RenderPose>& out) const;

private:
	// このサブシステムが所有する束縛。セッションを閉じると破棄する。
	std::unique_ptr<PhysicsScene> m_physics{};
};
