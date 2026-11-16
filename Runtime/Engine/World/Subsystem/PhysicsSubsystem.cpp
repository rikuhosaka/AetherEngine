#include "Runtime/Engine/World/Subsystem/PhysicsSubsystem.h"

#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Engine/World/PhysicsScene.h"

PhysicsSubsystem::PhysicsSubsystem() = default;

PhysicsSubsystem::~PhysicsSubsystem() = default;

Result<void> PhysicsSubsystem::OnBeginPlay(
	World& world,
	const DirectX::XMFLOAT3* /*cameraPosition*/,
	const DirectX::XMFLOAT3* /*cameraForward*/)
{
	Result<PhysicsScene> physics = BindRigidBodies(world);
	if (!physics)
	{
		return MakeFail(physics.error.code, physics.error.message);
	}

	m_physics = std::make_unique<PhysicsScene>(std::move(physics.value));
	return MakeOk();
}

void PhysicsSubsystem::OnEndPlay(World& /*world*/)
{
	m_physics.reset();
}

void PhysicsSubsystem::FixedTick(World& world, float fixedDeltaSeconds)
{
	// 届いた固定ステップは積分する。一時停止中は Engine が呼ばない。
	// ステップ失敗はセッションを止めず記録する。ここで失敗を返す口が無い。
	if (m_physics == nullptr)
	{
		return;
	}

	if (auto step = m_physics->Step(world, fixedDeltaSeconds); !step)
	{
		LogResult(step, LogCategory::Physics);
		return;
	}

	m_physics->CommitTransforms(world);
}

void PhysicsSubsystem::SamplePoses(float alpha, std::vector<RenderPose>& out) const
{
	if (m_physics == nullptr)
	{
		out.clear();
		return;
	}

	m_physics->SamplePoses(alpha, out);
}
