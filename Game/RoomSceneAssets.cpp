#include "Game/RoomSceneAssets.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/PrimitiveMesh.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/World/Transform.h"

#include <DirectXMath.h>

#include <array>
#include <utility>

namespace
{
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

[[nodiscard]] DirectX::XMFLOAT4 QuaternionFromAxisAngle(DirectX::FXMVECTOR axis, float angleRadians)
{
	DirectX::XMFLOAT4 quaternion{};
	DirectX::XMStoreFloat4(&quaternion, DirectX::XMQuaternionRotationAxis(axis, angleRadians));
	return quaternion;
}

} // namespace

Result<void> RoomSceneAssets::EnsureInitialized(
	Renderer& renderer,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot,
	const RoomDimensions& dimensions,
	const RoomMaterialConstants& materialConstants)
{
	if (m_ready)
	{
		return MakeOk();
	}

	if (commandList == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RoomSceneAssets requires a valid command list");
	}

	if (shaderRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RoomSceneAssets requires a valid shader root path");
	}

	if (dimensions.width <= 0.0f || dimensions.depth <= 0.0f || dimensions.height <= 0.0f)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RoomSceneAssets requires positive room dimensions");
	}

	RenderResourceServices* resources = renderer.GetResourceServices();
	if (resources == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"RoomSceneAssets requires renderer resource services");
	}

	const Result<PrimitiveMeshData> planeResult = GeneratePlaneMesh();
	if (!planeResult)
	{
		return MakeFail(planeResult.error.code, planeResult.error.message);
	}

	const MeshUploadDesc meshDesc = MakePrimitiveMeshUploadDesc(planeResult.value, "UnitPlane");
	const Result<MeshHandle> meshResult =
		resources->GetMeshServices().UploadMesh(meshDesc, frameContext, commandList);
	if (!meshResult)
	{
		return MakeFail(meshResult.error.code, meshResult.error.message);
	}
	m_planeMesh = meshResult.value;

	const Result<TextureHandle> textureResult = UploadWhiteTexture(*resources, frameContext, commandList);
	if (!textureResult)
	{
		return MakeFail(textureResult.error.code, textureResult.error.message);
	}
	m_baseColor = textureResult.value;

	MaterialCreateDesc materialDesc{};
	materialDesc.vertexShaderPath = shaderRoot / "LitBasicVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "LitBasicPS.hlsl";
	materialDesc.vertexEntryPoint = "LitBasicVS";
	materialDesc.pixelEntryPoint = "LitBasicPS";
	materialDesc.inputLayout = InputLayoutType::Basic;
	materialDesc.requiredLayout = VertexLayoutId::Basic;

	const Result<MaterialHandle> materialResult =
		resources->GetMaterialServices().CreateMaterial(materialDesc);
	if (!materialResult)
	{
		return MakeFail(materialResult.error.code, materialResult.error.message);
	}
	m_material = materialResult.value;
	m_materialConstants = materialConstants;
	m_dimensions = dimensions;

	const Material* material = resources->GetMaterialServices().GetMaterial(m_material);
	if (material == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"RoomSceneAssets failed to resolve created material");
	}

	m_materialConstantsSlot.reset();
	for (uint32_t layoutIndex = 0; layoutIndex < material->constantLayout.size(); ++layoutIndex)
	{
		const ShaderConstantBuffer& layout = material->constantLayout[layoutIndex];
		if (layout.Register == RenderRegisters::MaterialConstants && layout.Space == 0)
		{
			m_materialConstantsSlot = layoutIndex;
			break;
		}
	}

	m_ready = true;
	LOG_INFO_F(LogCategory::Core, "RoomSceneAssets initialized from unit planes: {} x {} x {} m",
		dimensions.width, dimensions.depth, dimensions.height);
	return MakeOk();
}

Result<void> RoomSceneAssets::SpawnInto(World& world)
{
	if (m_spawned)
	{
		return MakeOk();
	}

	if (!m_ready)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"RoomSceneAssets::SpawnInto requires initialized GPU assets");
	}

	using namespace DirectX;

	const float width = m_dimensions.width;
	const float depth = m_dimensions.depth;
	const float height = m_dimensions.height;
	const XMVECTOR xAxis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	const XMVECTOR zAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	const auto spawnPlane = [this, &world](const Transform& transform) {
		WorldSpawnDesc desc{};
		desc.transform = transform;
		desc.renderable.mesh = m_planeMesh;
		desc.renderable.material = m_material;
		desc.renderable.submeshIndex = 0;
		desc.renderable.layerMask = RenderLayer::Opaque | RenderLayer::Shadow;
		desc.renderable.visible = true;
		desc.renderable.overrides.baseColor = m_baseColor;
		desc.renderable.overrides.normal = {};
		if (m_materialConstantsSlot.has_value())
		{
			WorldMaterialParameterBlock materialBlock{};
			materialBlock.bindingSlot = *m_materialConstantsSlot;
			const auto* bytes = reinterpret_cast<const std::byte*>(&m_materialConstants);
			materialBlock.data.assign(bytes, bytes + sizeof(m_materialConstants));
			desc.renderable.overrides.parameters.push_back(std::move(materialBlock));
		}

		world.Spawn(std::move(desc));
	};

	Transform floor{};
	floor.scale = { width, 1.0f, depth };
	spawnPlane(floor);

	Transform ceiling{};
	ceiling.position = { 0.0f, height, 0.0f };
	ceiling.rotation = QuaternionFromAxisAngle(xAxis, XM_PI);
	ceiling.scale = { width, 1.0f, depth };
	spawnPlane(ceiling);

	Transform northWall{};
	northWall.position = { 0.0f, height * 0.5f, depth * 0.5f };
	northWall.rotation = QuaternionFromAxisAngle(xAxis, -XM_PIDIV2);
	northWall.scale = { width, 1.0f, height };
	spawnPlane(northWall);

	Transform southWall{};
	southWall.position = { 0.0f, height * 0.5f, -depth * 0.5f };
	southWall.rotation = QuaternionFromAxisAngle(xAxis, XM_PIDIV2);
	southWall.scale = { width, 1.0f, height };
	spawnPlane(southWall);

	Transform eastWall{};
	eastWall.position = { width * 0.5f, height * 0.5f, 0.0f };
	eastWall.rotation = QuaternionFromAxisAngle(zAxis, XM_PIDIV2);
	eastWall.scale = { height, 1.0f, depth };
	spawnPlane(eastWall);

	Transform westWall{};
	westWall.position = { -width * 0.5f, height * 0.5f, 0.0f };
	westWall.rotation = QuaternionFromAxisAngle(zAxis, -XM_PIDIV2);
	westWall.scale = { height, 1.0f, depth };
	spawnPlane(westWall);

	m_spawned = true;
	return MakeOk();
}
