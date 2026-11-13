#include "Runtime/Engine/Camera/CameraSystemComponent.h"

#include "Runtime/Engine/Camera/CameraComponent.h"
#include "Runtime/Engine/Camera/PlayerController.h"
#include "Runtime/Core/Platform/InputState.h"
#include "Runtime/Engine/World/World.h"

namespace
{
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
} // namespace

void CameraSystemComponent::ApplyControl(
	World& world,
	const InputState* input,
	float deltaSeconds,
	bool useMouse,
	bool useKeyboard)
{
	const EntityId owner = GetOwner();
	Transform* transform = world.GetTransform(owner);
	if (input == nullptr || transform == nullptr || world.Get<CameraComponent>(owner) == nullptr)
	{
		return;
	}

	if (!m_poseReady)
	{
		// 最初の入力では、置いてある姿勢から取り直す。別カメラのヨー・ピッチは載せない。
		const DirectX::XMFLOAT3 forward = ForwardFromRotation(transform->rotation);
		m_cameraController.Reset(m_cameraPose, transform->position, forward);
		m_poseReady = true;
	}

	m_cameraController.Update(m_cameraPose, *input, deltaSeconds, useMouse, useKeyboard);
	WriteCameraPose(*transform, m_cameraPose);
}

EntityId FindCamera(const World& world)
{
	EntityId found{};
	world.ForEach<CameraSystemComponent>([&found](EntityId id, const CameraSystemComponent&) {
		if (!found.IsValid())
		{
			found = id;
		}
	});
	return found;
}

EntityId SpawnCamera(
	World& world,
	const DirectX::XMFLOAT3* position,
	const DirectX::XMFLOAT3* forward)
{
	const DirectX::XMFLOAT3& posePosition = position != nullptr && forward != nullptr
		? *position
		: kPlayCameraFallbackPosition;
	const DirectX::XMFLOAT3& poseForward = position != nullptr && forward != nullptr
		? *forward
		: kPlayCameraFallbackForward;

	FreeFlyCameraController controller{};
	CameraState pose{};
	controller.Reset(pose, posePosition, poseForward);

	WorldSpawnDesc desc{};
	desc.name = "Camera";
	WriteCameraPose(desc.transform, pose);
	const EntityId camera = world.Spawn(std::move(desc));
	if (!world.Add<CameraComponent>(camera, CameraComponent{})
		|| !world.Add<CameraSystemComponent>(camera, CameraSystemComponent{})
		|| !world.Add<PlayerController>(camera, PlayerController{}))
	{
		world.Destroy(camera);
		return {};
	}

	return camera;
}

Result<void> EnsurePlayCamera(
	World& world,
	const DirectX::XMFLOAT3* position,
	const DirectX::XMFLOAT3* forward)
{
	EntityId lens{};
	world.ForEach<CameraComponent>([&lens](EntityId id, const CameraComponent&) {
		if (!lens.IsValid())
		{
			lens = id;
		}
	});

	if (lens.IsValid())
	{
		const bool hasSystem = world.Get<CameraSystemComponent>(lens) != nullptr
			|| world.Add<CameraSystemComponent>(lens, CameraSystemComponent{});
		const bool hasPlayer = world.Get<PlayerController>(lens) != nullptr
			|| world.Add<PlayerController>(lens, PlayerController{});
		if (hasSystem && hasPlayer)
		{
			return MakeOk();
		}

		return MakeFail(ErrorCode::InvalidArgument, "Play requires a camera entity");
	}

	if (!SpawnCamera(world, position, forward).IsValid())
	{
		return MakeFail(ErrorCode::InvalidArgument, "Play requires a camera entity");
	}

	return MakeOk();
}
