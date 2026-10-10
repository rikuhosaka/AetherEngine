#include "Game/FbxSceneAssets.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Model/Loader/FbxModelFileLoader.h"
#include "Engine/Renderer/Model/Loader/FbxSdkContext.h"
#include "Engine/Renderer/Model/Loader/ModelAssetPath.h"
#include "Engine/Renderer/Model/Loader/ModelLoadTypes.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Texture/Loader/TextureLoadTypes.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/Renderer/Texture/TextureUpload.h"
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/World/Transform.h"
#include "Game/WorldRenderBridge.h"

#include <DirectXMath.h>

#include <cmath>
#include <cstring>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
DirectX::XMMATRIX LoadFbxRowMajorMatrix(const float matrix4x4[16])
{
	DirectX::XMFLOAT4X4 loaded{};
	std::memcpy(loaded.m, matrix4x4, sizeof(loaded.m));
	return DirectX::XMLoadFloat4x4(&loaded);
}

void ComputeNodeWorldMatrix(
	const ModelAssetData& assetData,
	uint32_t nodeIndex,
	float outWorldMatrix[16])
{
	const ModelNodeData& node = assetData.nodes[nodeIndex];
	DirectX::XMMATRIX world = LoadFbxRowMajorMatrix(node.localTransform);

	if (node.parentIndex >= 0)
	{
		float parentWorldMatrix[16]{};
		ComputeNodeWorldMatrix(assetData, static_cast<uint32_t>(node.parentIndex), parentWorldMatrix);
		const DirectX::XMMATRIX parentWorld = Aether::Math::LoadMatrixFromHlsl(parentWorldMatrix);
		// Row-vector order, matching World::GetWorldMatrix: local, then parent.
		world = DirectX::XMMatrixMultiply(world, parentWorld);
	}

	Aether::Math::StoreMatrixForHlsl(outWorldMatrix, world);
}

[[nodiscard]] Result<Transform> TransformFromDxMatrix(DirectX::FXMMATRIX matrix)
{
	DirectX::XMVECTOR scale{};
	DirectX::XMVECTOR rotation{};
	DirectX::XMVECTOR translation{};
	if (!DirectX::XMMatrixDecompose(&scale, &rotation, &translation, matrix))
	{
		return FailInternal<Transform>(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FBX node transform could not be decomposed into translation, rotation, and scale");
	}

	Transform transform{};
	DirectX::XMStoreFloat3(&transform.position, translation);
	DirectX::XMStoreFloat4(&transform.rotation, rotation);
	DirectX::XMStoreFloat3(&transform.scale, scale);
	return MakeOk(std::move(transform));
}

[[nodiscard]] bool MatricesNearlyEqual(DirectX::FXMMATRIX left, DirectX::FXMMATRIX right)
{
	DirectX::XMFLOAT4X4 leftStored{};
	DirectX::XMFLOAT4X4 rightStored{};
	DirectX::XMStoreFloat4x4(&leftStored, left);
	DirectX::XMStoreFloat4x4(&rightStored, right);
	constexpr float kEpsilon = 0.001f;
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			if (std::fabs(leftStored.m[row][column] - rightStored.m[row][column]) > kEpsilon)
			{
				return false;
			}
		}
	}

	return true;
}

[[nodiscard]] Result<MeshHandle> UploadModelMesh(
	MeshSystemServices& meshServices,
	const ModelMeshData& meshData,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	std::vector<SubmeshRange> submeshRanges;
	submeshRanges.reserve(meshData.submeshes.size());
	for (const ModelSubMeshData& submesh : meshData.submeshes)
	{
		SubmeshRange range{};
		range.indexStart = submesh.indexStart;
		range.indexCount = submesh.indexCount;
		range.materialSlot = submesh.materialSlot;
		submeshRanges.push_back(range);
	}

	MeshUploadDesc meshDesc{};
	meshDesc.vertices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(meshData.vertices.data()),
		meshData.vertices.size() * sizeof(BasicVertex));
	meshDesc.indices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(meshData.indices.data()),
		meshData.indices.size() * sizeof(uint32_t));
	meshDesc.vertexStride = sizeof(BasicVertex);
	meshDesc.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
	meshDesc.indexCount = static_cast<uint32_t>(meshData.indices.size());
	meshDesc.indexFormat = IndexFormat::R32_UINT;
	meshDesc.layoutId = VertexLayoutId::Basic;
	meshDesc.submeshes = std::move(submeshRanges);
	meshDesc.bounds = meshData.bounds;
	meshDesc.DebugName = meshData.name.c_str();

	return meshServices.UploadMesh(meshDesc, frameContext, commandList);
}

// The FBX loader resolves texture paths to absolute paths, but TextureSystemServices only
// accepts paths relative to Assets/. Returns an empty path if the texture lies outside Assets/.
[[nodiscard]] std::filesystem::path MakeAssetsRelativePath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& texturePath)
{
	if (!texturePath.is_absolute())
	{
		return texturePath;
	}

	std::error_code errorCode{};
	const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(assetsRoot, errorCode);
	if (errorCode)
	{
		return {};
	}

	const std::filesystem::path relativePath =
		std::filesystem::relative(texturePath, canonicalRoot, errorCode);
	if (errorCode || relativePath.empty())
	{
		return {};
	}

	for (const std::filesystem::path& part : relativePath)
	{
		if (part == "..")
		{
			return {};
		}
	}

	return relativePath;
}
} // namespace

Result<void> FbxSceneAssets::LoadTextureForSlot(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& texturePath,
	TextureHandle& outTexture)
{
	const std::filesystem::path relativePath = MakeAssetsRelativePath(assetsRoot, texturePath);
	if (!texturePath.empty() && relativePath.empty())
	{
		LOG_WARN_F(LogCategory::Asset,
			"FBX texture is outside Assets/, using white fallback: {}", texturePath.string());
	}

	if (!relativePath.empty())
	{
		TextureLoadDesc textureLoadDesc{};
		textureLoadDesc.relativePath = relativePath;
		textureLoadDesc.colorSpace = TextureColorSpace::Srgb;
		textureLoadDesc.generateMips = true;
		textureLoadDesc.debugName = "FbxBaseColor";

		const Result<TextureHandle> textureResult = resources.GetTextureServices().GetOrLoadTexture(
			textureLoadDesc,
			frameContext,
			commandList);
		if (textureResult)
		{
			outTexture = textureResult.value;
			return MakeOk();
		}
	}

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

	const Result<TextureHandle> uploadResult = resources.GetTextureServices().UploadTexture(
		textureDesc,
		frameContext,
		commandList);
	if (!uploadResult)
	{
		return MakeFail(uploadResult.error.code, uploadResult.error.message);
	}

	outTexture = uploadResult.value;
	return MakeOk();
}

Result<void> FbxSceneAssets::EnsureInitialized(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& modelRelativePath,
	const LitMaterialConstants& materialConstants)
{
	if (m_ready)
	{
		return MakeOk();
	}

	if (commandList == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"FbxSceneAssets requires a valid command list");
	}

	if (shaderRoot.empty() || assetsRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"FbxSceneAssets requires valid shader and assets roots");
	}

	auto sdkResult = FbxSdkContext::Create();
	if (!sdkResult)
	{
		return MakeFail(sdkResult.error.code, sdkResult.error.message);
	}
	m_fbxSdkContext = std::move(sdkResult.value);

	const Result<std::filesystem::path> resolveResult =
		ResolveModelAssetPath(assetsRoot, modelRelativePath);
	if (!resolveResult)
	{
		return MakeFail(resolveResult.error.code, resolveResult.error.message);
	}

	const Result<ModelAssetData> loadResult = LoadModelAssetDataFromFile(
		resolveResult.value,
		m_fbxSdkContext);
	if (!loadResult)
	{
		return MakeFail(loadResult.error.code, loadResult.error.message);
	}

	const ModelAssetData& assetData = loadResult.value;
	if (assetData.meshes.empty())
	{
		return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FbxSceneAssets model contains no meshes");
	}

	std::unordered_map<uint32_t, MeshHandle> uploadedMeshes;
	uploadedMeshes.reserve(assetData.meshes.size());
	for (uint32_t meshIndex = 0; meshIndex < assetData.meshes.size(); ++meshIndex)
	{
		const Result<MeshHandle> meshResult = UploadModelMesh(
			resources.GetMeshServices(),
			assetData.meshes[meshIndex],
			frameContext,
			commandList);
		if (!meshResult)
		{
			return MakeFail(meshResult.error.code, meshResult.error.message);
		}

		uploadedMeshes.emplace(meshIndex, meshResult.value);
	}

	m_materialTextures.clear();
	m_materialTextures.resize(assetData.materialSlots.size());
	m_normalTextures.clear();
	m_normalTextures.resize(assetData.materialSlots.size());
	for (uint32_t slotIndex = 0; slotIndex < assetData.materialSlots.size(); ++slotIndex)
	{
		if (auto textureResult = LoadTextureForSlot(
				resources,
				frameContext,
				commandList,
				assetsRoot,
				assetData.materialSlots[slotIndex].diffuseTexturePath,
				m_materialTextures[slotIndex]);
			!textureResult)
		{
			return textureResult;
		}

		if (assetData.materialSlots[slotIndex].normalTexturePath.empty())
		{
			continue;
		}

		if (auto normalResult = LoadTextureForSlot(
				resources,
				frameContext,
				commandList,
				assetsRoot,
				assetData.materialSlots[slotIndex].normalTexturePath,
				m_normalTextures[slotIndex]);
			!normalResult)
		{
			return normalResult;
		}
	}

	MaterialCreateDesc materialDesc{};
	materialDesc.vertexShaderPath = shaderRoot / "LitBasicVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "LitBasicPS.hlsl";
	materialDesc.vertexEntryPoint = "LitBasicVS";
	materialDesc.pixelEntryPoint = "LitBasicPS";
	materialDesc.inputLayout = InputLayoutType::Basic;
	materialDesc.requiredLayout = VertexLayoutId::Basic;

	const Result<MaterialHandle> materialResult =
		resources.GetMaterialServices().CreateMaterial(materialDesc);
	if (!materialResult)
	{
		return MakeFail(materialResult.error.code, materialResult.error.message);
	}
	m_material = materialResult.value;
	m_materialConstants = materialConstants;

	const Material* material = resources.GetMaterialServices().GetMaterial(m_material);
	if (material == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"FbxSceneAssets failed to resolve created material");
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

	m_nodes.clear();
	m_nodes.reserve(assetData.nodes.size());
	for (const ModelNodeData& node : assetData.nodes)
	{
		const Result<Transform> localResult = TransformFromDxMatrix(LoadFbxRowMajorMatrix(node.localTransform));
		if (!localResult)
		{
			return MakeFail(localResult.error.code, localResult.error.message);
		}

		FbxNodeSpawn record{};
		record.local = localResult.value;
		record.parentIndex = node.parentIndex;
		m_nodes.push_back(record);
	}

	m_submeshes.clear();
	for (uint32_t nodeIndex = 0; nodeIndex < assetData.nodes.size(); ++nodeIndex)
	{
		const ModelNodeData& node = assetData.nodes[nodeIndex];
		if (node.meshIndex < 0)
		{
			continue;
		}

		const auto meshIt = uploadedMeshes.find(static_cast<uint32_t>(node.meshIndex));
		if (meshIt == uploadedMeshes.end())
		{
			continue;
		}

		const ModelMeshData& meshData = assetData.meshes[static_cast<std::size_t>(node.meshIndex)];
		for (uint32_t submeshIndex = 0; submeshIndex < meshData.submeshes.size(); ++submeshIndex)
		{
			const uint32_t materialSlot = meshData.submeshes[submeshIndex].materialSlot;

			FbxSubmeshSpawn submesh{};
			submesh.nodeIndex = nodeIndex;
			submesh.mesh = meshIt->second;
			submesh.submeshIndex = submeshIndex;
			if (materialSlot < m_materialTextures.size())
			{
				submesh.baseColor = m_materialTextures[materialSlot];
				submesh.normal = m_normalTextures[materialSlot];
			}
			else if (!m_materialTextures.empty())
			{
				submesh.baseColor = m_materialTextures.front();
			}
			ComputeNodeWorldMatrix(assetData, nodeIndex, submesh.worldMatrix);
			m_submeshes.push_back(submesh);
		}
	}

	if (m_submeshes.empty())
	{
		return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FbxSceneAssets model contains no renderable mesh nodes");
	}

	m_ready = true;
	LOG_INFO_F(LogCategory::Core, "FbxSceneAssets initialized: {}", modelRelativePath.string());
	return MakeOk();
}

Result<void> FbxSceneAssets::SpawnInto(World& world)
{
	if (m_spawned)
	{
		return MakeOk();
	}

	if (!m_ready)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"FbxSceneAssets::SpawnInto requires initialized GPU assets");
	}

	std::vector<EntityId> nodeEntities;
	nodeEntities.reserve(m_nodes.size());
	for (const FbxNodeSpawn& node : m_nodes)
	{
		WorldSpawnDesc desc{};
		desc.transform = node.local;
		desc.renderable.visible = false;
		nodeEntities.push_back(world.Spawn(std::move(desc)));
	}

	for (size_t nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex)
	{
		const int32_t parentIndex = m_nodes[nodeIndex].parentIndex;
		if (parentIndex < 0)
		{
			continue;
		}

		if (static_cast<size_t>(parentIndex) >= nodeEntities.size() ||
			!world.SetParent(nodeEntities[nodeIndex], nodeEntities[static_cast<size_t>(parentIndex)]))
		{
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"FBX node parent could not be attached");
		}
	}

	for (const FbxSubmeshSpawn& submesh : m_submeshes)
	{
		if (submesh.nodeIndex >= nodeEntities.size())
		{
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"FBX submesh node index is out of range");
		}

		WorldSpawnDesc desc{};
		desc.parent = nodeEntities[submesh.nodeIndex];
		desc.renderable.mesh = ToWorldMeshId(submesh.mesh);
		desc.renderable.material = ToWorldMaterialId(m_material);
		desc.renderable.submeshIndex = submesh.submeshIndex;
		desc.renderable.layerMask = WorldLayer::Opaque | WorldLayer::Shadow;
		desc.renderable.visible = true;
		desc.renderable.overrides.baseColor = ToWorldTextureId(submesh.baseColor);
		desc.renderable.overrides.normal = ToWorldTextureId(submesh.normal);
		if (m_materialConstantsSlot.has_value())
		{
			WorldMaterialParameterBlock materialBlock{};
			materialBlock.bindingSlot = *m_materialConstantsSlot;
			const auto* bytes = reinterpret_cast<const std::byte*>(&m_materialConstants);
			materialBlock.data.assign(bytes, bytes + sizeof(m_materialConstants));
			desc.renderable.overrides.parameters.push_back(std::move(materialBlock));
		}

		const EntityId submeshEntity = world.Spawn(std::move(desc));
		const DirectX::XMMATRIX resolved = world.GetWorldMatrix(submeshEntity);
		const DirectX::XMMATRIX baked = Aether::Math::LoadMatrixFromHlsl(submesh.worldMatrix);
		if (!MatricesNearlyEqual(resolved, baked))
		{
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"FBX hierarchy world matrix does not match the baked node transform");
		}
	}

	m_spawned = true;
	return MakeOk();
}
