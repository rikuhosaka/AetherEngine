#include "Engine/World/WorldExtract.h"

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Camera/CameraComponent.h"
#include "Engine/Camera/CameraExtract.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/World/Renderable.h"
#include "Engine/World/SceneLighting.h"
#include "Engine/World/World.h"
#include "Engine/World/WorldRenderBridge.h"

#include <span>

static_assert(WorldLayer::Opaque == RenderLayer::Opaque);
static_assert(WorldLayer::Shadow == RenderLayer::Shadow);
static_assert(WorldLayer::Transparent == RenderLayer::Transparent);

void ExtractWorld(const World& world, std::vector<ExtractedObject>& outObjects)
{
	world.ForEach<Renderable>([&world, &outObjects](EntityId id, const Renderable& renderable) {
		ExtractedObject object{};
		object.objectId.Index = id.Index;
		object.objectId.Generation = id.Generation;
		object.mesh = ToMeshHandle(renderable.mesh);
		object.material = ToMaterialHandle(renderable.material);
		object.submeshIndex = renderable.submeshIndex;
		object.layerMask = renderable.layerMask;
		object.visible = renderable.visible;

		Aether::Math::StoreMatrixForHlsl(object.worldMatrix, world.GetWorldMatrix(id));

		object.overrides.baseColor = ToTextureHandle(renderable.overrides.baseColor);
		object.overrides.normal = ToTextureHandle(renderable.overrides.normal);
		object.overrides.parameters.reserve(renderable.overrides.parameters.size());
		for (const WorldMaterialParameterBlock& block : renderable.overrides.parameters)
		{
			MaterialParameterBlock extractedBlock{};
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
	std::vector<ExtractedObject>& outObjects,
	float alpha,
	const SessionServices& session,
	DebugStatsServices& stats)
{
	if (session.mode == SessionMode::Paused)
	{
		alpha = 1.0f;
	}

	world.WriteInterpolatedTransforms(alpha);
	ExtractWorld(world, outObjects);
	stats.extractedObjectCount = static_cast<uint32_t>(outObjects.size());
}

void ExtractView(
	const World& world,
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	const EditorCameraServices* editorCamera)
{
	if (world.IsPlaying())
	{
		const EntityId camera = world.FindCamera();
		if (camera.IsValid())
		{
			ExtractCameraView(world, camera, viewportWidth, viewportHeight, outView);
		}
		return;
	}

	if (editorCamera != nullptr && editorCamera->valid)
	{
		outView = editorCamera->view;
	}
}

void ExtractLighting(const World& world, ExtractedLighting& outLighting)
{
	BuildExtractedLighting(world, outLighting);
}
