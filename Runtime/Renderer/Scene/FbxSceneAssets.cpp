#include "Runtime/Renderer/Scene/FbxSceneAssets.h"

#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Renderer/Material/MaterialSystemServices.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Mesh/MeshSystemServices.h"
#include "Runtime/Renderer/Resource/RenderResourceServices.h"
#include "Runtime/Renderer/Scene/RenderConstantsLayout.h"
#include "Runtime/RHI/Common/RHIInput.h"
#include "Runtime/RHI/Interface/RHICommandList.h"

#include <DirectXMath.h>

#include <cstring>
#include <utility>
#include <vector>

namespace
{
DirectX::XMMATRIX LoadRowMajorMatrix(const float matrix4x4[16])
{
	DirectX::XMFLOAT4X4 loaded{};
	std::memcpy(loaded.m, matrix4x4, sizeof(loaded.m));
	return DirectX::XMLoadFloat4x4(&loaded);
}

void ComputeNodeWorldMatrix(
	std::span<const SceneNodeUpload> nodes,
	uint32_t nodeIndex,
	float outWorldMatrix[16])
{
	const SceneNodeUpload& node = nodes[nodeIndex];
	DirectX::XMMATRIX world = LoadRowMajorMatrix(node.localTransform);

	if (node.parentIndex >= 0)
	{
		float parentWorldMatrix[16]{};
		ComputeNodeWorldMatrix(nodes, static_cast<uint32_t>(node.parentIndex), parentWorldMatrix);
		const DirectX::XMMATRIX parentWorld = Aether::Math::LoadMatrixFromHlsl(parentWorldMatrix);
		// Row-vector order, matching World::GetWorldMatrix: local, then parent.
		world = DirectX::XMMatrixMultiply(world, parentWorld);
	}

	Aether::Math::StoreMatrixForHlsl(outWorldMatrix, world);
}

[[nodiscard]] Result<TextureHandle> TextureAt(
	std::span<const TextureHandle> textures,
	uint32_t index)
{
	if (index >= textures.size() || !textures[index].IsValid())
	{
		return FailInternal<TextureHandle>(LogCategory::Asset, ErrorCode::InvalidArgument,
			"Model texture index is out of range");
	}

	return MakeOk(textures[index]);
}
} // namespace

FbxSceneAssets::FbxSceneAssets() = default;

FbxSceneAssets::~FbxSceneAssets() = default;

Result<void> FbxSceneAssets::EnsureInitialized(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot,
	const SceneModelUpload& model,
	std::span<const TextureHandle> textures,
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

	if (shaderRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"FbxSceneAssets requires a valid shader root");
	}

	if (model.meshes.empty())
	{
		return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FbxSceneAssets model contains no meshes");
	}

	if (model.meshNames.size() != model.meshes.size())
	{
		return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FbxSceneAssets model mesh names do not match meshes");
	}

	std::vector<MeshHandle> uploadedMeshes;
	uploadedMeshes.reserve(model.meshes.size());
	for (const MeshUploadDesc& meshDesc : model.meshes)
	{
		const Result<MeshHandle> meshResult = resources.GetMeshServices().UploadMesh(
			meshDesc,
			frameContext,
			commandList);
		if (!meshResult)
		{
			return MakeFail(meshResult.error.code, meshResult.error.message);
		}

		uploadedMeshes.push_back(meshResult.value);
	}

	m_materialTextures.clear();
	m_materialTextures.resize(model.baseColorBySlot.size());
	m_normalTextures.clear();
	m_normalTextures.resize(model.normalBySlot.size());
	m_specularTextures.clear();
	m_specularTextures.resize(model.specularBySlot.size());
	if (model.baseColorBySlot.size() != model.normalBySlot.size()
		|| model.baseColorBySlot.size() != model.specularBySlot.size())
	{
		return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FbxSceneAssets material texture slots do not match");
	}

	for (uint32_t slotIndex = 0; slotIndex < model.baseColorBySlot.size(); ++slotIndex)
	{
		const Result<TextureHandle> baseColor = TextureAt(textures, model.baseColorBySlot[slotIndex]);
		if (!baseColor)
		{
			return MakeFail(baseColor.error.code, baseColor.error.message);
		}

		m_materialTextures[slotIndex] = baseColor.value;
		if (model.normalBySlot[slotIndex] != kNoSceneTexture)
		{
			const Result<TextureHandle> normal = TextureAt(textures, model.normalBySlot[slotIndex]);
			if (!normal)
			{
				return MakeFail(normal.error.code, normal.error.message);
			}

			m_normalTextures[slotIndex] = normal.value;
		}

		if (model.specularBySlot[slotIndex] == kNoSceneTexture)
		{
			continue;
		}

		const Result<TextureHandle> specular = TextureAt(textures, model.specularBySlot[slotIndex]);
		if (!specular)
		{
			return MakeFail(specular.error.code, specular.error.message);
		}

		m_specularTextures[slotIndex] = specular.value;
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
	m_nodes.reserve(model.nodes.size());
	for (const SceneNodeUpload& node : model.nodes)
	{
		FbxNodeRecord record{};
		record.name = node.name;
		std::memcpy(record.localTransform, node.localTransform, sizeof(record.localTransform));
		record.parentIndex = node.parentIndex;
		m_nodes.push_back(record);
	}

	m_submeshes.clear();
	for (uint32_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
	{
		const SceneNodeUpload& node = model.nodes[nodeIndex];
		if (node.meshIndex < 0)
		{
			continue;
		}

		if (static_cast<uint32_t>(node.meshIndex) >= uploadedMeshes.size())
		{
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"Model node mesh index is out of range");
		}

		const uint32_t meshIndex = static_cast<uint32_t>(node.meshIndex);
		const MeshUploadDesc& meshDesc = model.meshes[meshIndex];
		for (uint32_t submeshIndex = 0; submeshIndex < meshDesc.submeshes.size(); ++submeshIndex)
		{
			const uint32_t materialSlot = meshDesc.submeshes[submeshIndex].materialSlot;

			FbxSubmeshRecord submesh{};
			const std::string& meshName = model.meshNames[meshIndex].empty() ? node.name : model.meshNames[meshIndex];
			submesh.name = meshDesc.submeshes.size() <= 1
				? meshName
				: meshName + " " + std::to_string(submeshIndex);
			submesh.nodeIndex = nodeIndex;
			submesh.mesh = uploadedMeshes[meshIndex];
			submesh.submeshIndex = submeshIndex;
			if (materialSlot < m_materialTextures.size())
			{
				submesh.baseColor = m_materialTextures[materialSlot];
				submesh.normal = m_normalTextures[materialSlot];
				submesh.specular = m_specularTextures[materialSlot];
			}
			else if (!m_materialTextures.empty())
			{
				submesh.baseColor = m_materialTextures.front();
			}
			ComputeNodeWorldMatrix(model.nodes, nodeIndex, submesh.worldMatrix);
			m_submeshes.push_back(submesh);
		}
	}

	if (m_submeshes.empty())
	{
		return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FbxSceneAssets model contains no renderable mesh nodes");
	}

	m_ready = true;
	LOG_INFO_F(LogCategory::Core, "FbxSceneAssets initialized: document {}", model.documentId);
	return MakeOk();
}
