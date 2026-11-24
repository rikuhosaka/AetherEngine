#include "Runtime/Engine/World/WorldExtract.h"

#include "Runtime/Engine/Camera/CameraState.h"
#include "Runtime/Engine/World/Component/CameraComponent.h"
#include "Runtime/Engine/World/Component/CameraSystemComponent.h"
#include "Runtime/Engine/World/Subsystem/PhysicsSubsystem.h"
#include "Runtime/Engine/World/World.h"

#include <DirectXMath.h>

namespace
{
void FillCameraState(DirectX::FXMMATRIX worldMatrix, const CameraComponent& camera, CameraState& out)
{
	using namespace DirectX;

	XMFLOAT4X4 stored{};
	XMStoreFloat4x4(&stored, worldMatrix);
	out = {};
	out.position = { stored._41, stored._42, stored._43 };
	out.forward = { stored._31, stored._32, stored._33 };
	out.up = { stored._21, stored._22, stored._23 };
	out.lens.fovYDegrees = camera.fovYDegrees;
	out.lens.nearPlane = camera.nearPlane;
	out.lens.farPlane = camera.farPlane;
}
} // namespace

void ExtractCameraView(
	const World& world,
	EntityId camera,
	WorldCameraSample& out,
	std::span<const RenderPose> poses)
{
	const CameraComponent* component = world.Get<CameraComponent>(camera);
	if (component == nullptr)
	{
		out = {};
		return;
	}

	FillCameraState(world.GetWorldMatrix(camera, poses), *component, out.state);
	out.found = true;
}

void SampleRenderPoses(const World& world, float alpha, std::vector<RenderPose>& out)
{
	const PhysicsSubsystem* physics = static_cast<const PhysicsSubsystem*>(world.FindSubsystem("Physics"));
	if (physics == nullptr)
	{
		out.clear();
		return;
	}

	physics->SamplePoses(alpha, out);
}

void ExtractView(
	const World& world,
	WorldCameraSample& out,
	std::span<const RenderPose> poses)
{
	const EntityId camera = FindCamera(world);
	if (camera.IsValid())
	{
		ExtractCameraView(world, camera, out, poses);
	}
}
