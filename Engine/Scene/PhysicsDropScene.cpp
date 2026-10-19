#include "Engine/Scene/PhysicsDropScene.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/PrimitiveMesh.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/Scene/SceneDocument.h"
#include "Engine/Scene/EditorScene.h"
#include "Engine/Scene/WorldRenderBridge.h"

#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr uint32_t kSphereStacks = 12;
constexpr uint32_t kSphereSlices = 16;
constexpr uint32_t kCapsuleSlices = 16;
constexpr uint32_t kCapsuleHemisphereStacks = 4;

[[nodiscard]] Result<MeshHandle> UploadPrimitive(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const PrimitiveMeshData& mesh,
	const char* debugName)
{
	return resources.GetMeshServices().UploadMesh(
		MakePrimitiveMeshUploadDesc(mesh, debugName),
		frameContext,
		commandList);
}

[[nodiscard]] Result<TextureHandle> UploadWhiteTexture(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	const std::array<std::byte, 4> whitePixel = {
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
	};
	TextureUploadDesc textureDesc{};
	textureDesc.width = 1;
	textureDesc.height = 1;
	TextureMipData mip{};
	mip.width = 1;
	mip.height = 1;
	mip.rowPitch = 4;
	mip.pixels = whitePixel;
	textureDesc.mips.push_back(mip);

	return resources.GetTextureServices().UploadTexture(textureDesc, frameContext, commandList);
}

[[nodiscard]] Result<std::string> ReadSceneText(const std::filesystem::path& scenePath)
{
	std::ifstream file(scenePath);
	if (!file)
	{
		return FailRuntime<std::string>(
			LogCategory::Physics,
			ErrorCode::FileNotFound,
			"PhysicsDropScene could not open the scene document");
	}

	return MakeOk(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
}

[[nodiscard]] const SceneEntity* FindMeshEntity(const SceneDocument& document, std::string_view key)
{
	for (const SceneEntity& entity : document.entities)
	{
		if (entity.mesh == key)
		{
			return &entity;
		}
	}

	return nullptr;
}

[[nodiscard]] const SceneEntity* FindEntity(const SceneDocument& document, uint32_t id)
{
	for (const SceneEntity& entity : document.entities)
	{
		if (entity.id == id)
		{
			return &entity;
		}
	}

	return nullptr;
}

[[nodiscard]] PhysicsBodyDesc MakeBodyDesc(const SceneBody& body)
{
	PhysicsBodyDesc desc{};
	desc.motion = body.motion;
	desc.friction = body.friction;
	desc.restitution = body.restitution;
	desc.mass = body.mass;
	desc.shape.type = body.shape.type;
	desc.shape.halfExtent = body.shape.halfExtent;
	desc.shape.radius = body.shape.radius;
	desc.shape.halfHeight = body.shape.halfHeight;
	desc.shape.points = body.shape.points;
	desc.shape.indices = body.shape.indices;
	return desc;
}

[[nodiscard]] const SceneBody* BodyOfType(const SceneEntity* entity, PhysicsShapeType type)
{
	if (entity == nullptr || !entity->body.has_value() || entity->body->shape.type != type)
	{
		return nullptr;
	}

	return &(*entity->body);
}
} // namespace

Result<void> PhysicsDropScene::EnsureInitialized(
	EditorScene& scene,
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot,
	const std::filesystem::path& scenePath)
{
	if (m_ready)
	{
		return MakeOk();
	}

	if (commandList == nullptr)
	{
		return FailInternal(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene requires a valid command list");
	}

	if (shaderRoot.empty())
	{
		return FailInternal(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene requires a valid shader root path");
	}

	const Result<std::string> sceneText = ReadSceneText(scenePath);
	if (!sceneText)
	{
		return MakeFail(sceneText.error.code, sceneText.error.message);
	}

	Result<SceneDocument> document = ReadSceneDocument(sceneText.value);
	if (!document)
	{
		return MakeFail(document.error.code, document.error.message);
	}

	const SceneBody* floor = BodyOfType(FindMeshEntity(document.value, "builtin:floor"), PhysicsShapeType::Mesh);
	const SceneBody* box = BodyOfType(FindMeshEntity(document.value, "builtin:box"), PhysicsShapeType::Box);
	const SceneBody* sphere = BodyOfType(FindMeshEntity(document.value, "builtin:sphere"), PhysicsShapeType::Sphere);
	const SceneBody* capsule = BodyOfType(FindMeshEntity(document.value, "builtin:capsule"), PhysicsShapeType::Capsule);
	const SceneBody* hull = BodyOfType(FindMeshEntity(document.value, "builtin:hull"), PhysicsShapeType::ConvexHull);
	if (floor == nullptr || box == nullptr || sphere == nullptr || capsule == nullptr || hull == nullptr)
	{
		return FailInternal(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene scene document is missing a built-in shape");
	}

	const Result<PrimitiveMeshData> floorMesh = GenerateTriangleMesh({ floor->shape.points, floor->shape.indices });
	if (!floorMesh)
	{
		return MakeFail(floorMesh.error.code, floorMesh.error.message);
	}

	const Result<MeshHandle> floorUpload = UploadPrimitive(
		resources, frameContext, commandList, floorMesh.value, "PhysicsDropFloor");
	if (!floorUpload)
	{
		return MakeFail(floorUpload.error.code, floorUpload.error.message);
	}
	m_floorMesh = floorUpload.value;

	const Result<PrimitiveMeshData> boxMesh = GenerateCubeMesh({
		box->shape.halfExtent.x * 2.0f,
		box->shape.halfExtent.y * 2.0f,
		box->shape.halfExtent.z * 2.0f,
		false });
	if (!boxMesh)
	{
		return MakeFail(boxMesh.error.code, boxMesh.error.message);
	}

	const Result<MeshHandle> boxUpload = UploadPrimitive(
		resources, frameContext, commandList, boxMesh.value, "PhysicsDropBox");
	if (!boxUpload)
	{
		return MakeFail(boxUpload.error.code, boxUpload.error.message);
	}
	m_boxMesh = boxUpload.value;

	const Result<PrimitiveMeshData> sphereMesh = GenerateSphereMesh({
		sphere->shape.radius, kSphereStacks, kSphereSlices });
	if (!sphereMesh)
	{
		return MakeFail(sphereMesh.error.code, sphereMesh.error.message);
	}

	const Result<MeshHandle> sphereUpload = UploadPrimitive(
		resources, frameContext, commandList, sphereMesh.value, "PhysicsDropSphere");
	if (!sphereUpload)
	{
		return MakeFail(sphereUpload.error.code, sphereUpload.error.message);
	}
	m_sphereMesh = sphereUpload.value;

	const Result<PrimitiveMeshData> capsuleMesh = GenerateCapsuleMesh({
		capsule->shape.radius,
		capsule->shape.halfHeight,
		kCapsuleSlices,
		kCapsuleHemisphereStacks });
	if (!capsuleMesh)
	{
		return MakeFail(capsuleMesh.error.code, capsuleMesh.error.message);
	}

	const Result<MeshHandle> capsuleUpload = UploadPrimitive(
		resources, frameContext, commandList, capsuleMesh.value, "PhysicsDropCapsule");
	if (!capsuleUpload)
	{
		return MakeFail(capsuleUpload.error.code, capsuleUpload.error.message);
	}
	m_capsuleMesh = capsuleUpload.value;

	const Result<PrimitiveMeshData> hullMesh = GenerateConvexHullMesh({ hull->shape.points });
	if (!hullMesh)
	{
		return MakeFail(hullMesh.error.code, hullMesh.error.message);
	}

	const Result<MeshHandle> hullUpload = UploadPrimitive(
		resources, frameContext, commandList, hullMesh.value, "PhysicsDropHull");
	if (!hullUpload)
	{
		return MakeFail(hullUpload.error.code, hullUpload.error.message);
	}
	m_hullMesh = hullUpload.value;

	const Result<TextureHandle> texture = UploadWhiteTexture(resources, frameContext, commandList);
	if (!texture)
	{
		return MakeFail(texture.error.code, texture.error.message);
	}
	m_baseColor = texture.value;

	MaterialCreateDesc materialDesc{};
	materialDesc.vertexShaderPath = shaderRoot / "LitBasicVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "LitBasicPS.hlsl";
	materialDesc.vertexEntryPoint = "LitBasicVS";
	materialDesc.pixelEntryPoint = "LitBasicPS";
	materialDesc.inputLayout = InputLayoutType::Basic;
	materialDesc.requiredLayout = VertexLayoutId::Basic;

	const Result<MaterialHandle> material = resources.GetMaterialServices().CreateMaterial(materialDesc);
	if (!material)
	{
		return MakeFail(material.error.code, material.error.message);
	}
	m_material = material.value;

	const Material* createdMaterial = resources.GetMaterialServices().GetMaterial(m_material);
	if (createdMaterial == nullptr)
	{
		return FailInternal(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene failed to resolve created material");
	}

	m_materialConstantsSlot.reset();
	for (uint32_t layoutIndex = 0; layoutIndex < createdMaterial->constantLayout.size(); ++layoutIndex)
	{
		const ShaderConstantBuffer& layout = createdMaterial->constantLayout[layoutIndex];
		if (layout.Register == RenderRegisters::MaterialConstants && layout.Space == 0)
		{
			m_materialConstantsSlot = layoutIndex;
			break;
		}
	}

	scene.SetPath(scenePath);
	scene.SetDocument(std::move(document.value));
	m_ready = true;
	return MakeOk();
}

Result<void> PhysicsDropScene::SpawnInto(EditorScene& scene)
{
	if (m_spawned)
	{
		return MakeOk();
	}

	if (!m_ready)
	{
		return FailInternal(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene::SpawnInto requires initialized GPU assets");
	}

	Result<EntityPhysics> physics = EntityPhysics::Create();
	if (!physics)
	{
		return FailInternal(LogCategory::Physics, physics.error.code, physics.error.message);
	}

	const SceneMeshRef meshes[] = {
		{ "builtin:floor", ToWorldMeshId(m_floorMesh) },
		{ "builtin:box", ToWorldMeshId(m_boxMesh) },
		{ "builtin:sphere", ToWorldMeshId(m_sphereMesh) },
		{ "builtin:capsule", ToWorldMeshId(m_capsuleMesh) },
		{ "builtin:hull", ToWorldMeshId(m_hullMesh) },
	};
	SceneRenderResources render{};
	render.material = ToWorldMaterialId(m_material);
	render.baseColor = ToWorldTextureId(m_baseColor);
	render.materialConstantsSlot = m_materialConstantsSlot;

	if (!scene.HasDocument())
	{
		return FailInternal(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene::SpawnInto requires a scene document");
	}

	Result<std::vector<WorldDocumentBinding>> bindings = InstantiateScene(
		scene.GetWorld(),
		physics.value,
		scene.GetDocument(),
		meshes,
		render);
	if (!bindings)
	{
		return MakeFail(bindings.error.code, bindings.error.message);
	}

	scene.SetBindings(std::move(bindings.value));
	m_physics = std::move(physics.value);
	m_spawned = true;

	LOG_INFO(LogCategory::Physics, "Physics drop scene spawned a mesh floor, static box, dynamic sphere, capsule, and convex hull");
	return MakeOk();
}

Result<EntityPhysics> PhysicsDropScene::CreatePlayPhysics(const EditorScene& editor, World& playWorld)
{
	if (!m_spawned)
	{
		return FailInternal<EntityPhysics>(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene::CreatePlayPhysics requires a spawned editor world");
	}

	Result<EntityPhysics> physics = EntityPhysics::Create();
	if (!physics)
	{
		return FailInternal<EntityPhysics>(LogCategory::Physics, physics.error.code, physics.error.message);
	}

	if (!editor.HasDocument())
	{
		return FailInternal<EntityPhysics>(LogCategory::Physics, ErrorCode::InvalidArgument,
			"PhysicsDropScene::CreatePlayPhysics requires a scene document");
	}

	for (const WorldDocumentBinding& binding : editor.GetBindings())
	{
		const SceneEntity* entity = FindEntity(editor.GetDocument(), binding.id);
		if (entity == nullptr || !entity->body.has_value())
		{
			continue;
		}

		if (auto bound = physics.value.Bind(playWorld, binding.entity, MakeBodyDesc(*entity->body)); !bound)
		{
			return FailInternal<EntityPhysics>(LogCategory::Physics, bound.error.code, bound.error.message);
		}
	}

	return physics;
}
