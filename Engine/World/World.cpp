#include "Engine/World/World.h"

#include "Engine/Application/Subsystem/IWorldSubsystem.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Transform.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

World::World() = default;

// 文書だけ写す。スナップショット、モード、サブシステムは持っていかない。
World::World(const World& other)
	: m_document(other.m_document)
{
}

// 文書だけ置き換える。代入先が再生中でもスナップショット、モード、既存サブシステムの実行時状態は残る。
World& World::operator=(const World& other)
{
	if (this != &other)
	{
		m_document = other.m_document;
	}

	return *this;
}

// 文書、セッション、サブシステムをまとめて移す。移した側のモードは Editor に戻し、サブシステム容器は空になる。
World::World(World&& other) noexcept
	: m_document(std::move(other.m_document))
	, m_editSnapshot(std::move(other.m_editSnapshot))
	, m_appliedMode(other.m_appliedMode)
	, m_subsystems(std::move(other.m_subsystems))
{
	other.m_appliedMode = SessionMode::Editor;
}

// ムーブ構築と同じ。代入先が持っていたセッションとサブシステムは捨て、移した側は Editor でサブシステム無しになる。
World& World::operator=(World&& other) noexcept
{
	if (this != &other)
	{
		m_document = std::move(other.m_document);
		m_editSnapshot = std::move(other.m_editSnapshot);
		m_appliedMode = other.m_appliedMode;
		m_subsystems = std::move(other.m_subsystems);

		other.m_appliedMode = SessionMode::Editor;
	}

	return *this;
}

World::~World() = default;

namespace
{
[[nodiscard]] DirectX::XMMATRIX ToLocalMatrix(const Transform& transform)
{
	return Aether::Math::TRS(
		DirectX::XMLoadFloat3(&transform.position),
		DirectX::XMLoadFloat4(&transform.rotation),
		DirectX::XMLoadFloat3(&transform.scale));
}
} // namespace

World::Slot* World::FindSlot(EntityId id)
{
	return const_cast<Slot*>(std::as_const(*this).FindSlot(id));
}

const World::Slot* World::FindSlot(EntityId id) const
{
	if (!id.IsValid() || id.Index >= m_document.slots.size())
	{
		return nullptr;
	}

	const Slot& slot = m_document.slots[id.Index];
	if (!slot.alive || slot.generation != id.Generation)
	{
		return nullptr;
	}

	return &slot;
}

EntityId World::Spawn(WorldSpawnDesc desc)
{
	uint32_t index = 0;
	if (!m_document.freeIndices.empty())
	{
		index = m_document.freeIndices.back();
		m_document.freeIndices.pop_back();
	}
	else
	{
		index = static_cast<uint32_t>(m_document.slots.size());
		Slot slot{};
		slot.generation = 1;
		m_document.slots.push_back(std::move(slot));
	}

	Slot& slot = m_document.slots[index];
	slot.alive = true;
	slot.actor.parent = desc.parent;
	slot.actor.name = std::move(desc.name);
	slot.actor.transform = desc.transform;
	return EntityId{ index, slot.generation };
}

bool World::IsUnder(EntityId entity, EntityId ancestor) const
{
	EntityId current = entity;
	for (int depth = 0; depth < kMaxParentDepth; ++depth)
	{
		const Slot* slot = FindSlot(current);
		if (slot == nullptr || !slot->actor.parent.IsValid())
		{
			return false;
		}

		if (slot->actor.parent == ancestor)
		{
			return true;
		}

		current = slot->actor.parent;
	}

	return false;
}

void World::ReleaseSlot(EntityId id)
{
	Slot* slot = FindSlot(id);
	if (slot == nullptr)
	{
		return;
	}

	slot->alive = false;
	slot->actor = {};
	++slot->generation;
	m_document.freeIndices.push_back(id.Index);
}

void World::Destroy(EntityId id)
{
	if (FindSlot(id) == nullptr)
	{
		return;
	}

	std::vector<EntityId> toDestroy;
	toDestroy.push_back(id);
	for (uint32_t index = 0; index < m_document.slots.size(); ++index)
	{
		const Slot& slot = m_document.slots[index];
		if (!slot.alive)
		{
			continue;
		}

		const EntityId entity{ index, slot.generation };
		if (entity == id)
		{
			continue;
		}

		if (IsUnder(entity, id))
		{
			toDestroy.push_back(entity);
		}
	}

	for (const EntityId entity : toDestroy)
	{
		ReleaseSlot(entity);
	}
}

bool World::SetParent(EntityId child, EntityId parent)
{
	Slot* childSlot = FindSlot(child);
	if (childSlot == nullptr)
	{
		return false;
	}

	if (!parent.IsValid())
	{
		childSlot->actor.parent = {};
		return true;
	}

	if (parent == child || FindSlot(parent) == nullptr)
	{
		return false;
	}

	EntityId current = parent;
	for (int depth = 0; depth < kMaxParentDepth; ++depth)
	{
		if (current == child)
		{
			return false;
		}

		const Slot* slot = FindSlot(current);
		if (slot == nullptr || !slot->actor.parent.IsValid())
		{
			childSlot->actor.parent = parent;
			return true;
		}

		current = slot->actor.parent;
	}

	return false;
}

EntityId World::GetParent(EntityId id) const
{
	const Slot* slot = FindSlot(id);
	return slot == nullptr ? EntityId{} : slot->actor.parent;
}

DirectX::XMMATRIX World::GetWorldMatrix(EntityId id) const
{
	std::array<DirectX::XMMATRIX, kMaxParentDepth> localMatrices{};
	int count = 0;
	EntityId current = id;
	for (; count < kMaxParentDepth; ++count)
	{
		const Slot* slot = FindSlot(current);
		if (slot == nullptr)
		{
			break;
		}

		localMatrices[static_cast<size_t>(count)] = ToLocalMatrix(slot->actor.transform);
		if (!slot->actor.parent.IsValid())
		{
			++count;
			break;
		}

		current = slot->actor.parent;
	}

	DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
	for (int index = count - 1; index >= 0; --index)
	{
		world = DirectX::XMMatrixMultiply(localMatrices[static_cast<size_t>(index)], world);
	}

	return world;
}

bool World::IsAlive(EntityId id) const
{
	return FindSlot(id) != nullptr;
}

std::string_view World::GetName(EntityId id) const
{
	const Slot* slot = FindSlot(id);
	return slot == nullptr ? std::string_view{} : std::string_view{ slot->actor.name };
}

Transform* World::GetTransform(EntityId id)
{
	Slot* slot = FindSlot(id);
	return slot == nullptr ? nullptr : &slot->actor.transform;
}

const Transform* World::GetTransform(EntityId id) const
{
	const Slot* slot = FindSlot(id);
	return slot == nullptr ? nullptr : &slot->actor.transform;
}

bool World::AddSubsystem(std::unique_ptr<IWorldSubsystem> subsystem)
{
	if (subsystem == nullptr || FindSubsystem(subsystem->GetName()) != nullptr)
	{
		return false;
	}

	m_subsystems.push_back(std::move(subsystem));
	return true;
}

IWorldSubsystem* World::FindSubsystem(std::string_view name)
{
	return const_cast<IWorldSubsystem*>(std::as_const(*this).FindSubsystem(name));
}

const IWorldSubsystem* World::FindSubsystem(std::string_view name) const
{
	for (const std::unique_ptr<IWorldSubsystem>& subsystem : m_subsystems)
	{
		if (subsystem != nullptr && name == subsystem->GetName())
		{
			return subsystem.get();
		}
	}

	return nullptr;
}

std::vector<IWorldSubsystem*> World::SubsystemOrder() const
{
	std::vector<IWorldSubsystem*> order{};
	order.reserve(m_subsystems.size());
	for (const std::unique_ptr<IWorldSubsystem>& subsystem : m_subsystems)
	{
		if (subsystem != nullptr)
		{
			order.push_back(subsystem.get());
		}
	}

	std::stable_sort(order.begin(), order.end(), [](const IWorldSubsystem* lhs, const IWorldSubsystem* rhs) {
		return static_cast<int32_t>(lhs->GetTickGroup()) < static_cast<int32_t>(rhs->GetTickGroup());
	});
	return order;
}

Result<void> World::BeginPlay(
	bool contentLoaded,
	const DirectX::XMFLOAT3* cameraPosition,
	const DirectX::XMFLOAT3* cameraForward)
{
	if (m_appliedMode == SessionMode::Playing)
	{
		return MakeOk();
	}

	if (!contentLoaded)
	{
		return MakeFail(ErrorCode::InvalidArgument, "Play requires loaded scene content");
	}

	m_editSnapshot = m_document;

	// 失敗したサブシステム自身は OnEndPlay しない。開始済みを登録順に戻し、文書をスナップショットへ戻す。
	// 逆順ではない。後続が先行の後始末に依存する書き方はできない。
	std::vector<IWorldSubsystem*> started{};
	for (IWorldSubsystem* subsystem : SubsystemOrder())
	{
		if (auto begun = subsystem->OnBeginPlay(*this, cameraPosition, cameraForward); !begun)
		{
			for (IWorldSubsystem* startedSubsystem : started)
			{
				startedSubsystem->OnEndPlay(*this);
			}

			m_document = std::move(*m_editSnapshot);
			m_editSnapshot.reset();
			return begun;
		}

		started.push_back(subsystem);
	}

	m_appliedMode = SessionMode::Playing;
	return MakeOk();
}

void World::EndPlay()
{
	if (m_appliedMode != SessionMode::Playing)
	{
		return;
	}

	if (m_editSnapshot.has_value())
	{
		m_document = std::move(*m_editSnapshot);
		m_editSnapshot.reset();
	}

	// 文書を戻したあと。OnEndPlay はプレイ中に足したカメラや剛体を文書上では見られない。
	for (IWorldSubsystem* subsystem : SubsystemOrder())
	{
		subsystem->OnEndPlay(*this);
	}

	m_appliedMode = SessionMode::Editor;
}

bool World::Tick(
	float deltaSeconds,
	const SessionServices& session,
	WorldServices& published,
	const InputState* input,
	bool mouseCaptured,
	bool keyboardCaptured)
{
	published.Publish(this);

	bool relativeMouse = false;
	for (IWorldSubsystem* subsystem : SubsystemOrder())
	{
		const bool wantsRelativeMouse = subsystem->Tick(
			*this, session, input, deltaSeconds, mouseCaptured, keyboardCaptured);
		relativeMouse = relativeMouse || wantsRelativeMouse;
	}

	return relativeMouse;
}

void World::FixedTick(const SessionServices& session, float fixedDeltaSeconds)
{
	for (IWorldSubsystem* subsystem : SubsystemOrder())
	{
		subsystem->FixedTick(*this, session, fixedDeltaSeconds);
	}
}

Result<void> World::LoadContent(
	SceneContentServices& content,
	WorldServices& published,
	SubsystemContext& ctx,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	if (content.loader == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"World requires scene content");
	}

	World* target = this;
	World editorDocument{};
	// 再生中のロードはシミュレーション中の文書へ書かない。停止時に戻る編集文書だけ更新する。
	if (m_editSnapshot.has_value())
	{
		editorDocument.m_document = *m_editSnapshot;
		target = &editorDocument;
	}

	if (auto loaded = content.loader->Load(*target, ctx, frameContext, commandList); !loaded)
	{
		return loaded;
	}

	if (m_editSnapshot.has_value())
	{
		m_editSnapshot = std::move(editorDocument.m_document);
	}

	published.Publish(this);
	return MakeOk();
}
