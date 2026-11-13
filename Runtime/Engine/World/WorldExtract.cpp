#include "Runtime/Engine/World/WorldExtract.h"

#include "Runtime/Engine/World/DebugStatsServices.h"
#include "Runtime/Engine/Camera/CameraComponent.h"
#include "Runtime/Engine/Camera/CameraExtract.h"
#include "Runtime/Engine/Camera/CameraSystemComponent.h"
#include "Runtime/Engine/Camera/ExtractedView.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Engine/World/ExtractedLighting.h"
#include "Runtime/Engine/World/Subsystem/GameSubsystem.h"
#include "Runtime/Engine/World/Renderable.h"
#include "Runtime/Engine/World/SceneLighting.h"
#include "Runtime/Engine/World/World.h"

#include <utility>

void ExtractWorld(const World& world, std::vector<WorldExtractedObject>& outObjects)
{
	world.ForEach<Renderable>([&world, &outObjects](EntityId id, const Renderable& renderable) {
		WorldExtractedObject object{};
		object.objectId = id;
		object.mesh = renderable.mesh;
		object.material = renderable.material;
		object.submeshIndex = renderable.submeshIndex;
		object.layerMask = renderable.layerMask;
		object.visible = renderable.visible;

		Aether::Math::StoreMatrixForHlsl(object.worldMatrix, world.GetWorldMatrix(id));

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
	ExtractedView& outView)
{
	const CameraComponent* component = world.Get<CameraComponent>(camera);
	if (component == nullptr)
	{
		outView = {};
		return;
	}

	BuildExtractedView(world.GetWorldMatrix(camera), *component, viewportWidth, viewportHeight, outView);
}

void Extract(
	World& world,
	std::vector<WorldExtractedObject>& outObjects,
	float alpha,
	DebugStatsServices& stats)
{
	if (GameSubsystem* game = static_cast<GameSubsystem*>(world.FindSubsystem("Game")))
	{
		game->WriteTransforms(world, alpha);
	}
	ExtractWorld(world, outObjects);
	stats.extractedObjectCount = static_cast<uint32_t>(outObjects.size());
}

void ExtractView(
	const World& world,
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight)
{
	const EntityId camera = FindCamera(world);
	if (camera.IsValid())
	{
		ExtractCameraView(world, camera, viewportWidth, viewportHeight, outView);
	}
}

void ExtractLighting(const World& world, ExtractedLighting& outLighting)
{
	BuildExtractedLighting(world, outLighting);
}
