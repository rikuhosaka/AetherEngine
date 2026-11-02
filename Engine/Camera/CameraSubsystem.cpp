#include "Engine/Camera/CameraSubsystem.h"

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

Result<void> CameraSubsystem::OnBeginPlay(
	World& world,
	const DirectX::XMFLOAT3* cameraPosition,
	const DirectX::XMFLOAT3* cameraForward)
{
	// 既存カメラがあるときは生やさず、コントローラも前セッションの対象を捨てる。
	// 最初の UpdateCamera がアクターの姿勢からヨー・ピッチを取り直す。
	// この時点では World はまだ IsPlaying ではない。Tick は BeginPlay 完了後。
	if (!FindCamera(world).IsValid())
	{
		const EntityId camera = SpawnCamera(world, cameraPosition, cameraForward);
		if (!camera.IsValid())
		{
			m_cameraEntity = {};
			return MakeFail(ErrorCode::InvalidArgument, "Play requires a camera entity");
		}
	}
	else
	{
		m_cameraEntity = {};
	}

	return MakeOk();
}

void CameraSubsystem::OnEndPlay(World& /*world*/)
{
	m_cameraEntity = {};
}

bool CameraSubsystem::Tick(
	World& world,
	const SessionServices& session,
	const InputState* input,
	float deltaSeconds,
	bool mouseCaptured,
	bool keyboardCaptured)
{
	// 一時停止中も視点は残す。セッションが閉じていれば編集カメラ側に任せる。
	const bool wantCamera = session.mode == SessionMode::Playing || session.mode == SessionMode::Paused;
	if (!world.IsPlaying() || !wantCamera)
	{
		return false;
	}

	const EntityId camera = FindCamera(world);
	if (!camera.IsValid())
	{
		return false;
	}

	return UpdateCamera(world, camera, input, deltaSeconds, mouseCaptured, keyboardCaptured);
}

EntityId CameraSubsystem::FindCamera(const World& world) const
{
	EntityId found{};
	world.ForEach<CameraComponent>([&found](EntityId id, const CameraComponent&) {
		if (!found.IsValid())
		{
			found = id;
		}
	});
	return found;
}

EntityId CameraSubsystem::SpawnCamera(
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

	FreeFlyCameraController controller = m_cameraController;
	CameraState pose{};
	controller.Reset(pose, posePosition, poseForward);

	WorldSpawnDesc desc{};
	desc.name = "Camera";
	WriteCameraPose(desc.transform, pose);
	const EntityId camera = world.Spawn(std::move(desc));
	if (!world.Add<CameraComponent>(camera, CameraComponent{}))
	{
		world.Destroy(camera);
		return {};
	}

	m_cameraController = controller;
	m_cameraPose = pose;
	m_cameraEntity = camera;
	return camera;
}

bool CameraSubsystem::UpdateCamera(
	World& world,
	EntityId camera,
	const InputState* input,
	float deltaSeconds,
	bool mouseCaptured,
	bool keyboardCaptured)
{
	Transform* transform = world.GetTransform(camera);
	if (input == nullptr || transform == nullptr || world.Get<CameraComponent>(camera) == nullptr)
	{
		return false;
	}

	if (m_cameraEntity != camera)
	{
		// 対象が替わった、または BeginPlay でコントローラを捨てたあとは、置いてある姿勢から取り直す。
		// 前のカメラのヨー・ピッチを新しいアクターへ載せない。
		const DirectX::XMFLOAT3 forward = ForwardFromRotation(transform->rotation);
		m_cameraController.Reset(m_cameraPose, transform->position, forward);
		m_cameraEntity = camera;
	}

	// mouseCaptured / keyboardCaptured は UI が入力を取っているという意味。取られている軸はカメラへ渡さない。
	// 右ボタン押下かつマウスが空いているときだけ、相対マウス（カーソルを隠して視点を回す）を要求する。
	const bool useMouse = !mouseCaptured;
	const bool useKeyboard = !keyboardCaptured;
	m_cameraController.Update(m_cameraPose, *input, deltaSeconds, useMouse, useKeyboard);
	WriteCameraPose(*transform, m_cameraPose);
	return useMouse && input->IsDown(Key::MouseRight);
}
