#include "Game/PhysicsDropScene.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/PrimitiveMesh.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/World/Transform.h"
#include "Game/WorldRenderBridge.h"

#include <array>
#include <utility>

namespace
{
constexpr float kFloorExtent = 4.0f;
constexpr float kBoxWidth = 4.0f;
constexpr float kBoxHeight = 1.0f;
constexpr float kBoxDepth = 4.0f;
constexpr float kSphereRadius = 0.5f;
constexpr float kCapsuleRadius = 0.22f;
constexpr float kCapsuleHalfHeight = 0.55f;
constexpr float kHullHalfExtent = 0.28f;

const std::array<DirectX::XMFLOAT3, 4> kFloorCorners = {{
	{ -kFloorExtent, 0.0f, -kFloorExtent },
	{ -kFloorExtent, 0.0f, kFloorExtent },
	{ kFloorExtent, 0.0f, -kFloorExtent },
	{ kFloorExtent, 0.0f, kFloorExtent },
}};
const std::array<uint32_t, 6> kFloorIndices = { 0, 1, 2, 3, 2, 1 };
const std::array<DirectX::XMFLOAT3, 8> kHullCorners = {{
	{ -kHullHalfExtent, -kHullHalfExtent, -kHullHalfExtent },
	{ kHullHalfExtent, -kHullHalfExtent, -kHullHalfExtent },
	{ kHullHalfExtent, kHullHalfExtent, -kHullHalfExtent },
	{ -kHullHalfExtent, kHullHalfExtent, -kHullHalfExtent },
	{ -kHullHalfExtent, -kHullHalfExtent, kHullHalfExtent },
	{ kHullHalfExtent, -kHullHalfExtent, kHullHalfExtent },
	{ kHullHalfExtent, kHullHalfExtent, kHullHalfExtent },
	{ -kHullHalfExtent, kHullHalfExtent, kHullHalfExtent },
}};

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

} // namespace

WorldSpawnDesc PhysicsDropScene::MakeSpawnDesc(
	std::string_view name,
	const Transform& transform,
	MeshHandle mesh,
	const MaterialTint& tint) const
{
	WorldSpawnDesc desc{};
	desc.name = name;
	desc.transform = transform;
	desc.renderable.mesh = ToWorldMeshId(mesh);
	desc.renderable.material = ToWorldMaterialId(m_material);
	desc.renderable.submeshIndex = 0;
	desc.renderable.layerMask = WorldLayer::Opaque | WorldLayer::Shadow;
	desc.renderable.visible = true;
	desc.renderable.overrides.baseColor = ToWorldTextureId(m_baseColor);
	if (m_materialConstantsSlot.has_value())
	{
		WorldMaterialParameterBlock materialBlock{};
		materialBlock.bindingSlot = *m_materialConstantsSlot;
		const auto* bytes = reinterpret_cast<const std::byte*>(&tint);
		materialBlock.data.assign(bytes, bytes + sizeof(tint));
		desc.renderable.overrides.parameters.push_back(std::move(materialBlock));
	}

	return desc;
}

Result<void> PhysicsDropScene::EnsureInitialized(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot)
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

	const Result<PrimitiveMeshData> floorMesh = GenerateTriangleMesh({ kFloorCorners, kFloorIndices });
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

	const Result<PrimitiveMeshData> boxMesh = GenerateCubeMesh({ kBoxWidth, kBoxHeight, kBoxDepth, false });
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

	const Result<PrimitiveMeshData> sphereMesh = GenerateSphereMesh({ kSphereRadius, 12, 16 });
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

	const Result<PrimitiveMeshData> capsuleMesh = GenerateCapsuleMesh({ kCapsuleRadius, kCapsuleHalfHeight, 16, 4 });
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

	const Result<PrimitiveMeshData> hullMesh = GenerateConvexHullMesh({ kHullCorners });
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

	m_ready = true;
	return MakeOk();
}

Result<void> PhysicsDropScene::SpawnInto(World& world)
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

	PhysicsBodyDesc floorDesc{};
	floorDesc.motion = PhysicsMotion::Static;
	floorDesc.shape.type = PhysicsShapeType::Mesh;
	floorDesc.shape.points = kFloorCorners;
	floorDesc.shape.indices = kFloorIndices;

	PhysicsBodyDesc boxDesc{};
	boxDesc.motion = PhysicsMotion::Static;
	boxDesc.shape.type = PhysicsShapeType::Box;
	boxDesc.shape.halfExtent = { kBoxWidth * 0.5f, kBoxHeight * 0.5f, kBoxDepth * 0.5f };

	PhysicsBodyDesc sphereDesc{};
	sphereDesc.motion = PhysicsMotion::Dynamic;
	sphereDesc.shape.type = PhysicsShapeType::Sphere;
	sphereDesc.shape.radius = kSphereRadius;

	PhysicsBodyDesc capsuleDesc{};
	capsuleDesc.motion = PhysicsMotion::Dynamic;
	capsuleDesc.shape.type = PhysicsShapeType::Capsule;
	capsuleDesc.shape.radius = kCapsuleRadius;
	capsuleDesc.shape.halfHeight = kCapsuleHalfHeight;

	PhysicsBodyDesc hullDesc{};
	hullDesc.motion = PhysicsMotion::Dynamic;
	hullDesc.shape.type = PhysicsShapeType::ConvexHull;
	hullDesc.shape.points = kHullCorners;

	Transform floorTransform{};
	Transform boxTransform{};
	boxTransform.position = { 0.0f, kBoxHeight * 0.5f, 0.0f };
	Transform sphereTransform{};
	sphereTransform.position = { 0.0f, 4.0f, 0.0f };
	Transform capsuleTransform{};
	capsuleTransform.position = { -1.15f, 4.4f, 0.85f };
	Transform hullTransform{};
	hullTransform.position = { 1.15f, 3.6f, -0.7f };
	DirectX::XMStoreFloat4(
		&hullTransform.rotation,
		DirectX::XMQuaternionRotationRollPitchYaw(0.45f, 0.35f, 0.7f));

	const MaterialTint floorTint{ { 0.42f, 0.48f, 0.40f, 1.0f } };
	const MaterialTint boxTint{ { 0.62f, 0.64f, 0.68f, 1.0f } };
	const MaterialTint sphereTint{ { 0.90f, 0.35f, 0.12f, 1.0f } };
	const MaterialTint capsuleTint{ { 0.25f, 0.45f, 0.86f, 1.0f } };
	const MaterialTint hullTint{ { 0.30f, 0.72f, 0.38f, 1.0f } };
	const EntityId floor = world.Spawn(MakeSpawnDesc("Floor", floorTransform, m_floorMesh, floorTint));
	const EntityId box = world.Spawn(MakeSpawnDesc("Box", boxTransform, m_boxMesh, boxTint));
	const EntityId sphere = world.Spawn(MakeSpawnDesc("Sphere", sphereTransform, m_sphereMesh, sphereTint));
	const EntityId capsule = world.Spawn(MakeSpawnDesc("Capsule", capsuleTransform, m_capsuleMesh, capsuleTint));
	const EntityId hull = world.Spawn(MakeSpawnDesc("Hull", hullTransform, m_hullMesh, hullTint));
	if (auto bound = physics.value.Bind(world, floor, floorDesc); !bound)
	{
		return FailInternal(LogCategory::Physics, bound.error.code, bound.error.message);
	}
	if (auto bound = physics.value.Bind(world, box, boxDesc); !bound)
	{
		return FailInternal(LogCategory::Physics, bound.error.code, bound.error.message);
	}
	if (auto bound = physics.value.Bind(world, sphere, sphereDesc); !bound)
	{
		return FailInternal(LogCategory::Physics, bound.error.code, bound.error.message);
	}
	if (auto bound = physics.value.Bind(world, capsule, capsuleDesc); !bound)
	{
		return FailInternal(LogCategory::Physics, bound.error.code, bound.error.message);
	}
	if (auto bound = physics.value.Bind(world, hull, hullDesc); !bound)
	{
		return FailInternal(LogCategory::Physics, bound.error.code, bound.error.message);
	}

	m_physics = std::move(physics.value);
	m_spawned = true;

	LOG_INFO(LogCategory::Physics, "Physics drop scene spawned a mesh floor, static box, dynamic sphere, capsule, and convex hull");
	return MakeOk();
}

Result<void> PhysicsDropScene::Step(World& world, float fixedDeltaSeconds)
{
	if (!m_spawned)
	{
		return MakeOk();
	}

	return m_physics.Step(world, fixedDeltaSeconds);
}

void PhysicsDropScene::WriteRenderTransforms(World& world, float alpha)
{
	if (!m_spawned)
	{
		return;
	}

	m_physics.WriteTransforms(world, alpha);
}
