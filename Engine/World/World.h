#pragma once

#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/World/Actor.h"
#include "Engine/World/EntityId.h"
#include "Engine/World/Lighting.h"
#include "Engine/World/Transform.h"

#include <DirectXMath.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct WorldSpawnDesc
{
	std::string name{};
	Transform transform{};
	EntityId parent{};
};

class InputState;
class IWorldSubsystem;
class RHICommandList;
class SubsystemContext;
struct FrameContext;

// 責任: シーン文書と、その文書に紐づく再生用サブシステムの寿命を持つ。
// 役割: エンティティの生成、破棄、親子と、プレイセッションの開始、終了、Tick を行う。
// 所有: Engine のメンバ。アクター、コンポーネント、IWorldSubsystem を所有する。
//       セッションと入力は参照で受け取り、所有しない。サブシステムの実行時状態はコピーに含めず、ムーブで一緒に移る。
class World final
{
public:
	World();
	World(const World& other);
	World& operator=(const World& other);
	World(World&&) noexcept;
	World& operator=(World&&) noexcept;
	~World();

	EntityId Spawn(WorldSpawnDesc desc);
	void Destroy(EntityId id);

	// Parent must be alive. An invalid parent detaches the child to the root.
	// Local TRS is left unchanged. Returns false for a missing child, a dead parent,
	// a self parent, or a parent that would create a cycle.
	[[nodiscard]] bool SetParent(EntityId child, EntityId parent);

	[[nodiscard]] EntityId GetParent(EntityId id) const;
	// Row-vector product local * parent * ... * root. A dead parent breaks the chain.
	[[nodiscard]] DirectX::XMMATRIX GetWorldMatrix(EntityId id) const;

	[[nodiscard]] bool IsAlive(EntityId id) const;
	// Empty when the entity is dead or was spawned without a name.
	[[nodiscard]] std::string_view GetName(EntityId id) const;
	[[nodiscard]] Transform* GetTransform(EntityId id);
	[[nodiscard]] const Transform* GetTransform(EntityId id) const;

	// Fails when the entity is dead or already has this component.
	// T must derive from Component and be copyable.
	template<typename T>
	[[nodiscard]] bool Add(EntityId id, T value)
	{
		Slot* slot = FindSlot(id);
		if (slot == nullptr)
		{
			return false;
		}

		return slot->actor.Add(std::move(value));
	}

	template<typename T>
	[[nodiscard]] T* Get(EntityId id)
	{
		return const_cast<T*>(std::as_const(*this).Get<T>(id));
	}

	template<typename T>
	[[nodiscard]] const T* Get(EntityId id) const
	{
		const Slot* slot = FindSlot(id);
		if (slot == nullptr)
		{
			return nullptr;
		}

		return slot->actor.Get<T>();
	}

	template<typename T>
	[[nodiscard]] bool Remove(EntityId id)
	{
		Slot* slot = FindSlot(id);
		if (slot == nullptr)
		{
			return false;
		}

		return slot->actor.Remove<T>();
	}

	// Alive entities that have T, in slot order.
	template<typename T, typename Fn>
	void ForEach(Fn&& fn) const
	{
		for (uint32_t index = 0; index < m_document.slots.size(); ++index)
		{
			const Slot& slot = m_document.slots[index];
			if (!slot.alive)
			{
				continue;
			}

			const T* component = slot.actor.Get<T>();
			if (component == nullptr)
			{
				continue;
			}

			fn(EntityId{ index, slot.generation }, *component);
		}
	}

	[[nodiscard]] LightingSettings& GetLighting() { return m_document.lighting; }
	[[nodiscard]] const LightingSettings& GetLighting() const { return m_document.lighting; }

	// 文書だけを写す。スロット、世代、親子、名前、変換、空き番号、コンポーネント、ライティング。
	// 死んだスロットは死んだままなのでエンティティ ID は一致する。欠けているコンポーネントも欠けたまま。
	// プレイセッションとサブシステムは写さない。複製先のサブシステムは空。
	[[nodiscard]] World Clone() const
	{
		return World(*this);
	}

	// ワールド所有のサブシステムを足す。null、または同名が既にあるときは足さない。
	[[nodiscard]] bool AddSubsystem(std::unique_ptr<IWorldSubsystem> subsystem);

	// この名前のワールドサブシステム。未登録なら nullptr。同名は先に登録された方。
	[[nodiscard]] IWorldSubsystem* FindSubsystem(std::string_view name);
	[[nodiscard]] const IWorldSubsystem* FindSubsystem(std::string_view name) const;

	// プレイセッションが開いている。一時停止中も true。Editor へ戻して EndPlay したときだけ false。
	[[nodiscard]] bool IsPlaying() const { return m_appliedMode == SessionMode::Playing; }

	// 文書をスナップショットしてから、サブシステムを Tick グループ順に OnBeginPlay する。
	// 開始が揃ったら、先頭のカメラへ CameraSystemComponent を足す。カメラが無ければ1つ作る。
	// すでにセッション中なら何もしない。コンテンツ未ロード、開始失敗、カメラを足せないとき失敗する。
	// 失敗時は開始済みだけ OnEndPlay し、文書をスナップショットへ戻してセッションを開かない。
	// カメラ位置と前方は両方非 null のときだけ使う。片方でも null なら代替姿勢でカメラを足す。
	[[nodiscard]] Result<void> BeginPlay(
		bool contentLoaded,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward);
	// セッションを閉じ、文書をスナップショットへ戻してから各 OnEndPlay を呼ぶ。
	// OnEndPlay から見えるのは編集時の文書で、再生中に生成したエンティティは既に無い。
	// IsPlaying は OnEndPlay の間も true。セッションが閉じていなければ何もしない。一時停止では呼ばない。
	void EndPlay();

	// このワールドを公開し、編集中も含めてサブシステムと全コンポーネントを Tick する。
	// コンポーネントはスロット順。戻り値は、いずれかが相対マウスを要求したか。
	[[nodiscard]] bool Tick(
		float deltaSeconds,
		const SessionServices& session,
		WorldServices& published,
		const InputState* input,
		bool mouseCaptured,
		bool keyboardCaptured);

	// 固定ステップ。積分するかは各サブシステムが session.mode を見て決める。
	// 一時停止中も届くが、GameSubsystem は Playing のときだけ進める。
	void FixedTick(const SessionServices& session, float fixedDeltaSeconds);

	// シーンを文書へ読む。再生中はライブの文書を触らず、EndPlay で戻すスナップショットだけ差し替える。
	[[nodiscard]] Result<void> LoadContent(
		SceneContentServices& content,
		WorldServices& published,
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList);

	template<typename Fn>
	void ForEachAlive(Fn&& fn) const
	{
		for (uint32_t index = 0; index < m_document.slots.size(); ++index)
		{
			const Slot& slot = m_document.slots[index];
			if (!slot.alive)
			{
				continue;
			}

			const EntityId id{ index, slot.generation };
			fn(id, slot.actor.transform);
		}
	}

	// Alive entities whose parent is missing or dead, in slot order.
	template<typename Fn>
	void ForEachRoot(Fn&& fn) const
	{
		for (uint32_t index = 0; index < m_document.slots.size(); ++index)
		{
			const Slot& slot = m_document.slots[index];
			if (!slot.alive || FindSlot(slot.actor.parent) != nullptr)
			{
				continue;
			}

			fn(EntityId{ index, slot.generation });
		}
	}

	// Direct children in slot order. A dead or missing parent yields no children.
	template<typename Fn>
	void ForEachChild(EntityId parent, Fn&& fn) const
	{
		if (FindSlot(parent) == nullptr)
		{
			return;
		}

		for (uint32_t index = 0; index < m_document.slots.size(); ++index)
		{
			const Slot& slot = m_document.slots[index];
			if (!slot.alive || slot.actor.parent != parent)
			{
				continue;
			}

			fn(EntityId{ index, slot.generation });
		}
	}

private:
	struct Slot
	{
		bool alive = false;
		uint32_t generation = 0;
		Actor actor{};
	};

	static constexpr int kMaxParentDepth = 64;

	[[nodiscard]] Slot* FindSlot(EntityId id);
	[[nodiscard]] const Slot* FindSlot(EntityId id) const;
	[[nodiscard]] bool IsUnder(EntityId entity, EntityId ancestor) const;
	void ReleaseSlot(EntityId id);

	[[nodiscard]] std::vector<IWorldSubsystem*> SubsystemOrder() const;

	// コピー構築、コピー代入、Clone が写すもの。プレイ中の実行時状態は外に置く。
	struct Document
	{
		std::vector<Slot> slots{};
		std::vector<uint32_t> freeIndices{};
		LightingSettings lighting{};
	};

	Document m_document{};

	// プレイセッション。コピーと Clone には含めない。コピー代入もここを消さない。
	// 再生中の World へコピー代入すると、文書だけ替わってセッションと物理が残る。
	std::optional<Document> m_editSnapshot{};
	SessionMode m_appliedMode = SessionMode::Editor;

	// ワールド所有のサブシステム。生成後に AddSubsystem で足す。コピーと Clone は空のまま。
	// ムーブは実行時状態ごと移す。移した側のサブシステムは空。
	std::vector<std::unique_ptr<IWorldSubsystem>> m_subsystems{};
};
