#include "Engine/Renderer/Test/FbxModelFileLoaderTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Model/Loader/FbxModelFileLoader.h"
#include "Engine/Renderer/Model/Loader/FbxSdkContext.h"
#include "Engine/Renderer/Model/Loader/ModelAssetPath.h"

#include <cmath>
#include <limits>

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
			!std::isfinite(vertex.normal[2]))
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
			"ResolveModelAssetPath should reject non-fbx extensions");
	}

	auto sdkResult = FbxSdkContext::Create();
	if (!sdkResult)
	{
		return MakeFail(sdkResult.error.code, sdkResult.error.message);
	}

	if (auto triangleResult = RunLoaderTestCase(
			assetsRoot,
			"Models/Test/Triangle.fbx",
			sdkResult.value);
		!triangleResult)
	{
		return triangleResult;
	}

	LOG_INFO(LogCategory::Renderer, "FbxModelFileLoaderTests passed");
	return MakeOk();
}
