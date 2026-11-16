#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/World/RenderPose.h"
#include "Runtime/Physics/PhysicsWorld.h"
#include "Runtime/Engine/World/World.h"

#include <vector>

// 責任: エンティティと剛体の対応を保ち、ステップと姿勢の公開を行う。
// 役割: 親の無いアクターを PhysicsWorld のボディへ束縛する。ステップは変換を書かない。
//       確定姿勢の書き戻しと、描画用の補間サンプルは別呼び出し。
// 所有: PhysicsSubsystem が再生中だけ持つ。PhysicsWorld と束縛表を所有する。エンティティは World の所有のまま。コピー不可。
class PhysicsScene
{
public:
	PhysicsScene() = default;
	~PhysicsScene() = default;

	PhysicsScene(PhysicsScene&& other) noexcept = default;
	PhysicsScene& operator=(PhysicsScene&& other) noexcept = default;

	PhysicsScene(const PhysicsScene&) = delete;
	PhysicsScene& operator=(const PhysicsScene&) = delete;

	[[nodiscard]] static Result<PhysicsScene> Create();

	// Copies the entity position and rotation into the body. Scale is ignored.
	// The entity must be alive, parentless, and not already bound.
	[[nodiscard]] Result<PhysicsBodyId> Bind(World& world, EntityId entity, PhysicsBodyDesc desc);

	[[nodiscard]] Result<void> Unbind(EntityId entity);

	// Drops bodies whose entities are gone. A kinematic body then moves to its Transform.
	// The step itself does not write transforms.
	[[nodiscard]] Result<void> Step(World& world, float fixedDeltaSeconds);

	// 最後のステップ後の位置と回転を Transform へ書く。スケールは変えない。
	// エンティティが死んでいる束縛は外す。
	void CommitTransforms(World& world);

	// alpha 0 はステップ前、1 はステップ後。Transform も束縛も変えない。
	void SamplePoses(float alpha, std::vector<RenderPose>& out) const;

	[[nodiscard]] PhysicsWorld& GetWorld();

private:
	struct Binding
	{
		EntityId entity{};
		PhysicsBodyId body{};
		PhysicsMotion motion = PhysicsMotion::Dynamic;
	};

	explicit PhysicsScene(PhysicsWorld world);

	[[nodiscard]] bool IsBound(EntityId entity) const;
	void ReleaseIfEntityDead(World& world);
	void EraseAt(size_t index);

	PhysicsWorld m_physicsWorld{};
	std::vector<Binding> m_bindings{};
};

// Binds RigidBody components. A failure unbinds bodies from this call.
[[nodiscard]] Result<PhysicsScene> BindRigidBodies(World& world);
