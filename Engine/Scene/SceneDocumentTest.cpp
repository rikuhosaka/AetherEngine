#include "Engine/Scene/SceneDocumentTest.h"

#include "Engine/Scene/SceneDocument.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Physics/PhysicsLibrary.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Scene/SceneLighting.h"
#include "Engine/Scene/EntityPhysics.h"
#include "Engine/World/World.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] size_t CountAlive(const World& world)
{
	size_t count = 0;
	world.ForEachAlive([&count](EntityId, const Transform&) {
		++count;
	});
	return count;
}

[[nodiscard]] const WorldDocumentBinding* FindBinding(
	const std::vector<WorldDocumentBinding>& bindings,
	uint32_t id)
{
	for (const WorldDocumentBinding& binding : bindings)
	{
		if (binding.id == id)
		{
			return &binding;
		}
	}

	return nullptr;
}

[[nodiscard]] SceneDocument MakeScene()
{
	SceneDocument document{};

	WorldDocumentEntity sphereWorld{};
	sphereWorld.id = 1;
	sphereWorld.name = "Sphere";
	sphereWorld.transform.position = { 0.0f, 4.0f, 0.0f };
	SceneEntity sphere{};
	sphere.id = 1;
	sphere.mesh = "builtin:sphere";
	sphere.tint[0] = 0.90f;
	sphere.tint[1] = 0.35f;
	sphere.tint[2] = 0.12f;
	sphere.tint[3] = 1.0f;
	RigidBody sphereBody{};
	sphereBody.motion = PhysicsMotion::Dynamic;
	sphereBody.friction = 0.2f;
	sphereBody.shape.type = PhysicsShapeType::Sphere;
	sphereBody.shape.radius = 0.5f;
	sphere.body = sphereBody;

	WorldDocumentEntity hullWorld{};
	hullWorld.id = 2;
	hullWorld.name = "Hull";
	hullWorld.transform.position = { 1.0f, 3.0f, -0.5f };
	SceneEntity hull{};
	hull.id = 2;
	hull.mesh = "builtin:hull";
	hull.tint[0] = 0.30f;
	hull.tint[1] = 0.72f;
	hull.tint[2] = 0.38f;
	hull.tint[3] = 1.0f;
	RigidBody hullBody{};
	hullBody.motion = PhysicsMotion::Dynamic;
	hullBody.friction = 0.4f;
	hullBody.shape.type = PhysicsShapeType::ConvexHull;
	hullBody.shape.points = {
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
	};
	hull.body = hullBody;

	WorldDocumentEntity markerWorld{};
	markerWorld.id = 3;
	markerWorld.name = "Marker";
	markerWorld.visible = false;
	SceneEntity marker{};
	marker.id = 3;
	marker.mesh = "builtin:box";
	marker.tint[0] = 0.2f;
	marker.tint[1] = 0.2f;
	marker.tint[2] = 0.2f;
	marker.tint[3] = 1.0f;

	document.world.push_back(sphereWorld);
	document.world.push_back(hullWorld);
	document.world.push_back(markerWorld);
	document.entities.push_back(sphere);
	document.entities.push_back(hull);
	document.entities.push_back(marker);
	return document;
}

[[nodiscard]] const SceneEntity* FindNamedEntity(const SceneDocument& document, std::string_view name)
{
	for (size_t index = 0; index < document.world.size() && index < document.entities.size(); ++index)
	{
		if (document.world[index].name == name)
		{
			return &document.entities[index];
		}
	}

	return nullptr;
}

[[nodiscard]] const WorldDocumentEntity* FindNamedWorld(const SceneDocument& document, std::string_view name)
{
	for (const WorldDocumentEntity& entity : document.world)
	{
		if (entity.name == name)
		{
			return &entity;
		}
	}

	return nullptr;
}

[[nodiscard]] Result<void> TestPhysicsDropDocument(const std::filesystem::path& assetsRoot)
{
	const std::filesystem::path scenePath = assetsRoot / "Scenes" / "PhysicsDrop.json";
	const Result<SceneDocument> document = LoadSceneDocument(scenePath);
	const WorldDocumentEntity* floor = document ? FindNamedWorld(document.value, "Floor") : nullptr;
	const WorldDocumentEntity* box = document ? FindNamedWorld(document.value, "Box") : nullptr;
	const WorldDocumentEntity* sphere = document ? FindNamedWorld(document.value, "Sphere") : nullptr;
	const WorldDocumentEntity* capsule = document ? FindNamedWorld(document.value, "Capsule") : nullptr;
	const WorldDocumentEntity* hull = document ? FindNamedWorld(document.value, "Hull") : nullptr;
	const SceneEntity* floorEntity = document ? FindNamedEntity(document.value, "Floor") : nullptr;
	const SceneEntity* boxEntity = document ? FindNamedEntity(document.value, "Box") : nullptr;
	const SceneEntity* sphereEntity = document ? FindNamedEntity(document.value, "Sphere") : nullptr;
	const SceneEntity* capsuleEntity = document ? FindNamedEntity(document.value, "Capsule") : nullptr;
	const SceneEntity* hullEntity = document ? FindNamedEntity(document.value, "Hull") : nullptr;
	const bool shapesMatch = floorEntity != nullptr && boxEntity != nullptr && sphereEntity != nullptr
		&& capsuleEntity != nullptr && hullEntity != nullptr && floorEntity->body.has_value()
		&& boxEntity->body.has_value() && sphereEntity->body.has_value() && capsuleEntity->body.has_value()
		&& hullEntity->body.has_value() && floorEntity->body->shape.type == PhysicsShapeType::Mesh
		&& floorEntity->body->shape.indices.size() == 6 && floorEntity->body->shape.indices[0] == 0
		&& floorEntity->body->shape.indices[1] == 1 && floorEntity->body->shape.indices[2] == 2
		&& floorEntity->body->shape.indices[3] == 3 && floorEntity->body->shape.indices[4] == 2
		&& floorEntity->body->shape.indices[5] == 1 && boxEntity->body->shape.type == PhysicsShapeType::Box
		&& IsNearlyEqual(boxEntity->body->shape.halfExtent.x, 2.0f)
		&& IsNearlyEqual(boxEntity->body->shape.halfExtent.y, 0.5f)
		&& IsNearlyEqual(boxEntity->body->shape.halfExtent.z, 2.0f)
		&& sphereEntity->body->shape.type == PhysicsShapeType::Sphere
		&& IsNearlyEqual(sphereEntity->body->shape.radius, 0.5f)
		&& capsuleEntity->body->shape.type == PhysicsShapeType::Capsule
		&& IsNearlyEqual(capsuleEntity->body->shape.radius, 0.22f)
		&& IsNearlyEqual(capsuleEntity->body->shape.halfHeight, 0.55f)
		&& hullEntity->body->shape.type == PhysicsShapeType::ConvexHull
		&& hullEntity->body->shape.points.size() == 8;
	const bool placed = floor != nullptr && box != nullptr && sphere != nullptr && capsule != nullptr && hull != nullptr
		&& IsNearlyEqual(floor->transform.position.x, 0.0f) && IsNearlyEqual(box->transform.position.y, 0.5f)
		&& IsNearlyEqual(sphere->transform.position.y, 4.0f) && IsNearlyEqual(capsule->transform.position.x, -1.15f)
		&& IsNearlyEqual(capsule->transform.position.y, 4.4f) && IsNearlyEqual(capsule->transform.position.z, 0.85f)
		&& IsNearlyEqual(hull->transform.position.x, 1.15f) && IsNearlyEqual(hull->transform.position.y, 3.6f)
		&& IsNearlyEqual(hull->transform.position.z, -0.7f)
		&& IsNearlyEqual(hull->transform.rotation.x, 0.264575481f)
		&& IsNearlyEqual(hull->transform.rotation.y, 0.0840957165f)
		&& IsNearlyEqual(hull->transform.rotation.z, 0.292659938f)
		&& IsNearlyEqual(hull->transform.rotation.w, 0.91502887f);
	if (!document || document.value.entities.size() != 5 || !shapesMatch || !placed)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Physics drop scene document should keep the five shapes and positions");
	}

	World world{};
	const SceneMeshRef meshes[] = {
		{ "builtin:floor", WorldMeshId{ 1, 1 } },
		{ "builtin:box", WorldMeshId{ 2, 1 } },
		{ "builtin:sphere", WorldMeshId{ 3, 1 } },
		{ "builtin:capsule", WorldMeshId{ 4, 1 } },
		{ "builtin:hull", WorldMeshId{ 5, 1 } },
	};
	const Result<std::vector<WorldDocumentBinding>> instantiated = InstantiateScene(
		world,
		document.value,
		meshes,
		SceneRenderResources{});
	const WorldDocumentBinding* sphereBinding = instantiated ? FindBinding(instantiated.value, sphereEntity->id) : nullptr;
	Result<EntityPhysics> created = instantiated ? BindRigidBodies(world) : MakeFail<EntityPhysics>(ErrorCode::InvalidArgument, "Scene did not instantiate");
	if (!instantiated || !created || sphereBinding == nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Physics drop scene document should instantiate");
	}

	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	for (int stepIndex = 0; stepIndex < 30; ++stepIndex)
	{
		if (auto step = created.value.Step(world, kFixedDeltaSeconds); !step)
		{
			return step;
		}
	}

	created.value.WriteTransforms(world, 1.0f);
	const Transform* fallen = world.GetTransform(sphereBinding->entity);
	if (fallen == nullptr || !(fallen->position.y < 4.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Physics drop scene sphere should fall");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestBindScenePhysics()
{
	const SceneDocument source = MakeScene();
	World world{};
	const Result<std::vector<WorldDocumentBinding>> spawned = Instantiate(world, source.world);
	if (!spawned)
	{
		return MakeFail(spawned.error.code, spawned.error.message);
	}

	for (const SceneEntity& entity : source.entities)
	{
		if (!entity.body.has_value())
		{
			continue;
		}

		const WorldDocumentBinding* binding = FindBinding(spawned.value, entity.id);
		if (binding == nullptr || !world.Add<RigidBody>(binding->entity, *entity.body))
		{
			return FailRuntime(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"BindRigidBodies test should add authored bodies");
		}
	}

	Result<EntityPhysics> physics = BindRigidBodies(world);
	const WorldDocumentBinding* sphere = spawned ? FindBinding(spawned.value, 1) : nullptr;
	const WorldDocumentBinding* hull = spawned ? FindBinding(spawned.value, 2) : nullptr;
	const WorldDocumentBinding* marker = spawned ? FindBinding(spawned.value, 3) : nullptr;
	if (!physics || sphere == nullptr || hull == nullptr || marker == nullptr
		|| !physics.value.Unbind(sphere->entity) || !physics.value.Unbind(hull->entity)
		|| physics.value.Unbind(marker->entity))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"BindRigidBodies should bind only entities with rigid body components");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestWriteScene()
{
	const SceneDocument source = MakeScene();
	const Result<std::string> written = WriteSceneDocument(source);
	if (!written || written.value.find("builtin:hull") == std::string::npos
		|| written.value.find("builtin:sphere") == std::string::npos
		|| written.value.find("builtin:box") == std::string::npos)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document JSON should keep mesh keys");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestLightExtract()
{
	SceneLightingSettings settings{};
	World empty{};
	ExtractedLighting fallback{};
	BuildExtractedLighting(settings, empty, fallback);
	if (!IsNearlyEqual(fallback.mainLightDirection[0], settings.mainLightDirection[0])
		|| !IsNearlyEqual(fallback.mainLightDirection[1], settings.mainLightDirection[1])
		|| !IsNearlyEqual(fallback.mainLightIntensity, settings.mainLightIntensity)
		|| !IsNearlyEqual(fallback.ambientIntensity, settings.ambientIntensity))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Lighting without a light component should keep the scene light");
	}

	World world{};
	const EntityId entity = world.Spawn(WorldSpawnDesc{});
	LightComponent light{};
	light.color[0] = 0.2f;
	light.color[1] = 0.4f;
	light.color[2] = 0.6f;
	light.intensity = 3.5f;
	if (!world.Add<LightComponent>(entity, light))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Light extract test should add a light");
	}

	ExtractedLighting extracted{};
	BuildExtractedLighting(settings, world, extracted);
	if (!IsNearlyEqual(extracted.mainLightColor[0], 0.2f) || !IsNearlyEqual(extracted.mainLightColor[1], 0.4f)
		|| !IsNearlyEqual(extracted.mainLightColor[2], 0.6f) || !IsNearlyEqual(extracted.mainLightIntensity, 3.5f)
		|| !IsNearlyEqual(extracted.mainLightDirection[2], 1.0f) || !IsNearlyEqual(extracted.ambientIntensity, settings.ambientIntensity))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"The first light component should replace the main light and keep ambient");
	}

	return MakeOk();
}

} // namespace

Result<void> RunSceneDocumentLoadTests(const std::filesystem::path& assetsRoot)
{
	if (auto written = TestWriteScene(); !written)
	{
		return written;
	}

	if (auto light = TestLightExtract(); !light)
	{
		return light;
	}

	if (auto library = InitializePhysicsLibrary(); !library)
	{
		return library;
	}

	if (auto bound = TestBindScenePhysics(); !bound)
	{
		return bound;
	}

	const SceneDocument source = MakeScene();
	World world{};
	WorldSpawnDesc existingDesc{};
	existingDesc.name = "Existing";
	const EntityId existing = world.Spawn(std::move(existingDesc));

	const SceneMeshRef meshes[] = {
		{ "builtin:sphere", WorldMeshId{ 7, 1 } },
		{ "builtin:hull", WorldMeshId{ 8, 1 } },
		{ "builtin:box", WorldMeshId{ 9, 1 } },
	};
	SceneRenderResources render{};
	render.material = WorldMaterialId{ 3, 2 };
	render.baseColor = WorldTextureId{ 4, 1 };
	render.materialConstantsSlot = 5;

	const Result<std::vector<WorldDocumentBinding>> instantiated = InstantiateScene(
		world,
		source,
		meshes,
		render);
	const WorldDocumentBinding* sphereBinding = instantiated ? FindBinding(instantiated.value, 1) : nullptr;
	const WorldDocumentBinding* hullBinding = instantiated ? FindBinding(instantiated.value, 2) : nullptr;
	const WorldDocumentBinding* markerBinding = instantiated ? FindBinding(instantiated.value, 3) : nullptr;
	const Renderable* sphereRenderable = sphereBinding == nullptr ? nullptr : world.Get<Renderable>(sphereBinding->entity);
	const Renderable* markerRenderable = markerBinding == nullptr ? nullptr : world.Get<Renderable>(markerBinding->entity);
	const RigidBody* sphereBody = sphereBinding == nullptr ? nullptr : world.Get<RigidBody>(sphereBinding->entity);
	const RigidBody* hullBody = hullBinding == nullptr ? nullptr : world.Get<RigidBody>(hullBinding->entity);
	const RigidBody* markerBody = markerBinding == nullptr ? nullptr : world.Get<RigidBody>(markerBinding->entity);
	Result<EntityPhysics> created = instantiated ? BindRigidBodies(world) : MakeFail<EntityPhysics>(ErrorCode::InvalidArgument, "Scene did not instantiate");
	float sphereTint[4]{};
	const bool tintCopied = sphereRenderable != nullptr && sphereRenderable->overrides.parameters.size() == 1
		&& sphereRenderable->overrides.parameters[0].data.size() == sizeof(sphereTint)
		&& sphereRenderable->overrides.parameters[0].bindingSlot == 5;
	if (tintCopied)
	{
		std::memcpy(sphereTint, sphereRenderable->overrides.parameters[0].data.data(), sizeof(sphereTint));
	}

	if (!instantiated || sphereBinding == nullptr || hullBinding == nullptr || markerBinding == nullptr
		|| sphereRenderable == nullptr || markerRenderable == nullptr || !tintCopied
		|| sphereRenderable->mesh.Index != 7 || sphereRenderable->mesh.Generation != 1
		|| sphereRenderable->material.Index != 3 || sphereRenderable->overrides.baseColor.Index != 4
		|| !IsNearlyEqual(sphereTint[0], 0.90f) || !IsNearlyEqual(sphereTint[1], 0.35f)
		|| !IsNearlyEqual(sphereTint[2], 0.12f) || markerRenderable->mesh.Index != 9
		|| sphereBody == nullptr || hullBody == nullptr || markerBody != nullptr
		|| !created || !created.value.Unbind(sphereBinding->entity) || !created.value.Unbind(hullBinding->entity)
		|| created.value.Unbind(markerBinding->entity) || CountAlive(world) != 4 || !world.IsAlive(existing))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"InstantiateScene should resolve meshes, copy tint, and store rigid bodies");
	}

	const size_t aliveBeforeReject = CountAlive(world);
	SceneDocument missingMesh = source;
	missingMesh.world.resize(1);
	missingMesh.entities.resize(1);
	missingMesh.entities[0].mesh = "builtin:missing";
	const SceneMeshRef knownMeshes[] = {
		{ "builtin:sphere", WorldMeshId{ 7, 1 } },
	};
	const Result<std::vector<WorldDocumentBinding>> missingMeshResult = InstantiateScene(
		world,
		missingMesh,
		knownMeshes,
		render);

	const char* childPhysics =
		R"({"version":1,"entities":[{"id":1,"name":"Parent","parent":null,"transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"renderable":{"mesh":"builtin:box","submesh":0,"layerMask":1,"visible":true,"tint":[1,1,1,1]}},{"id":2,"name":"Child","parent":1,"transform":{"position":[0,1,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"renderable":{"mesh":"builtin:sphere","submesh":0,"layerMask":1,"visible":true,"tint":[1,1,1,1]},"physics":{"motion":"dynamic","friction":0.2,"restitution":0,"mass":0,"shape":{"type":"sphere","radius":0.5}}}]})";
	const Result<SceneDocument> childResult = ReadSceneDocument(childPhysics);

	const char* unknownShape =
		R"({"version":1,"entities":[{"id":1,"name":"Bad","parent":null,"transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"renderable":{"mesh":"builtin:sphere","submesh":0,"layerMask":1,"visible":true,"tint":[1,1,1,1]},"physics":{"motion":"dynamic","friction":0.2,"restitution":0,"mass":0,"shape":{"type":"cylinder","radius":0.5}}}]})";
	const Result<SceneDocument> shapeResult = ReadSceneDocument(unknownShape);
	const char* badCamera =
		R"({"version":1,"entities":[{"id":1,"name":"Bad","parent":null,"transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"renderable":{"mesh":"builtin:sphere","submesh":0,"layerMask":1,"visible":true,"tint":[1,1,1,1]},"camera":{"fovY":"wide"}}]})";
	const Result<SceneDocument> cameraResult = ReadSceneDocument(badCamera);
	if (missingMeshResult || childResult || shapeResult || cameraResult || CountAlive(world) != aliveBeforeReject || !world.IsAlive(existing))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A rejected scene document should leave existing entities unchanged");
	}

	if (auto drop = TestPhysicsDropDocument(assetsRoot); !drop)
	{
		return drop;
	}

	LOG_INFO(LogCategory::ECS, "SceneDocumentLoadTests passed");
	return MakeOk();
}
