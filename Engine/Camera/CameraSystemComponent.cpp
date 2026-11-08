#include "Engine/Camera/CameraSystemComponent.h"

#include "Engine/World/SessionServices.h"
#include "Engine/Camera/CameraComponent.h"
#include "Engine/Platform/InputState.h"
#include "Engine/World/World.h"

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

bool CameraSystemComponent::Tick(
	World& world,
	EntityId self,
	const SessionServices& session,
	const InputState* input,
	float deltaSeconds,
	bool mouseCaptured,
	bool keyboardCaptured)
{
	// 編集中は編集用 World だけが Tick される。再生中と一時停止中はゲーム用 World だけが Tick される。
	const bool editorView = session.mode == SessionMode::Editor && !world.IsPlaying();
	const bool playView = world.IsPlaying()
		&& (session.mode == SessionMode::Playing || session.mode == SessionMode::Paused);
	if (!editorView && !playView)
	{
		return false;
	}

	Transform* transform = world.GetTransform(self);
	if (input == nullptr || transform == nullptr || world.Get<CameraComponent>(self) == nullptr)
	{
		return false;
	}

	if (!m_poseReady)
	{
		// 最初の入力では、置いてある姿勢から取り直す。別カメラのヨー・ピッチは載せない。
		const DirectX::XMFLOAT3 forward = ForwardFromRotation(transform->rotation);
		m_cameraController.Reset(m_cameraPose, transform->position, forward);
		m_poseReady = true;
	}

	// mouseCaptured / keyboardCaptured は UI が入力を取っているという意味。取られている軸はカメラへ渡さない。
	// 右ボタン押下かつマウスが空いているときだけ、相対マウス（カーソルを隠して視点を回す）を要求する。
	const bool useMouse = !mouseCaptured;
	const bool useKeyboard = !keyboardCaptured;
	m_cameraController.Update(m_cameraPose, *input, deltaSeconds, useMouse, useKeyboard);
	WriteCameraPose(*transform, m_cameraPose);
	return useMouse && input->IsDown(Key::MouseRight);
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
		|| !world.Add<CameraSystemComponent>(camera, CameraSystemComponent{}))
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
		if (world.Get<CameraSystemComponent>(lens) != nullptr
			|| world.Add<CameraSystemComponent>(lens, CameraSystemComponent{}))
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
