#include "Engine/World/GameSubsystem.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/World/EntityPhysics.h"

GameSubsystem::GameSubsystem() = default;

GameSubsystem::~GameSubsystem() = default;

Result<void> GameSubsystem::OnBeginPlay(
	World& world,
	const DirectX::XMFLOAT3* /*cameraPosition*/,
	const DirectX::XMFLOAT3* /*cameraForward*/)
{
	Result<EntityPhysics> physics = BindRigidBodies(world);
	if (!physics)
	{
		return MakeFail(physics.error.code, physics.error.message);
	}

	m_physics = std::make_unique<EntityPhysics>(std::move(physics.value));
	return MakeOk();
}

void GameSubsystem::OnEndPlay(World& /*world*/)
{
	m_physics.reset();
}

void GameSubsystem::FixedTick(World& world, const SessionServices& session, float fixedDeltaSeconds)
{
	// 一時停止中も World::FixedTick は届く。積分するのは Playing の間だけ。
	// ステップ失敗はセッションを止めず記録する。ここで失敗を返す口が無い。
	if (session.mode != SessionMode::Playing || m_physics == nullptr)
	{
		return;
	}

	if (auto step = m_physics->Step(world, fixedDeltaSeconds); !step)
	{
		LogResult(step, LogCategory::Physics);
	}
}

void GameSubsystem::WriteTransforms(World& world, float alpha)
{
	if (m_physics != nullptr)
	{
		m_physics->WriteTransforms(world, alpha);
	}
}
