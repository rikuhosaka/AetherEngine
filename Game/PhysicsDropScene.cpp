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
constexpr float kBoxWidth = 4.0f;
constexpr float kBoxHeight = 1.0f;
constexpr float kBoxDepth = 4.0f;
constexpr float kSphereRadius = 0.5f;

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

void WritePose(World& world, EntityId entity, const PhysicsPose& pose)
{
	Transform* transform = world.GetTransform(entity);
	if (transform == nullptr)
	{
		return;
	}

	transform->position = pose.position;
	transform->rotation = pose.rotation;
}
} // namespace

WorldSpawnDesc PhysicsDropScene::MakeSpawnDesc(
	const Transform& transform,
	MeshHandle mesh,
	const MaterialTint& tint) const
{
	WorldSpawnDesc desc{};
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

	const Result<PrimitiveMeshData> boxMesh = GenerateCubeMesh({ kBoxWidth, kBoxHeight, kBoxDepth, false });
	if (!boxMesh)
	{
		return MakeFail(boxMesh.error.code, boxMesh.error.message);
	}

	const Result<MeshHandle> boxUpload = resources.GetMeshServices().UploadMesh(
		MakePrimitiveMeshUploadDesc(boxMesh.value, "PhysicsDropBox"),
		frameContext,
		commandList);
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

	const Result<MeshHandle> sphereUpload = resources.GetMeshServices().UploadMesh(
		MakePrimitiveMeshUploadDesc(sphereMesh.value, "PhysicsDropSphere"),
		frameContext,
		commandList);
	if (!sphereUpload)
	{
		return MakeFail(sphereUpload.error.code, sphereUpload.error.message);
	}
	m_sphereMesh = sphereUpload.value;

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

	Result<PhysicsWorld> physics = PhysicsWorld::Create();
	if (!physics)
	{
		return FailInternal(LogCategory::Physics, physics.error.code, physics.error.message);
	}

	PhysicsBoxDesc boxDesc{};
	boxDesc.halfExtent = { kBoxWidth * 0.5f, kBoxHeight * 0.5f, kBoxDepth * 0.5f };
	boxDesc.position = { 0.0f, kBoxHeight * 0.5f, 0.0f };
	const Result<PhysicsBodyId> boxBody = physics.value.CreateBox(boxDesc);
	if (!boxBody)
	{
		return FailInternal(LogCategory::Physics, boxBody.error.code, boxBody.error.message);
	}

	PhysicsSphereDesc sphereDesc{};
	sphereDesc.radius = kSphereRadius;
	sphereDesc.position = { 0.0f, 4.0f, 0.0f };
	const Result<PhysicsBodyId> sphereBody = physics.value.CreateSphere(sphereDesc);
	if (!sphereBody)
	{
		return FailInternal(LogCategory::Physics, sphereBody.error.code, sphereBody.error.message);
	}

	Transform boxTransform{};
	boxTransform.position = boxDesc.position;
	Transform sphereTransform{};
	sphereTransform.position = sphereDesc.position;

	const MaterialTint boxTint{ { 0.62f, 0.64f, 0.68f, 1.0f } };
	const MaterialTint sphereTint{ { 0.90f, 0.35f, 0.12f, 1.0f } };
	m_box.entity = world.Spawn(MakeSpawnDesc(boxTransform, m_boxMesh, boxTint));
	m_box.body = boxBody.value;
	m_sphere.entity = world.Spawn(MakeSpawnDesc(sphereTransform, m_sphereMesh, sphereTint));
	m_sphere.body = sphereBody.value;
	m_physics = std::move(physics.value);
	m_spawned = true;

	LOG_INFO(LogCategory::Physics, "Physics drop scene spawned a static box and a dynamic sphere");
	return MakeOk();
}

Result<void> PhysicsDropScene::Step(float fixedDeltaSeconds)
{
	if (!m_spawned)
	{
		return MakeOk();
	}

	return m_physics.Step(fixedDeltaSeconds);
}

void PhysicsDropScene::WriteRenderTransforms(World& world, float alpha)
{
	if (!m_spawned)
	{
		return;
	}

	const BodyBinding bindings[] = { m_box, m_sphere };
	for (const BodyBinding& binding : bindings)
	{
		PhysicsPose pose{};
		if (!m_physics.Sample(binding.body, alpha, pose))
		{
			continue;
		}

		WritePose(world, binding.entity, pose);
	}
}
