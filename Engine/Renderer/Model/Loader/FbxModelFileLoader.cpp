#include "Engine/Renderer/Model/Loader/FbxModelFileLoader.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"

#include <fbxsdk.h>
#include <fbxsdk/scene/fbxaxissystem.h>
#include <fbxsdk/scene/shading/fbxfiletexture.h>
#include <fbxsdk/scene/shading/fbxsurfacematerial.h>
#include <fbxsdk/utils/fbxgeometryconverter.h>
#include <fbxsdk/utils/fbxrootnodeutility.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <system_error>
#include <unordered_map>

namespace
{
[[nodiscard]] std::filesystem::path CanonicalizeIfPossible(const std::filesystem::path& path)
{
	std::error_code errorCode{};
	const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
	return errorCode ? path : canonicalPath;
}

void CopyMatrixFromFbx(float* destination, const FbxAMatrix& source)
{
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			destination[(row * 4) + column] = static_cast<float>(source.Get(row, column));
		}
	}
}

void MakeIdentityMatrix(float* destination)
{
	std::memset(destination, 0, sizeof(float) * 16);
	destination[0] = 1.0f;
	destination[5] = 1.0f;
	destination[10] = 1.0f;
	destination[15] = 1.0f;
}

void ExpandBounds(MeshBounds& bounds, const float position[3])
{
	bounds.minX = (std::min)(bounds.minX, position[0]);
	bounds.minY = (std::min)(bounds.minY, position[1]);
	bounds.minZ = (std::min)(bounds.minZ, position[2]);
	bounds.maxX = (std::max)(bounds.maxX, position[0]);
	bounds.maxY = (std::max)(bounds.maxY, position[1]);
	bounds.maxZ = (std::max)(bounds.maxZ, position[2]);
}

void ResetBounds(MeshBounds& bounds)
{
	const float maxValue = (std::numeric_limits<float>::max)();
	bounds.minX = bounds.minY = bounds.minZ = maxValue;
	bounds.maxX = bounds.maxY = bounds.maxZ = std::numeric_limits<float>::lowest();
}

[[nodiscard]] bool BoundsAreValid(const MeshBounds& bounds)
{
	return bounds.minX <= bounds.maxX && bounds.minY <= bounds.maxY && bounds.minZ <= bounds.maxZ;
}

[[nodiscard]] std::filesystem::path ResolveDiffuseTexturePath(
	const std::filesystem::path& fbxDirectory,
	const std::filesystem::path& fbxStem,
	const std::string& texturePathFromFbx)
{
	if (texturePathFromFbx.empty())
	{
		return {};
	}

	const std::filesystem::path relativeTexturePath(texturePathFromFbx);
	const std::filesystem::path fbmDirectory = fbxDirectory / (fbxStem.string() + ".fbm");

	const auto tryCandidate = [](const std::filesystem::path& candidate) -> std::filesystem::path
	{
		std::error_code errorCode{};
		if (!candidate.empty() && std::filesystem::exists(candidate, errorCode) && !errorCode)
		{
			return CanonicalizeIfPossible(candidate);
		}

		return {};
	};

	if (relativeTexturePath.is_absolute())
	{
		if (const std::filesystem::path resolved = tryCandidate(relativeTexturePath); !resolved.empty())
		{
			return resolved;
		}
	}

	const std::filesystem::path fileName = relativeTexturePath.filename();
	const std::filesystem::path candidates[] = {
		fbxDirectory / relativeTexturePath,
		fbxDirectory / fileName,
		fbmDirectory / relativeTexturePath,
		fbmDirectory / fileName,
	};

	for (const std::filesystem::path& candidate : candidates)
	{
		if (const std::filesystem::path resolved = tryCandidate(candidate); !resolved.empty())
		{
			return resolved;
		}
	}

	return {};
}

class MaterialSlotRegistry
{
public:
	[[nodiscard]] uint32_t GetOrAdd(
		const FbxSurfaceMaterial* material,
		const std::filesystem::path& fbxDirectory,
		const std::filesystem::path& fbxStem,
		std::vector<ModelMaterialSlotData>& materialSlots)
	{
		if (material == nullptr)
		{
			return GetOrAddDefault(materialSlots);
		}

		const auto existing = m_lookup.find(material);
		if (existing != m_lookup.end())
		{
			return existing->second;
		}

		ModelMaterialSlotData slot{};
		slot.name = material->GetName();

		const FbxProperty diffuseProperty =
			material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		const int textureCount = diffuseProperty.GetSrcObjectCount<FbxTexture>();
		for (int textureIndex = 0; textureIndex < textureCount; ++textureIndex)
		{
			const FbxFileTexture* fileTexture =
				FbxCast<FbxFileTexture>(diffuseProperty.GetSrcObject<FbxTexture>(textureIndex));
			if (fileTexture == nullptr)
			{
				continue;
			}

			const char* fileName = fileTexture->GetFileName();
			if (fileName == nullptr || fileName[0] == '\0')
			{
				fileName = fileTexture->GetRelativeFileName();
			}

			if (fileName != nullptr && fileName[0] != '\0')
			{
				slot.diffuseTexturePath =
					ResolveDiffuseTexturePath(fbxDirectory, fbxStem, fileName);
				break;
			}
		}

		const uint32_t slotIndex = static_cast<uint32_t>(materialSlots.size());
		materialSlots.push_back(std::move(slot));
		m_lookup.emplace(material, slotIndex);
		return slotIndex;
	}

private:
	[[nodiscard]] uint32_t GetOrAddDefault(std::vector<ModelMaterialSlotData>& materialSlots)
	{
		constexpr const char* kDefaultMaterialKey = "__default__";
		const auto existing = m_namedLookup.find(kDefaultMaterialKey);
		if (existing != m_namedLookup.end())
		{
			return existing->second;
		}

		const uint32_t slotIndex = static_cast<uint32_t>(materialSlots.size());
		materialSlots.push_back(ModelMaterialSlotData{ "Default", {} });
		m_namedLookup.emplace(kDefaultMaterialKey, slotIndex);
		return slotIndex;
	}

	std::unordered_map<const FbxSurfaceMaterial*, uint32_t> m_lookup;
	std::unordered_map<std::string, uint32_t> m_namedLookup;
};

[[nodiscard]] bool ExtractPolygonVertexNormal(
	FbxMesh* mesh,
	int polygonIndex,
	int positionInPolygon,
	float outNormal[3])
{
	FbxVector4 normal{};
	if (mesh->GetPolygonVertexNormal(polygonIndex, positionInPolygon, normal))
	{
		outNormal[0] = static_cast<float>(normal[0]);
		outNormal[1] = static_cast<float>(normal[1]);
		outNormal[2] = static_cast<float>(normal[2]);
		return true;
	}

	return false;
}

[[nodiscard]] bool ExtractPolygonVertexUV(
	FbxMesh* mesh,
	int polygonIndex,
	int positionInPolygon,
	float outUv[2])
{
	const FbxGeometryElementUV* uvElement = mesh->GetElementUV(0);
	if (uvElement == nullptr)
	{
		outUv[0] = 0.0f;
		outUv[1] = 0.0f;
		return false;
	}

	const FbxGeometryElement::EMappingMode mappingMode = uvElement->GetMappingMode();
	const FbxGeometryElement::EReferenceMode referenceMode = uvElement->GetReferenceMode();

	int directIndex = 0;
	if (mappingMode == FbxGeometryElement::eByControlPoint)
	{
		const int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, positionInPolygon);
		if (controlPointIndex < 0)
		{
			outUv[0] = 0.0f;
			outUv[1] = 0.0f;
			return false;
		}

		directIndex = controlPointIndex;
		if (referenceMode == FbxGeometryElement::eIndexToDirect)
		{
			directIndex = uvElement->GetIndexArray().GetAt(controlPointIndex);
		}
	}
	else if (mappingMode == FbxGeometryElement::eByPolygonVertex)
	{
		directIndex = mesh->GetTextureUVIndex(polygonIndex, positionInPolygon);
		if (directIndex < 0)
		{
			int polygonVertexIndex = 0;
			for (int polygon = 0; polygon < polygonIndex; ++polygon)
			{
				polygonVertexIndex += mesh->GetPolygonSize(polygon);
			}
			polygonVertexIndex += positionInPolygon;
			directIndex = polygonVertexIndex;
		}

		if (referenceMode == FbxGeometryElement::eIndexToDirect)
		{
			directIndex = uvElement->GetIndexArray().GetAt(directIndex);
		}
	}
	else
	{
		outUv[0] = 0.0f;
		outUv[1] = 0.0f;
		return false;
	}

	const FbxVector2 uv = uvElement->GetDirectArray().GetAt(directIndex);
	outUv[0] = static_cast<float>(uv[0]);
	outUv[1] = static_cast<float>(uv[1]);
	return true;
}

[[nodiscard]] uint32_t ResolvePolygonMaterialSlot(
	FbxMesh* mesh,
	FbxNode* node,
	int polygonIndex,
	MaterialSlotRegistry& materialRegistry,
	const std::filesystem::path& fbxDirectory,
	const std::filesystem::path& fbxStem,
	std::vector<ModelMaterialSlotData>& materialSlots)
{
	const FbxGeometryElementMaterial* materialElement = mesh->GetElementMaterial(0);
	int nodeMaterialIndex = 0;
	if (materialElement != nullptr)
	{
		switch (materialElement->GetMappingMode())
		{
		case FbxGeometryElement::eByPolygon:
			nodeMaterialIndex = materialElement->GetIndexArray().GetAt(polygonIndex);
			break;
		case FbxGeometryElement::eAllSame:
			nodeMaterialIndex = materialElement->GetIndexArray().GetAt(0);
			break;
		default:
			break;
		}
	}

	FbxSurfaceMaterial* material = nullptr;
	if (nodeMaterialIndex >= 0 && nodeMaterialIndex < node->GetMaterialCount())
	{
		material = node->GetMaterial(nodeMaterialIndex);
	}

	return materialRegistry.GetOrAdd(material, fbxDirectory, fbxStem, materialSlots);
}

[[nodiscard]] Result<ModelMeshData> ExtractMeshFromNode(
	FbxNode* node,
	const std::filesystem::path& fbxDirectory,
	const std::filesystem::path& fbxStem,
	MaterialSlotRegistry& materialRegistry,
	std::vector<ModelMaterialSlotData>& materialSlots)
{
	FbxMesh* mesh = node->GetMesh();
	if (mesh == nullptr)
	{
		return FailRuntime<ModelMeshData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"FBX node does not contain a mesh");
	}

	ModelMeshData meshData{};
	meshData.name = node->GetName();
	ResetBounds(meshData.bounds);

	const int polygonCount = mesh->GetPolygonCount();
	if (polygonCount <= 0)
	{
		return FailRuntime<ModelMeshData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"FBX mesh contains no polygons: " + meshData.name);
	}

	meshData.vertices.reserve(static_cast<std::size_t>(polygonCount) * 3U);
	meshData.indices.reserve(static_cast<std::size_t>(polygonCount) * 3U);

	int32_t activeSubMeshIndex = -1;
	uint32_t activeMaterialSlot = 0;

	for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
	{
		const int polygonSize = mesh->GetPolygonSize(polygonIndex);
		if (polygonSize != 3)
		{
			return FailRuntime<ModelMeshData>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"FBX mesh polygon is not triangulated: " + meshData.name);
		}

		const uint32_t materialSlot = ResolvePolygonMaterialSlot(
			mesh,
			node,
			polygonIndex,
			materialRegistry,
			fbxDirectory,
			fbxStem,
			materialSlots);

		if (activeSubMeshIndex < 0 || activeMaterialSlot != materialSlot)
		{
			ModelSubMeshData subMesh{};
			subMesh.indexStart = static_cast<uint32_t>(meshData.indices.size());
			subMesh.indexCount = 0;
			subMesh.materialSlot = materialSlot;
			meshData.submeshes.push_back(subMesh);
			activeSubMeshIndex = static_cast<int32_t>(meshData.submeshes.size()) - 1;
			activeMaterialSlot = materialSlot;
		}

		for (int cornerIndex = 0; cornerIndex < polygonSize; ++cornerIndex)
		{
			const int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, cornerIndex);
			if (controlPointIndex < 0)
			{
				return FailRuntime<ModelMeshData>(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"FBX mesh has invalid control point index: " + meshData.name);
			}

			const FbxVector4 position = mesh->GetControlPointAt(controlPointIndex);

			BasicVertex vertex{};
			vertex.position[0] = static_cast<float>(position[0]);
			vertex.position[1] = static_cast<float>(position[1]);
			vertex.position[2] = static_cast<float>(position[2]);
			vertex.position[3] = 1.0f;

			if (!ExtractPolygonVertexNormal(mesh, polygonIndex, cornerIndex, vertex.normal))
			{
				vertex.normal[0] = 0.0f;
				vertex.normal[1] = 1.0f;
				vertex.normal[2] = 0.0f;
			}

			(void)ExtractPolygonVertexUV(mesh, polygonIndex, cornerIndex, vertex.uv);

			ExpandBounds(meshData.bounds, vertex.position);

			const uint32_t vertexIndex = static_cast<uint32_t>(meshData.vertices.size());
			meshData.vertices.push_back(vertex);
			meshData.indices.push_back(vertexIndex);
			meshData.submeshes[static_cast<std::size_t>(activeSubMeshIndex)].indexCount += 1;
		}
	}

	if (meshData.vertices.empty() || meshData.indices.empty() || meshData.submeshes.empty())
	{
		return FailRuntime<ModelMeshData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"FBX mesh extraction produced empty geometry: " + meshData.name);
	}

	if (!BoundsAreValid(meshData.bounds))
	{
		return FailRuntime<ModelMeshData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"FBX mesh bounds are invalid: " + meshData.name);
	}

	return MakeOk(std::move(meshData));
}

void TraverseNodeHierarchy(
	FbxNode* fbxNode,
	int32_t parentIndex,
	ModelAssetData& assetData,
	MaterialSlotRegistry& materialRegistry,
	const std::filesystem::path& fbxDirectory,
	const std::filesystem::path& fbxStem)
{
	ModelNodeData nodeData{};
	nodeData.name = fbxNode->GetName();
	nodeData.parentIndex = parentIndex;
	CopyMatrixFromFbx(nodeData.localTransform, fbxNode->EvaluateLocalTransform());

	const uint32_t nodeIndex = static_cast<uint32_t>(assetData.nodes.size());
	assetData.nodes.push_back(std::move(nodeData));

	if (parentIndex >= 0)
	{
		assetData.nodes[static_cast<std::size_t>(parentIndex)].childNodeIndices.push_back(nodeIndex);
	}

	if (fbxNode->GetMesh() != nullptr)
	{
		auto meshResult = ExtractMeshFromNode(
			fbxNode,
			fbxDirectory,
			fbxStem,
			materialRegistry,
			assetData.materialSlots);
		if (!meshResult)
		{
			LOG_ERROR(LogCategory::Asset, meshResult.error.message);
		}
		else
		{
			assetData.nodes[nodeIndex].meshIndex =
				static_cast<int32_t>(assetData.meshes.size());
			assetData.meshes.push_back(std::move(meshResult.value));
		}
	}

	for (int childIndex = 0; childIndex < fbxNode->GetChildCount(); ++childIndex)
	{
		TraverseNodeHierarchy(
			fbxNode->GetChild(childIndex),
			static_cast<int32_t>(nodeIndex),
			assetData,
			materialRegistry,
			fbxDirectory,
			fbxStem);
	}
}

[[nodiscard]] Result<void> PrepareSceneGeometry(
	FbxScene* scene,
	FbxManager* manager,
	const FbxModelLoadOptions& options)
{
	if (options.triangulate)
	{
		FbxGeometryConverter geometryConverter(manager);
		if (!geometryConverter.Triangulate(scene, true))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::RuntimeError,
				"Failed to triangulate FBX scene");
		}
	}

	if (options.convertAxisToDirectX)
	{
		FbxRootNodeUtility::RemoveAllFbxRoots(scene);
		const FbxAxisSystem targetAxis = FbxAxisSystem::DirectX;
		const FbxAxisSystem sourceAxis = scene->GetGlobalSettings().GetAxisSystem();
		if (sourceAxis != targetAxis)
		{
			targetAxis.ConvertScene(scene);
		}
	}

	if (options.generateNormalsIfMissing)
	{
		const int nodeCount = scene->GetNodeCount();
		for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			FbxNode* node = scene->GetNode(nodeIndex);
			if (node == nullptr)
			{
				continue;
			}

			FbxMesh* mesh = node->GetMesh();
			if (mesh == nullptr)
			{
				continue;
			}

			(void)mesh->GenerateNormals(false);
		}
	}

	return MakeOk();
}
} // namespace

Result<ModelAssetData> LoadModelAssetDataFromFile(
	const std::filesystem::path& absolutePath,
	FbxSdkContext& sdkContext,
	const FbxModelLoadOptions& options)
{
	if (absolutePath.empty())
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model file path is empty");
	}

	std::error_code errorCode{};
	if (!std::filesystem::exists(absolutePath, errorCode) || errorCode)
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Model file not found: " + absolutePath.string());
	}

	FbxManager* manager = sdkContext.GetManager();
	if (manager == nullptr)
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"FBX SDK context is not initialized");
	}

	FbxImporter* importer = FbxImporter::Create(manager, "");
	if (importer == nullptr)
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::OutOfMemory,
			"Failed to create FBX importer");
	}

	const std::string importPath = absolutePath.string();
	if (!importer->Initialize(importPath.c_str(), -1, sdkContext.GetIOSettings()))
	{
		const std::string errorMessage =
			"Failed to initialize FBX importer for " + absolutePath.string() + ": " +
			importer->GetStatus().GetErrorString();
		importer->Destroy();
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::RuntimeError,
			errorMessage);
	}

	FbxScene* scene = FbxScene::Create(manager, "ImportScene");
	if (scene == nullptr)
	{
		importer->Destroy();
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::OutOfMemory,
			"Failed to create FBX scene");
	}

	if (!importer->Import(scene))
	{
		const std::string errorMessage =
			"Failed to import FBX scene " + absolutePath.string() + ": " +
			importer->GetStatus().GetErrorString();
		scene->Destroy();
		importer->Destroy();
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::RuntimeError,
			errorMessage);
	}

	importer->Destroy();

	if (const Result<void> prepareResult = PrepareSceneGeometry(scene, manager, options); !prepareResult)
	{
		scene->Destroy();
		return MakeFail<ModelAssetData>(prepareResult.error.code, prepareResult.error.message);
	}

	ModelAssetData assetData{};
	assetData.sourcePath = absolutePath;

	const std::filesystem::path fbxDirectory = absolutePath.parent_path();
	const std::filesystem::path fbxStem = absolutePath.stem();

	ModelNodeData rootNode{};
	rootNode.name = "Root";
	MakeIdentityMatrix(rootNode.localTransform);
	assetData.rootNodeIndex = 0;
	assetData.nodes.push_back(std::move(rootNode));

	MaterialSlotRegistry materialRegistry;
	FbxNode* rootFbxNode = scene->GetRootNode();
	if (rootFbxNode != nullptr)
	{
		for (int childIndex = 0; childIndex < rootFbxNode->GetChildCount(); ++childIndex)
		{
			TraverseNodeHierarchy(
				rootFbxNode->GetChild(childIndex),
				static_cast<int32_t>(assetData.rootNodeIndex),
				assetData,
				materialRegistry,
				fbxDirectory,
				fbxStem);
		}
	}

	scene->Destroy();

	if (assetData.meshes.empty())
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"FBX file contains no meshes: " + absolutePath.string());
	}

	if (assetData.materialSlots.empty())
	{
		(void)materialRegistry.GetOrAdd(nullptr, fbxDirectory, fbxStem, assetData.materialSlots);
	}

	return MakeOk(std::move(assetData));
}
