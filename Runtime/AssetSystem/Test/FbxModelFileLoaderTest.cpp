#include "Runtime/AssetSystem/Test/FbxModelFileLoaderTest.h"

#include "Runtime/Core/Hash/Fnv.h"
#include "Runtime/Core/IO/BinaryFile.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/AssetSystem/Model/Loader/CookedModel.h"
#include "Runtime/AssetSystem/Model/Loader/FbxModelFileLoader.h"
#include "Runtime/AssetSystem/Model/Loader/FbxSdkContext.h"
#include "Runtime/AssetSystem/Model/Loader/ModelAssetPath.h"

#include <cctype>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
[[nodiscard]] bool BoundsAreValid(const MeshBounds& bounds)
{
	return bounds.minX <= bounds.maxX &&
		bounds.minY <= bounds.maxY &&
		bounds.minZ <= bounds.maxZ;
}

[[nodiscard]] Result<void> ValidateNodeHierarchy(const ModelAssetData& assetData)
{
	if (assetData.nodes.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model asset must contain at least one node");
	}

	if (assetData.rootNodeIndex >= assetData.nodes.size())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model asset root node index is out of range");
	}

	for (std::size_t nodeIndex = 0; nodeIndex < assetData.nodes.size(); ++nodeIndex)
	{
		const ModelNodeData& node = assetData.nodes[nodeIndex];
		if (node.parentIndex >= static_cast<int32_t>(assetData.nodes.size()))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Model node parent index is out of range");
		}

		if (node.meshIndex >= static_cast<int32_t>(assetData.meshes.size()))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Model node mesh index is out of range");
		}

		for (const uint32_t childIndex : node.childNodeIndices)
		{
			if (childIndex >= assetData.nodes.size())
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"Model node child index is out of range");
			}

			if (assetData.nodes[childIndex].parentIndex != static_cast<int32_t>(nodeIndex))
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"Model node parent/child relationship is inconsistent");
			}
		}
	}

	return MakeOk();
}

[[nodiscard]] Result<void> ValidateMeshData(
	const ModelMeshData& meshData,
	const std::vector<ModelMaterialSlotData>& materialSlots)
{
	if (meshData.vertices.empty() || meshData.indices.empty() || meshData.submeshes.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model mesh data is empty: " + meshData.name);
	}

	if (meshData.indices.size() % 3 != 0)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model mesh indices are not triangulated: " + meshData.name);
	}

	if (!BoundsAreValid(meshData.bounds))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model mesh bounds are invalid: " + meshData.name);
	}

	for (const BasicVertex& vertex : meshData.vertices)
	{
		if (!std::isfinite(vertex.position[0]) ||
			!std::isfinite(vertex.position[1]) ||
			!std::isfinite(vertex.position[2]) ||
			!std::isfinite(vertex.normal[0]) ||
			!std::isfinite(vertex.normal[1]) ||
			!std::isfinite(vertex.normal[2]) ||
			!std::isfinite(vertex.tangent[0]) ||
			!std::isfinite(vertex.tangent[1]) ||
			!std::isfinite(vertex.tangent[2]) ||
			!std::isfinite(vertex.tangent[3]))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Model mesh contains non-finite vertex data: " + meshData.name);
		}

		if (std::abs(vertex.position[3] - 1.0f) > 0.0001f)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Model mesh vertex position w must be 1.0: " + meshData.name);
		}
	}

	if (meshData.vertices.size() > meshData.indices.size())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model mesh has more vertices than corners: " + meshData.name);
	}

	for (const uint32_t index : meshData.indices)
	{
		if (index >= meshData.vertices.size())
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Model mesh index is out of range: " + meshData.name);
		}
	}

	for (const ModelSubMeshData& subMesh : meshData.submeshes)
	{
		if (subMesh.indexCount == 0 ||
			subMesh.indexStart + subMesh.indexCount > meshData.indices.size())
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Model submesh index range is invalid: " + meshData.name);
		}

		if (subMesh.materialSlot >= materialSlots.size())
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Model submesh material slot is out of range: " + meshData.name);
		}
	}

	return MakeOk();
}

[[nodiscard]] Result<void> ValidateModelAssetData(const ModelAssetData& assetData)
{
	if (assetData.meshes.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model asset must contain at least one mesh");
	}

	if (assetData.materialSlots.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model asset must contain at least one material slot");
	}

	if (auto nodeResult = ValidateNodeHierarchy(assetData); !nodeResult)
	{
		return nodeResult;
	}

	for (const ModelMeshData& meshData : assetData.meshes)
	{
		if (auto meshResult = ValidateMeshData(meshData, assetData.materialSlots); !meshResult)
		{
			return meshResult;
		}
	}

	return MakeOk();
}

[[nodiscard]] Result<void> RunLoaderTestCase(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	FbxSdkContext& sdkContext)
{
	auto resolveResult = ResolveModelAssetPath(assetsRoot, relativePath);
	if (!resolveResult)
	{
		if (resolveResult.error.code == ErrorCode::FileNotFound)
		{
			return MakeOk();
		}

		return MakeFail(resolveResult.error.code, resolveResult.error.message);
	}

	const std::filesystem::path& absolutePath = resolveResult.value;
	auto loadResult = LoadModelAssetDataFromFile(absolutePath, sdkContext);
	if (!loadResult)
	{
		return MakeFail(loadResult.error.code, loadResult.error.message);
	}

	return ValidateModelAssetData(loadResult.value);
}

[[nodiscard]] std::string LowercaseExtension(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	for (char& character : extension)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension;
}

[[nodiscard]] bool IsPaladinFbmPng(const std::filesystem::path& path)
{
	if (path.empty() || LowercaseExtension(path) != ".png")
	{
		return false;
	}

	const std::filesystem::path parentName = path.parent_path().filename();
	return parentName == "Paladin J Nordstrom.fbm";
}

[[nodiscard]] Result<void> ValidatePaladinMaterials(const ModelAssetData& assetData)
{
	bool hasDiffuse = false;
	bool hasNormal = false;
	for (const ModelMaterialSlotData& slot : assetData.materialSlots)
	{
		if (IsPaladinFbmPng(slot.diffuseTexturePath))
		{
			hasDiffuse = true;
		}

		if (IsPaladinFbmPng(slot.normalTexturePath))
		{
			hasNormal = true;
		}
	}

	if (!hasDiffuse || !hasNormal)
	{
		std::string detail = "Paladin material textures must resolve inside Paladin J Nordstrom.fbm";
		for (const ModelMaterialSlotData& slot : assetData.materialSlots)
		{
			detail += " | slot " + slot.name;
			detail += " diffuse=" + slot.diffuseTexturePath.string();
			detail += " normal=" + slot.normalTexturePath.string();
		}

		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			detail);
	}

	return MakeOk();
}

[[nodiscard]] Result<void> RunPaladinLoaderTest(
	const std::filesystem::path& assetsRoot,
	FbxSdkContext& sdkContext)
{
	const std::filesystem::path relativePath = "Models/Paladin J Nordstrom.fbx";
	auto resolveResult = ResolveModelAssetPath(assetsRoot, relativePath);
	if (!resolveResult)
	{
		return MakeFail(resolveResult.error.code, resolveResult.error.message);
	}

	auto loadResult = LoadModelAssetDataFromFile(resolveResult.value, sdkContext);
	if (!loadResult)
	{
		return MakeFail(loadResult.error.code, loadResult.error.message);
	}

	const ModelAssetData& assetData = loadResult.value;
	if (auto modelResult = ValidateModelAssetData(assetData); !modelResult)
	{
		return modelResult;
	}

	if (auto materialResult = ValidatePaladinMaterials(assetData); !materialResult)
	{
		return materialResult;
	}

	LOG_INFO_F(
		LogCategory::Asset,
		"Paladin FBX loaded: meshes={} materials={} nodes={}",
		assetData.meshes.size(),
		assetData.materialSlots.size(),
		assetData.nodes.size());
	for (const ModelMaterialSlotData& slot : assetData.materialSlots)
	{
		LOG_INFO_F(
			LogCategory::Asset,
			"Paladin material '{}': diffuse='{}' normal='{}'",
			slot.name,
			slot.diffuseTexturePath.string(),
			slot.normalTexturePath.string());
	}

	return MakeOk();
}

[[nodiscard]] bool SameFloats(const float* left, const float* right, size_t count)
{
	for (size_t index = 0; index < count; ++index)
	{
		if (left[index] != right[index])
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool SameModel(const ModelAssetData& left, const ModelAssetData& right)
{
	if (left.sourcePath != right.sourcePath || left.rootNodeIndex != right.rootNodeIndex
		|| left.meshes.size() != right.meshes.size() || left.materialSlots.size() != right.materialSlots.size()
		|| left.nodes.size() != right.nodes.size())
	{
		return false;
	}

	for (size_t meshIndex = 0; meshIndex < left.meshes.size(); ++meshIndex)
	{
		const ModelMeshData& leftMesh = left.meshes[meshIndex];
		const ModelMeshData& rightMesh = right.meshes[meshIndex];
		if (leftMesh.name != rightMesh.name || leftMesh.indices != rightMesh.indices
			|| leftMesh.vertices.size() != rightMesh.vertices.size()
			|| leftMesh.submeshes.size() != rightMesh.submeshes.size()
			|| leftMesh.bounds.minX != rightMesh.bounds.minX || leftMesh.bounds.minY != rightMesh.bounds.minY
			|| leftMesh.bounds.minZ != rightMesh.bounds.minZ || leftMesh.bounds.maxX != rightMesh.bounds.maxX
			|| leftMesh.bounds.maxY != rightMesh.bounds.maxY || leftMesh.bounds.maxZ != rightMesh.bounds.maxZ)
		{
			return false;
		}

		for (size_t vertexIndex = 0; vertexIndex < leftMesh.vertices.size(); ++vertexIndex)
		{
			const BasicVertex& leftVertex = leftMesh.vertices[vertexIndex];
			const BasicVertex& rightVertex = rightMesh.vertices[vertexIndex];
			if (!SameFloats(leftVertex.position, rightVertex.position, 4)
				|| !SameFloats(leftVertex.uv, rightVertex.uv, 2)
				|| !SameFloats(leftVertex.normal, rightVertex.normal, 3)
				|| !SameFloats(leftVertex.tangent, rightVertex.tangent, 4))
			{
				return false;
			}
		}

		for (size_t submeshIndex = 0; submeshIndex < leftMesh.submeshes.size(); ++submeshIndex)
		{
			const ModelSubMeshData& leftSubmesh = leftMesh.submeshes[submeshIndex];
			const ModelSubMeshData& rightSubmesh = rightMesh.submeshes[submeshIndex];
			if (leftSubmesh.indexStart != rightSubmesh.indexStart || leftSubmesh.indexCount != rightSubmesh.indexCount
				|| leftSubmesh.materialSlot != rightSubmesh.materialSlot)
			{
				return false;
			}
		}
	}

	for (size_t slotIndex = 0; slotIndex < left.materialSlots.size(); ++slotIndex)
	{
		const ModelMaterialSlotData& leftSlot = left.materialSlots[slotIndex];
		const ModelMaterialSlotData& rightSlot = right.materialSlots[slotIndex];
		if (leftSlot.name != rightSlot.name || leftSlot.diffuseTexturePath != rightSlot.diffuseTexturePath
			|| leftSlot.normalTexturePath != rightSlot.normalTexturePath)
		{
			return false;
		}
	}

	for (size_t nodeIndex = 0; nodeIndex < left.nodes.size(); ++nodeIndex)
	{
		const ModelNodeData& leftNode = left.nodes[nodeIndex];
		const ModelNodeData& rightNode = right.nodes[nodeIndex];
		if (leftNode.name != rightNode.name || leftNode.parentIndex != rightNode.parentIndex
			|| leftNode.meshIndex != rightNode.meshIndex || leftNode.childNodeIndices != rightNode.childNodeIndices
			|| !SameFloats(leftNode.localTransform, rightNode.localTransform, 16))
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool CookedFileStartsWithMagic(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	char magic[4] = {};
	file.read(magic, 4);
	return file.gcount() == 4 && magic[0] == 'A' && magic[1] == 'E' && magic[2] == 'M' && magic[3] == 'O';
}

[[nodiscard]] Result<void> TestCookedModelCache(
	const std::filesystem::path& assetsRoot,
	FbxSdkContext& sdkContext)
{
	const std::filesystem::path tempRoot = std::filesystem::temp_directory_path() / "AetherModelCook" / "Assets";
	std::error_code errorCode{};
	std::filesystem::remove_all(tempRoot.parent_path(), errorCode);
	std::filesystem::create_directories(tempRoot / "Models", errorCode);
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked model test directory could not be created");
	}

	{
		std::ofstream badFile(tempRoot / "Models" / "bad.fbx", std::ios::binary | std::ios::trunc);
		if (!badFile)
		{
			std::filesystem::remove_all(tempRoot.parent_path(), errorCode);
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::FileNotFound,
				"Invalid FBX test file could not be written");
		}
		badFile << "not an fbx";
	}
	const Result<ModelAssetData> badLoad = LoadModelAsset(tempRoot, "Models/bad.fbx");
	if (badLoad || std::filesystem::exists(CookedModelPath(tempRoot, "Models/bad.fbx"), errorCode))
	{
		std::filesystem::remove_all(tempRoot.parent_path(), errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"An invalid FBX should fail before a cooked model is written");
	}
	std::filesystem::remove_all(tempRoot.parent_path(), errorCode);

	const std::filesystem::path candidates[] = {
		"Models/trangle.fbx",
		"Models/Paladin J Nordstrom.fbx",
	};
	std::filesystem::path relativePath{};
	Result<std::filesystem::path> resolved = FailRuntime<std::filesystem::path>(
		LogCategory::Asset,
		ErrorCode::FileNotFound,
		"No FBX model is available for the cook test");
	for (const std::filesystem::path& candidate : candidates)
	{
		resolved = ResolveModelAssetPath(assetsRoot, candidate);
		if (resolved)
		{
			relativePath = candidate;
			break;
		}
	}
	if (!resolved)
	{
		return MakeOk();
	}

	const Result<std::string> sourceBytes = ReadBinaryFile(resolved.value);
	if (!sourceBytes)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Model file could not be opened");
	}
	const Result<ModelAssetData> imported = sourceBytes
		? LoadModelAssetDataFromFile(resolved.value, sdkContext)
		: MakeFail<ModelAssetData>(sourceBytes.error.code, sourceBytes.error.message);
	const Result<ModelAssetData> cooked = imported ? LoadModelAsset(assetsRoot, relativePath) : imported;
	const Result<ModelAssetData> cookedAgain = cooked ? LoadModelAsset(assetsRoot, relativePath) : cooked;
	const std::filesystem::path cookedPath = CookedModelPath(assetsRoot, relativePath);
	if (!sourceBytes || !imported || !cooked || !cookedAgain || !std::filesystem::exists(cookedPath, errorCode)
		|| !SameModel(imported.value, cooked.value) || !SameModel(cooked.value, cookedAgain.value))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			sourceBytes && imported && cooked && cookedAgain
				? "Cooked model should match the FBX import and load again from disk"
				: (sourceBytes
					? (imported ? (cooked ? cookedAgain.error.message : cooked.error.message) : imported.error.message)
					: sourceBytes.error.message));
	}

	const FbxModelLoadOptions options{};
	ModelAssetData patched{};
	const bool readPatched = TryReadCookedModel(
		cookedPath,
		Fnv1a64(sourceBytes.value),
		options,
		assetsRoot,
		relativePath,
		patched) && !patched.meshes.empty() && !patched.meshes.front().vertices.empty();
	if (readPatched)
	{
		patched.meshes.front().vertices.front().position[0] += 3.0f;
	}
	const float patchedPosition = readPatched ? patched.meshes.front().vertices.front().position[0] : 0.0f;
	const Result<void> writtenPatch = readPatched
		? WriteCookedModel(cookedPath, Fnv1a64(sourceBytes.value), options, assetsRoot, relativePath, patched)
		: FailRuntime(LogCategory::Asset, ErrorCode::InvalidArgument, "Cooked model could not be read back");
	const Result<ModelAssetData> patchedLoad = writtenPatch
		? LoadModelAsset(assetsRoot, relativePath)
		: MakeFail<ModelAssetData>(writtenPatch.error.code, writtenPatch.error.message);
	if (!writtenPatch || !patchedLoad || patchedLoad.value.meshes.empty()
		|| patchedLoad.value.meshes.front().vertices.empty()
		|| patchedLoad.value.meshes.front().vertices.front().position[0] != patchedPosition)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			writtenPatch && patchedLoad
				? "A matching cooked model should be reused"
				: (writtenPatch ? patchedLoad.error.message : writtenPatch.error.message));
	}

	{
		std::ofstream corrupt(cookedPath, std::ios::binary | std::ios::trunc);
		corrupt << "XXXX";
	}
	const Result<ModelAssetData> recovered = LoadModelAsset(assetsRoot, relativePath);
	if (!recovered || !SameModel(imported.value, recovered.value) || !CookedFileStartsWithMagic(cookedPath))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			recovered
				? "A corrupt cooked model should be rebuilt from the FBX"
				: recovered.error.message);
	}

	return MakeOk();
}
} // namespace

Result<void> RunFbxModelFileLoaderTests(const std::filesystem::path& assetsRoot)
{
	if (assetsRoot.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"FBX model file loader tests require a valid assets root");
	}

	if (auto traversalResult = ResolveModelAssetPath(assetsRoot, "../CMakeLists.txt"); traversalResult)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"ResolveModelAssetPath should reject parent traversal");
	}

	if (auto extensionResult = ResolveModelAssetPath(assetsRoot, "Models/Test/NotAModel.obj"); extensionResult)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"ResolveModelAssetPath should reject unsupported model extensions");
	}

	auto sdkResult = FbxSdkContext::Create();
	if (!sdkResult)
	{
		return MakeFail(sdkResult.error.code, sdkResult.error.message);
	}

	if (auto triangleResult = RunLoaderTestCase(
			assetsRoot,
			"Models/trangle.fbx",
			sdkResult.value);
		!triangleResult)
	{
		return triangleResult;
	}

	if (auto paladinResult = RunPaladinLoaderTest(assetsRoot, sdkResult.value); !paladinResult)
	{
		return paladinResult;
	}

	if (auto cookedResult = TestCookedModelCache(assetsRoot, sdkResult.value); !cookedResult)
	{
		return cookedResult;
	}

	LOG_INFO(LogCategory::Asset, "FbxModelFileLoaderTests passed");
	return MakeOk();
}
