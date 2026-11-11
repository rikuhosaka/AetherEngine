#pragma once

#include "Engine/World/SceneContentServices.h"
#include "Engine/World/WorldServices.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/World/Actor.h"
#include "Engine/World/EntityId.h"
#include "Engine/World/Lighting.h"
#include "Engine/World/Transform.h"

#include <DirectXMath.h>

#include <cstdint>
#include <memory>
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

class IWorldSubsystem;
class SubsystemContext;

// 責任: シーン文書と、その文書に紐づく再生用サブシステムの寿命を持つ。
// 役割: エンティティの生成、破棄、親子と、プレイセッションの開始、終了、Tick を行う。
// 所有: Engine のメンバ。アクター、コンポーネント、IWorldSubsystem を所有する。
//       セッションが開いているかどうかだけを持つ。モードは持たない。
//       サブシステムの実行時状態は Clone に含めず、ムーブで一緒に移る。
class World final
{
public:
	World();
	World& operator=(const World&) = delete;
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
	// 足せたコンポーネントには、このエンティティの ID を書く。
	template<typename T>
	[[nodiscard]] bool Add(EntityId id, T value)
	{
		Slot* slot = FindSlot(id);
		if (slot == nullptr)
		{
			return false;
		}

		if (!slot->actor.Add(std::move(value)))
		{
			return false;
		}

		slot->actor.Get<T>()->BindOwner(id);
		return true;
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

	// 文書だけを写した新しい World を返す。スロット、世代、親子、名前、変換、空き番号、コンポーネント、ライティング。
	// 死んだスロットは死んだままなのでエンティティ ID は一致する。欠けているコンポーネントも欠けたまま。
	// プレイセッションとサブシステムは写さない。複製先のサブシステムは空。
	[[nodiscard]] World Clone() const;

	// ワールド所有のサブシステムを足す。null、または同名が既にあるときは足さない。
	[[nodiscard]] bool AddSubsystem(std::unique_ptr<IWorldSubsystem> subsystem);

	// この名前のワールドサブシステム。未登録なら nullptr。同名は先に登録された方。
	[[nodiscard]] IWorldSubsystem* FindSubsystem(std::string_view name);
	[[nodiscard]] const IWorldSubsystem* FindSubsystem(std::string_view name) const;

	// プレイセッションが開いている。一時停止中も true。Editor へ戻して EndPlay したときだけ false。
	[[nodiscard]] bool IsPlaying() const { return m_playing; }

	// サブシステムを Tick グループ順に OnBeginPlay する。
	// 開始が揃ったら、先頭のカメラへ CameraSystemComponent を足す。カメラが無ければ1つ作る。
	// すでにセッション中なら何もしない。コンテンツ未ロード、開始失敗、カメラを足せないとき失敗する。
	// 失敗時は開始済みだけ OnEndPlay し、セッションを開かない。文書は戻さない。
	// カメラ位置と前方は両方非 null のときだけ使う。片方でも null なら代替姿勢でカメラを足す。
	[[nodiscard]] Result<void> BeginPlay(
		bool contentLoaded,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward);
	// セッションを閉じ、各 OnEndPlay を呼ぶ。文書は戻さない。
	// OnEndPlay から見えるのは再生中の文書で、再生中に生成したエンティティも残っている。
	// IsPlaying は OnEndPlay の間も true。セッションが閉じていなければ何もしない。一時停止では呼ばない。
	void EndPlay();

	// このワールドを公開し、編集中も含めてサブシステムと全コンポーネントを Tick する。
	// コンポーネントはスロット順。入力はコンポーネントが自分で読む。
	void Tick(float deltaSeconds, WorldServices& published);

	// 固定ステップ。呼ぶのは再生中だけ。一時停止と編集中は Engine が呼ばない。
	void FixedTick(float fixedDeltaSeconds);

	// シーンをこの World の文書へ読む。再生中でもライブの文書を更新する。
	// GPU リソースの生成は呼び出し側が行う。
	[[nodiscard]] Result<void> LoadContent(
		SceneContentServices& content,
		WorldServices& published,
		SubsystemContext& ctx);

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

	// Clone だけが使う。文書だけを写し、セッションとサブシステムは持っていかない。
	World(const World& other);

	// Clone が写すもの。プレイ中の実行時状態は外に置く。
	struct Document
	{
		std::vector<Slot> slots{};
		std::vector<uint32_t> freeIndices{};
		LightingSettings lighting{};
	};

	Document m_document{};

	// プレイセッションが開いている。Clone には含めない。一時停止でも true のまま。
	bool m_playing = false;

	// ワールド所有のサブシステム。生成後に AddSubsystem で足す。Clone は空のまま。
	// ムーブは実行時状態ごと移す。移した側のサブシステムは空。
	std::vector<std::unique_ptr<IWorldSubsystem>> m_subsystems{};
};
