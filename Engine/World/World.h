#pragma once

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Camera/FreeFlyCameraController.h"
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

class EntityPhysics;
class InputState;
class RHICommandList;
class SubsystemContext;
struct EditorCameraServices;
struct ExtractedLighting;
struct ExtractedObject;
struct ExtractedView;
struct FrameContext;

class World final : public ISubsystem
{
public:
	World();
	World(const World& other);
	World& operator=(const World& other);
	World(World&&) noexcept;
	World& operator=(World&&) noexcept;
	~World() override;

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

	// Copies the document: slots, generations, parents, names, transforms, free indices, components, and lighting.
	// Dead slots stay dead, so entity ids match. A missing component stays missing.
	[[nodiscard]] World Clone() const
	{
		return World(*this);
	}

	// First alive entity with a camera, in slot order. Invalid when none exist.
	[[nodiscard]] EntityId FindCamera() const;

	// Root camera. A null position or forward uses the play fallback pose.
	[[nodiscard]] EntityId SpawnCamera(
		const DirectX::XMFLOAT3* position,
		const DirectX::XMFLOAT3* forward);

	// Writes free-fly movement into the camera entity's local transform.
	// Returns whether that camera wants relative mouse movement.
	[[nodiscard]] bool UpdateCamera(
		EntityId camera,
		const InputState* input,
		float deltaSeconds,
		bool mouseCaptured,
		bool keyboardCaptured);

	// View from the camera entity's world matrix and lens. Clears the output when the entity has no camera.
	void ExtractCameraView(
		EntityId camera,
		uint32_t viewportWidth,
		uint32_t viewportHeight,
		ExtractedView& outView) const;

	[[nodiscard]] bool IsPlaying() const { return m_appliedMode == SessionMode::Playing; }

	// Snapshots the document, binds rigid bodies, and keeps the first camera entity.
	// A missing camera is spawned. A null position or forward uses the play fallback pose.
	[[nodiscard]] Result<void> BeginPlay(
		bool contentLoaded,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward);
	void EndPlay();

	// Applies the requested session, publishes this world, and updates the play camera.
	// Returns whether that camera wants relative mouse movement.
	[[nodiscard]] bool Tick(
		float deltaSeconds,
		SessionServices& session,
		WorldServices& published,
		const SceneContentServices& content,
		const InputState* input,
		bool mouseCaptured,
		bool keyboardCaptured,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward);

	void FixedTick(const SessionServices& session, float fixedDeltaSeconds);

	[[nodiscard]] Result<void> LoadContent(
		SceneContentServices& content,
		WorldServices& published,
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList);

	void Extract(
		std::vector<ExtractedObject>& outObjects,
		float alpha,
		const SessionServices& session,
		DebugStatsServices& stats);
	void ExtractView(
		ExtractedView& outView,
		uint32_t viewportWidth,
		uint32_t viewportHeight,
		const EditorCameraServices* editorCamera) const;
	void ExtractLighting(ExtractedLighting& outLighting) const;

	[[nodiscard]] const char* GetName() const override { return "World"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void FixedTick(SubsystemContext& ctx, float fixedDeltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

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

	// Copied by copy construction, copy assignment, and Clone. Later runtime state stays outside this.
	struct Document
	{
		std::vector<Slot> slots{};
		std::vector<uint32_t> freeIndices{};
		LightingSettings lighting{};
	};

	Document m_document{};

	// Play session. Copy and Clone leave this behind.
	std::optional<Document> m_editSnapshot{};
	std::unique_ptr<EntityPhysics> m_physics{};
	SessionMode m_appliedMode = SessionMode::Editor;

	// Free-fly state for one camera entity. Copy and Clone leave this behind.
	FreeFlyCameraController m_cameraController{};
	CameraState m_cameraPose{};
	EntityId m_cameraEntity{};

	// Subsystem services. Copy and Clone leave these behind.
	DebugStatsServices m_debugStats{};
	WorldServices m_worldServices{};
	SessionServices m_session{};
	SceneContentServices m_content{};

	[[nodiscard]] bool ApplySession(
		SessionServices& session,
		bool contentLoaded,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward);
};
