#include "Runtime/Engine/World/WorldExtract.h"

#include "Runtime/Engine/World/DebugStatsServices.h"
#include "Runtime/Engine/World/Component/CameraComponent.h"
#include "Runtime/Engine/Camera/CameraExtract.h"
#include "Runtime/Engine/World/Component/CameraSystemComponent.h"
#include "Runtime/Engine/Camera/ExtractedView.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Engine/World/ExtractedLighting.h"
#include "Runtime/Engine/World/Subsystem/PhysicsSubsystem.h"
#include "Runtime/Engine/World/Component/Renderable.h"
#include "Runtime/Engine/World/SceneLighting.h"
#include "Runtime/Engine/World/World.h"

#include <span>
#include <utility>

void ExtractWorld(
	const World& world,
	std::vector<WorldExtractedObject>& outObjects,
	std::span<const RenderPose> poses)
{
	world.ForEach<Renderable>([&world, &outObjects, poses](EntityId id, const Renderable& renderable) {
		WorldExtractedObject object{};
		object.objectId = id;
		object.mesh = renderable.mesh;
		object.material = renderable.material;
		object.submeshIndex = renderable.submeshIndex;
		object.layerMask = renderable.layerMask;
		object.visible = renderable.visible;

		Aether::Math::StoreMatrixForHlsl(object.worldMatrix, world.GetWorldMatrix(id, poses));

		object.overrides.baseColor = renderable.overrides.baseColor;
		object.overrides.normal = renderable.overrides.normal;
		object.overrides.parameters.reserve(renderable.overrides.parameters.size());
		for (const WorldMaterialParameterBlock& block : renderable.overrides.parameters)
		{
			WorldExtractedParameterBlock extractedBlock{};
			extractedBlock.bindingSlot = block.bindingSlot;
			extractedBlock.data = std::span<const std::byte>(block.data);
			object.overrides.parameters.push_back(extractedBlock);
		}

		outObjects.push_back(std::move(object));
	});
}

void ExtractCameraView(
	const World& world,
	EntityId camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView,
	std::span<const RenderPose> poses)
{
	const CameraComponent* component = world.Get<CameraComponent>(camera);
	if (component == nullptr)
	{
		outView = {};
		return;
	}

	BuildExtractedView(world.GetWorldMatrix(camera, poses), *component, viewportWidth, viewportHeight, outView);
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

void Extract(
	const World& world,
	std::vector<WorldExtractedObject>& outObjects,
	std::span<const RenderPose> poses,
	DebugStatsServices& stats)
{
	ExtractWorld(world, outObjects, poses);
	stats.extractedObjectCount = static_cast<uint32_t>(outObjects.size());
}

void ExtractView(
	const World& world,
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	std::span<const RenderPose> poses)
{
	const EntityId camera = FindCamera(world);
	if (camera.IsValid())
	{
		ExtractCameraView(world, camera, viewportWidth, viewportHeight, outView, poses);
	}
}

void ExtractLighting(const World& world, WorldExtractedLighting& outLighting)
{
	BuildExtractedLighting(world, outLighting);
}
