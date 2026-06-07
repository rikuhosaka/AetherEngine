#include "Game/FbxSceneAssets.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Model/Loader/FbxModelFileLoader.h"
#include "Engine/Renderer/Model/Loader/FbxSdkContext.h"
#include "Engine/Renderer/Model/Loader/ModelAssetPath.h"
#include "Engine/Renderer/Model/Loader/ModelLoadTypes.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Texture/Loader/TextureLoadTypes.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/Renderer/Texture/TextureUpload.h"
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Interface/RHICommandList.h"

#include <DirectXMath.h>

#include <cstring>
#include <unordered_map>
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
		world = DirectX::XMMatrixMultiply(parentWorld, world);
	}

	Aether::Math::StoreMatrixForHlsl(outWorldMatrix, world);
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
} // namespace

Result<void> FbxSceneAssets::LoadTextureForSlot(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& texturePath,
	TextureHandle& outTexture)
{
	if (!texturePath.empty())
	{
		TextureLoadDesc textureLoadDesc{};
		textureLoadDesc.relativePath = texturePath;
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
	Renderer& renderer,
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

	RenderResourceServices* resources = renderer.GetResourceServices();
	if (resources == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"FbxSceneAssets requires renderer resource services");
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
			resources->GetMeshServices(),
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
	for (uint32_t slotIndex = 0; slotIndex < assetData.materialSlots.size(); ++slotIndex)
	{
		if (auto textureResult = LoadTextureForSlot(
				*resources,
				frameContext,
				commandList,
				assetData.materialSlots[slotIndex].diffuseTexturePath,
				m_materialTextures[slotIndex]);
			!textureResult)
		{
			return textureResult;
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
		resources->GetMaterialServices().CreateMaterial(materialDesc);
	if (!materialResult)
	{
		return MakeFail(materialResult.error.code, materialResult.error.message);
	}
	m_material = materialResult.value;
	m_materialConstants = materialConstants;

	m_instances.clear();
	uint32_t nextObjectId = 1;
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

			FbxSceneInstance instance{};
			instance.objectId.Index = nextObjectId++;
			instance.objectId.Generation = 1;
			instance.mesh = meshIt->second;
			instance.submeshIndex = submeshIndex;
			instance.baseColor = materialSlot < m_materialTextures.size()
				? m_materialTextures[materialSlot]
				: m_materialTextures.front();
			ComputeNodeWorldMatrix(assetData, nodeIndex, instance.worldMatrix);
			m_instances.push_back(instance);
		}
	}

	if (m_instances.empty())
	{
		return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FbxSceneAssets model contains no renderable mesh nodes");
	}

	m_ready = true;
	LOG_INFO_F(LogCategory::Core, "FbxSceneAssets initialized: {}", modelRelativePath.string());
	return MakeOk();
}

void FbxSceneAssets::FillExtractedObjects(std::vector<ExtractedObject>& outObjects) const
{
	if (!m_ready)
	{
		return;
	}

	outObjects.reserve(outObjects.size() + m_instances.size());
	for (const FbxSceneInstance& sceneInstance : m_instances)
	{
		ExtractedObject object{};
		object.objectId = sceneInstance.objectId;
		object.mesh = sceneInstance.mesh;
		object.material = m_material;
		object.submeshIndex = sceneInstance.submeshIndex;
		object.layerMask = RenderLayer::Opaque;
		object.visible = true;
		std::memcpy(object.worldMatrix, sceneInstance.worldMatrix, sizeof(object.worldMatrix));

		object.overrides.baseColor = sceneInstance.baseColor;
		object.overrides.normal = {};
		MaterialParameterBlock materialBlock{};
		materialBlock.bindingSlot = 0;
		materialBlock.data = std::span<const std::byte>(
			reinterpret_cast<const std::byte*>(&m_materialConstants),
			sizeof(m_materialConstants));
		object.overrides.parameters.push_back(materialBlock);

		outObjects.push_back(object);
	}
}
