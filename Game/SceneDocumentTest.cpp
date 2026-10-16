#include "Game/SceneDocumentTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Physics/EntityPhysics.h"
#include "Engine/World/World.h"
#include "Game/SceneDocument.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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
	world.ForEachAlive([&count](EntityId, const Transform&, const Renderable&) {
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

[[nodiscard]] bool SameTint(const float left[4], const float right[4])
{
	return IsNearlyEqual(left[0], right[0]) && IsNearlyEqual(left[1], right[1])
		&& IsNearlyEqual(left[2], right[2]) && IsNearlyEqual(left[3], right[3]);
}

[[nodiscard]] bool SamePoint(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
{
	return IsNearlyEqual(left.x, right.x) && IsNearlyEqual(left.y, right.y) && IsNearlyEqual(left.z, right.z);
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
	SceneBody sphereBody{};
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
	SceneBody hullBody{};
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

[[nodiscard]] bool SameScene(const SceneDocument& left, const SceneDocument& right)
{
	if (left.world.size() != right.world.size() || left.entities.size() != right.entities.size())
	{
		return false;
	}

	for (size_t index = 0; index < left.world.size(); ++index)
	{
		const WorldDocumentEntity& leftWorld = left.world[index];
		const WorldDocumentEntity& rightWorld = right.world[index];
		const SceneEntity& leftEntity = left.entities[index];
		const SceneEntity& rightEntity = right.entities[index];
		if (leftWorld.id != rightWorld.id || leftWorld.parent != rightWorld.parent || leftWorld.name != rightWorld.name
			|| !IsNearlyEqual(leftWorld.transform.position.x, rightWorld.transform.position.x)
			|| !IsNearlyEqual(leftWorld.transform.position.y, rightWorld.transform.position.y)
			|| !IsNearlyEqual(leftWorld.transform.position.z, rightWorld.transform.position.z)
			|| leftEntity.id != rightEntity.id || leftEntity.mesh != rightEntity.mesh
			|| !SameTint(leftEntity.tint, rightEntity.tint) || leftEntity.body.has_value() != rightEntity.body.has_value())
		{
			return false;
		}

		if (!leftEntity.body.has_value())
		{
			continue;
		}

		const SceneBody& leftBody = *leftEntity.body;
		const SceneBody& rightBody = *rightEntity.body;
		if (leftBody.motion != rightBody.motion || !IsNearlyEqual(leftBody.friction, rightBody.friction)
			|| leftBody.shape.type != rightBody.shape.type || leftBody.shape.points.size() != rightBody.shape.points.size())
		{
			return false;
		}

		if (leftBody.shape.type == PhysicsShapeType::Sphere
			&& !IsNearlyEqual(leftBody.shape.radius, rightBody.shape.radius))
		{
			return false;
		}

		for (size_t pointIndex = 0; pointIndex < leftBody.shape.points.size(); ++pointIndex)
		{
			if (!SamePoint(leftBody.shape.points[pointIndex], rightBody.shape.points[pointIndex]))
			{
				return false;
			}
		}
	}

	return true;
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
	std::ifstream file(scenePath);
	if (!file)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::FileNotFound,
			"Physics drop scene document should be readable");
	}

	const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	const Result<SceneDocument> document = ReadSceneDocument(text);
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

	Result<EntityPhysics> created = EntityPhysics::Create();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	World world{};
	const SceneMeshRef meshes[] = {
		{ "builtin:floor", MeshHandle{ 1, 1 } },
		{ "builtin:box", MeshHandle{ 2, 1 } },
		{ "builtin:sphere", MeshHandle{ 3, 1 } },
		{ "builtin:capsule", MeshHandle{ 4, 1 } },
		{ "builtin:hull", MeshHandle{ 5, 1 } },
	};
	const Result<std::vector<WorldDocumentBinding>> instantiated = InstantiateScene(
		world,
		created.value,
		document.value,
		meshes,
		SceneRenderResources{});
	const WorldDocumentBinding* sphereBinding = instantiated ? FindBinding(instantiated.value, sphereEntity->id) : nullptr;
	if (!instantiated || sphereBinding == nullptr)
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

[[nodiscard]] Result<void> TestCaptureScene()
{
	const SceneDocument source = MakeScene();
	Result<EntityPhysics> created = EntityPhysics::Create();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	World world{};
	const SceneMeshRef meshes[] = {
		{ "builtin:sphere", MeshHandle{ 7, 1 } },
		{ "builtin:hull", MeshHandle{ 8, 1 } },
		{ "builtin:box", MeshHandle{ 9, 1 } },
	};
	const Result<std::vector<WorldDocumentBinding>> instantiated = InstantiateScene(
		world,
		created.value,
		source,
		meshes,
		SceneRenderResources{});
	const WorldDocumentBinding* sphereBinding = instantiated ? FindBinding(instantiated.value, 1) : nullptr;
	const WorldDocumentBinding* markerBinding = instantiated ? FindBinding(instantiated.value, 3) : nullptr;
	Transform* sphereTransform = sphereBinding == nullptr ? nullptr : world.GetTransform(sphereBinding->entity);
	if (!instantiated || sphereBinding == nullptr || markerBinding == nullptr || sphereTransform == nullptr
		|| !world.SetParent(markerBinding->entity, sphereBinding->entity))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene capture test should instantiate and reparent the marker");
	}

	sphereTransform->position = { 9.0f, 8.0f, 7.0f };
	const Result<SceneDocument> captured = CaptureScene(world, source, instantiated.value);
	const WorldDocumentEntity* capturedSphere = captured ? FindNamedWorld(captured.value, "Sphere") : nullptr;
	const WorldDocumentEntity* capturedMarker = captured ? FindNamedWorld(captured.value, "Marker") : nullptr;
	const SceneEntity* capturedHull = captured ? FindNamedEntity(captured.value, "Hull") : nullptr;
	const bool hullKept = capturedHull != nullptr && capturedHull->mesh == "builtin:hull" && capturedHull->body.has_value()
		&& capturedHull->body->shape.points.size() == source.entities[1].body->shape.points.size()
		&& SamePoint(capturedHull->body->shape.points[0], source.entities[1].body->shape.points[0]);
	if (!captured || capturedSphere == nullptr || capturedMarker == nullptr || !hullKept
		|| !IsNearlyEqual(capturedSphere->transform.position.x, 9.0f)
		|| !IsNearlyEqual(capturedSphere->transform.position.y, 8.0f)
		|| !IsNearlyEqual(capturedSphere->transform.position.z, 7.0f)
		|| capturedMarker->parent != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene capture should update the local transform and parent without changing the hull");
	}

	world.Destroy(sphereBinding->entity);
	if (CaptureScene(world, source, instantiated.value))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene capture should fail when an editor entity is gone");
	}

	return MakeOk();
}
} // namespace

Result<void> RunSceneDocumentTests(const std::filesystem::path& assetsRoot)
{
	const SceneDocument source = MakeScene();
	const Result<std::string> written = WriteSceneDocument(source);
	const Result<SceneDocument> readBack = written
		? ReadSceneDocument(written.value)
		: MakeFail<SceneDocument>(written.error.code, written.error.message);
	const Result<std::string> writtenAgain = readBack
		? WriteSceneDocument(readBack.value)
		: MakeFail<std::string>(readBack.error.code, readBack.error.message);
	const Result<SceneDocument> readAgain = writtenAgain
		? ReadSceneDocument(writtenAgain.value)
		: MakeFail<SceneDocument>(writtenAgain.error.code, writtenAgain.error.message);
	const SceneBody* readHull = readAgain && readAgain.value.entities.size() == 3 && readAgain.value.entities[1].body.has_value()
		? &(*readAgain.value.entities[1].body)
		: nullptr;
	if (!written || !readBack || !writtenAgain || !readAgain || !SameScene(source, readAgain.value) || readHull == nullptr
		|| readHull->shape.points.size() != 4)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document JSON should round-trip mesh keys, tint, and owned hull points");
	}

	Result<EntityPhysics> created = EntityPhysics::Create();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	World world{};
	WorldSpawnDesc existingDesc{};
	existingDesc.name = "Existing";
	const EntityId existing = world.Spawn(std::move(existingDesc));

	const SceneMeshRef meshes[] = {
		{ "builtin:sphere", MeshHandle{ 7, 1 } },
		{ "builtin:hull", MeshHandle{ 8, 1 } },
		{ "builtin:box", MeshHandle{ 9, 1 } },
	};
	SceneRenderResources render{};
	render.material = MaterialHandle{ 3, 2 };
	render.baseColor = TextureHandle{ 4, 1 };
	render.materialConstantsSlot = 5;

	const Result<std::vector<WorldDocumentBinding>> instantiated = InstantiateScene(
		world,
		created.value,
		readAgain.value,
		meshes,
		render);
	const WorldDocumentBinding* sphereBinding = instantiated ? FindBinding(instantiated.value, 1) : nullptr;
	const WorldDocumentBinding* hullBinding = instantiated ? FindBinding(instantiated.value, 2) : nullptr;
	const WorldDocumentBinding* markerBinding = instantiated ? FindBinding(instantiated.value, 3) : nullptr;
	const Renderable* sphereRenderable = sphereBinding == nullptr ? nullptr : world.GetRenderable(sphereBinding->entity);
	const Renderable* markerRenderable = markerBinding == nullptr ? nullptr : world.GetRenderable(markerBinding->entity);
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
		|| !created.value.Unbind(sphereBinding->entity) || !created.value.Unbind(hullBinding->entity)
		|| created.value.Unbind(markerBinding->entity) || CountAlive(world) != 4 || !world.IsAlive(existing))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"InstantiateScene should resolve meshes, copy tint, and bind only bodies");
	}

	const size_t aliveBeforeReject = CountAlive(world);
	SceneDocument missingMesh = source;
	missingMesh.world.resize(1);
	missingMesh.entities.resize(1);
	missingMesh.entities[0].mesh = "builtin:missing";
	const SceneMeshRef knownMeshes[] = {
		{ "builtin:sphere", MeshHandle{ 7, 1 } },
	};
	const Result<std::vector<WorldDocumentBinding>> missingMeshResult = InstantiateScene(
		world,
		created.value,
		missingMesh,
		knownMeshes,
		render);

	const char* childPhysics =
		R"({"version":1,"entities":[{"id":1,"name":"Parent","parent":null,"transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"renderable":{"mesh":"builtin:box","submesh":0,"layerMask":1,"visible":true,"tint":[1,1,1,1]}},{"id":2,"name":"Child","parent":1,"transform":{"position":[0,1,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"renderable":{"mesh":"builtin:sphere","submesh":0,"layerMask":1,"visible":true,"tint":[1,1,1,1]},"physics":{"motion":"dynamic","friction":0.2,"restitution":0,"mass":0,"shape":{"type":"sphere","radius":0.5}}}]})";
	const Result<SceneDocument> childResult = ReadSceneDocument(childPhysics);

	const char* unknownShape =
		R"({"version":1,"entities":[{"id":1,"name":"Bad","parent":null,"transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"renderable":{"mesh":"builtin:sphere","submesh":0,"layerMask":1,"visible":true,"tint":[1,1,1,1]},"physics":{"motion":"dynamic","friction":0.2,"restitution":0,"mass":0,"shape":{"type":"cylinder","radius":0.5}}}]})";
	const Result<SceneDocument> shapeResult = ReadSceneDocument(unknownShape);
	if (missingMeshResult || childResult || shapeResult || CountAlive(world) != aliveBeforeReject || !world.IsAlive(existing))
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

	if (auto captured = TestCaptureScene(); !captured)
	{
		return captured;
	}

	LOG_INFO(LogCategory::ECS, "SceneDocumentTests passed");
	return MakeOk();
}
