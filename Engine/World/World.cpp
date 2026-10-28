#include "Engine/World/World.h"

#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Camera/CameraComponent.h"
#include "Engine/Camera/CameraExtract.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Transform.h"
#include "Engine/Platform/InputState.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/World/EntityPhysics.h"
#include "Engine/World/SceneLighting.h"
#include "Engine/World/WorldExtract.h"

#include <array>
#include <span>
#include <utility>
#include <vector>

World::World() = default;

World::World(const World& other)
	: m_document(other.m_document)
{
}

World& World::operator=(const World& other)
{
	if (this != &other)
	{
		m_document = other.m_document;
	}

	return *this;
}

World::World(World&& other) noexcept
	: m_document(std::move(other.m_document))
	, m_editSnapshot(std::move(other.m_editSnapshot))
	, m_physics(std::move(other.m_physics))
	, m_appliedMode(other.m_appliedMode)
	, m_cameraController(std::move(other.m_cameraController))
	, m_cameraPose(std::move(other.m_cameraPose))
	, m_cameraEntity(other.m_cameraEntity)
	, m_debugStats(std::move(other.m_debugStats))
	, m_worldServices(std::move(other.m_worldServices))
	, m_session(std::move(other.m_session))
	, m_content(std::move(other.m_content))
{
	if (m_worldServices.world == &other)
	{
		m_worldServices.world = this;
	}

	other.m_appliedMode = SessionMode::Editor;
	other.m_cameraEntity = {};
	other.m_debugStats = {};
	other.m_worldServices = {};
	other.m_session = {};
	other.m_content = {};
}

World& World::operator=(World&& other) noexcept
{
	if (this != &other)
	{
		m_document = std::move(other.m_document);
		m_editSnapshot = std::move(other.m_editSnapshot);
		m_physics = std::move(other.m_physics);
		m_appliedMode = other.m_appliedMode;
		m_cameraController = std::move(other.m_cameraController);
		m_cameraPose = std::move(other.m_cameraPose);
		m_cameraEntity = other.m_cameraEntity;
		m_debugStats = std::move(other.m_debugStats);
		m_worldServices = std::move(other.m_worldServices);
		m_session = std::move(other.m_session);
		m_content = std::move(other.m_content);
		if (m_worldServices.world == &other)
		{
			m_worldServices.world = this;
		}

		other.m_appliedMode = SessionMode::Editor;
		other.m_cameraEntity = {};
		other.m_debugStats = {};
		other.m_worldServices = {};
		other.m_session = {};
		other.m_content = {};
	}

	return *this;
}

World::~World() = default;

namespace
{
constexpr const char* kDependencies[] = { "Render", "Input" };
constexpr DirectX::XMFLOAT3 kPlayCameraFallbackPosition{ 0.0f, 1.6f, -4.0f };
constexpr DirectX::XMFLOAT3 kPlayCameraFallbackForward{ 0.0f, 0.0f, 1.0f };

[[nodiscard]] DirectX::XMFLOAT3 ForwardFromRotation(const DirectX::XMFLOAT4& rotation)
{
	using namespace DirectX;

	const XMVECTOR forward = XMVector3Rotate(
		XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		XMLoadFloat4(&rotation));
	XMFLOAT3 stored{};
	XMStoreFloat3(&stored, forward);
	return stored;
}

[[nodiscard]] DirectX::XMFLOAT4 RotationFromPose(const CameraState& pose)
{
	using namespace DirectX;

	const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&pose.forward));
	const XMVECTOR upHint = XMVector3Normalize(XMLoadFloat3(&pose.up));
	const XMVECTOR right = XMVector3Normalize(XMVector3Cross(upHint, forward));
	const XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

	XMMATRIX rotation = XMMatrixIdentity();
	rotation.r[0] = XMVectorSetW(right, 0.0f);
	rotation.r[1] = XMVectorSetW(up, 0.0f);
	rotation.r[2] = XMVectorSetW(forward, 0.0f);
	rotation.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	XMFLOAT4 stored{};
	XMStoreFloat4(&stored, XMQuaternionRotationMatrix(rotation));
	return stored;
}

void WriteCameraPose(Transform& transform, const CameraState& pose)
{
	transform.position = pose.position;
	transform.rotation = RotationFromPose(pose);
}

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

EntityId World::FindCamera() const
{
	EntityId found{};
	ForEach<CameraComponent>([&found](EntityId id, const CameraComponent&) {
		if (!found.IsValid())
		{
			found = id;
		}
	});
	return found;
}

EntityId World::SpawnCamera(const DirectX::XMFLOAT3* position, const DirectX::XMFLOAT3* forward)
{
	const DirectX::XMFLOAT3& posePosition = position != nullptr && forward != nullptr
		? *position
		: kPlayCameraFallbackPosition;
	const DirectX::XMFLOAT3& poseForward = position != nullptr && forward != nullptr
		? *forward
		: kPlayCameraFallbackForward;

	FreeFlyCameraController controller = m_cameraController;
	CameraState pose{};
	controller.Reset(pose, posePosition, poseForward);

	WorldSpawnDesc desc{};
	desc.name = "Camera";
	WriteCameraPose(desc.transform, pose);
	const EntityId camera = Spawn(std::move(desc));
	if (!Add<CameraComponent>(camera, CameraComponent{}))
	{
		Destroy(camera);
		return {};
	}

	m_cameraController = controller;
	m_cameraPose = pose;
	m_cameraEntity = camera;
	return camera;
}

bool World::UpdateCamera(
	EntityId camera,
	const InputState* input,
	float deltaSeconds,
	bool mouseCaptured,
	bool keyboardCaptured)
{
	Transform* transform = GetTransform(camera);
	if (input == nullptr || transform == nullptr || Get<CameraComponent>(camera) == nullptr)
	{
		return false;
	}

	if (m_cameraEntity != camera)
	{
		const DirectX::XMFLOAT3 forward = ForwardFromRotation(transform->rotation);
		m_cameraController.Reset(m_cameraPose, transform->position, forward);
		m_cameraEntity = camera;
	}

	const bool useMouse = !mouseCaptured;
	const bool useKeyboard = !keyboardCaptured;
	m_cameraController.Update(m_cameraPose, *input, deltaSeconds, useMouse, useKeyboard);
	WriteCameraPose(*transform, m_cameraPose);
	return useMouse && input->IsDown(Key::MouseRight);
}

void World::ExtractCameraView(
	EntityId camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView) const
{
	const CameraComponent* component = Get<CameraComponent>(camera);
	if (component == nullptr)
	{
		outView = {};
		return;
	}

	BuildExtractedView(GetWorldMatrix(camera), *component, viewportWidth, viewportHeight, outView);
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
	Result<EntityPhysics> physics = BindRigidBodies(*this);
	if (!physics)
	{
		m_document = std::move(*m_editSnapshot);
		m_editSnapshot.reset();
		return MakeFail(physics.error.code, physics.error.message);
	}

	m_physics = std::make_unique<EntityPhysics>(std::move(physics.value));
	if (!FindCamera().IsValid())
	{
		const EntityId camera = SpawnCamera(cameraPosition, cameraForward);
		if (!camera.IsValid())
		{
			m_physics.reset();
			m_document = std::move(*m_editSnapshot);
			m_editSnapshot.reset();
			m_cameraEntity = {};
			return MakeFail(ErrorCode::InvalidArgument, "Play requires a camera entity");
		}
	}
	else
	{
		m_cameraEntity = {};
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

	m_physics.reset();
	if (m_editSnapshot.has_value())
	{
		m_document = std::move(*m_editSnapshot);
		m_editSnapshot.reset();
	}

	m_cameraEntity = {};
	m_appliedMode = SessionMode::Editor;
}

bool World::ApplySession(
	SessionServices& session,
	bool contentLoaded,
	const DirectX::XMFLOAT3* cameraPosition,
	const DirectX::XMFLOAT3* cameraForward)
{
	const bool sessionOpen = m_appliedMode == SessionMode::Playing;
	const bool wantSession = session.mode == SessionMode::Playing || session.mode == SessionMode::Paused;
	if (wantSession && !sessionOpen)
	{
		if (session.mode == SessionMode::Paused)
		{
			session.mode = SessionMode::Editor;
			return false;
		}

		if (auto begun = BeginPlay(contentLoaded, cameraPosition, cameraForward); !begun)
		{
			LogResult(begun, LogCategory::Core);
			session.mode = SessionMode::Editor;
			return false;
		}

		return true;
	}

	if (!wantSession && sessionOpen)
	{
		EndPlay();
		return true;
	}

	return false;
}

bool World::Tick(
	float deltaSeconds,
	SessionServices& session,
	WorldServices& published,
	const SceneContentServices& content,
	const InputState* input,
	bool mouseCaptured,
	bool keyboardCaptured,
	const DirectX::XMFLOAT3* cameraPosition,
	const DirectX::XMFLOAT3* cameraForward)
{
	if (ApplySession(session, content.loaded, cameraPosition, cameraForward))
	{
		published.Advance();
	}

	published.Publish(this);

	const bool wantCamera = session.mode == SessionMode::Playing || session.mode == SessionMode::Paused;
	if (m_appliedMode != SessionMode::Playing || !wantCamera)
	{
		return false;
	}

	const EntityId camera = FindCamera();
	if (!camera.IsValid())
	{
		return false;
	}

	return UpdateCamera(camera, input, deltaSeconds, mouseCaptured, keyboardCaptured);
}

void World::FixedTick(const SessionServices& session, float fixedDeltaSeconds)
{
	if (session.mode != SessionMode::Playing || m_physics == nullptr)
	{
		return;
	}

	if (auto step = m_physics->Step(*this, fixedDeltaSeconds); !step)
	{
		LogResult(step, LogCategory::Physics);
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

void World::Extract(
	std::vector<ExtractedObject>& outObjects,
	float alpha,
	const SessionServices& session,
	DebugStatsServices& stats)
{
	if (session.mode == SessionMode::Paused)
	{
		alpha = 1.0f;
	}

	if (m_physics != nullptr)
	{
		m_physics->WriteTransforms(*this, alpha);
	}

	ExtractWorld(*this, outObjects);
	stats.extractedObjectCount = static_cast<uint32_t>(outObjects.size());
}

void World::ExtractView(
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	const EditorCameraServices* editorCamera) const
{
	if (m_appliedMode == SessionMode::Playing)
	{
		const EntityId camera = FindCamera();
		if (camera.IsValid())
		{
			ExtractCameraView(camera, viewportWidth, viewportHeight, outView);
		}
		return;
	}

	if (editorCamera != nullptr && editorCamera->valid)
	{
		outView = editorCamera->view;
	}
}

void World::ExtractLighting(ExtractedLighting& outLighting) const
{
	BuildExtractedLighting(*this, outLighting);
}

std::span<const char* const> World::GetDependencies() const
{
	return kDependencies;
}

Result<void> World::Initialize(SubsystemContext& ctx)
{
	if (ctx.GetService<RenderServices>() == nullptr || ctx.GetService<InputServices>() == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"World requires RenderServices and InputServices");
	}

	m_session.mode = SessionMode::Editor;
	m_worldServices.Publish(this);
	ctx.RegisterService(&m_debugStats);
	ctx.RegisterService(&m_worldServices);
	ctx.RegisterService(&m_session);
	ctx.RegisterService(&m_content);
	return MakeOk();
}

void World::Tick(SubsystemContext& ctx, float deltaSeconds)
{
	InputServices* inputServices = ctx.GetService<InputServices>();
	const EditorCameraServices* camera = ctx.GetService<EditorCameraServices>();
	const bool cameraValid = camera != nullptr && camera->valid;
	const bool relativeMouse = Tick(
		deltaSeconds,
		m_session,
		m_worldServices,
		m_content,
		inputServices != nullptr ? inputServices->state : nullptr,
		inputServices != nullptr && inputServices->mouseCaptured,
		inputServices != nullptr && inputServices->keyboardCaptured,
		cameraValid ? &camera->position : nullptr,
		cameraValid ? &camera->forward : nullptr);
	if (IsPlaying() && inputServices != nullptr)
	{
		inputServices->relativeMouse = relativeMouse;
	}
}

void World::FixedTick(SubsystemContext& /*ctx*/, float fixedDeltaSeconds)
{
	FixedTick(m_session, fixedDeltaSeconds);
}

void World::Shutdown(SubsystemContext& ctx)
{
	EndPlay();

	ctx.UnregisterService(&m_content);
	m_content = {};
	ctx.UnregisterService(&m_session);
	m_session = {};
	ctx.UnregisterService(&m_worldServices);
	m_worldServices = {};
	ctx.UnregisterService(&m_debugStats);
	m_debugStats = {};
}
