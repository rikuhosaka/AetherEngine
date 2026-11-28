#include "Runtime/AssetSystem/Test/GltfModelFileLoaderTest.h"

#include "Runtime/AssetSystem/Model/Loader/CookedModel.h"
#include "Runtime/AssetSystem/Model/Loader/FbxModelFileLoader.h"
#include "Runtime/AssetSystem/Model/Loader/FbxSdkContext.h"
#include "Runtime/AssetSystem/Model/Loader/GltfModelFileLoader.h"
#include "Runtime/Core/IO/Path.h"
#include "Runtime/Core/Log/LogMacros.h"

#include <fbxsdk.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace
{
void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
	{
		out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
	}
}

void WriteFloat(std::vector<std::uint8_t>& out, float value)
{
	static_assert(sizeof(float) == 4);
	std::uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	WriteU32(out, bits);
}

[[nodiscard]] std::vector<std::uint8_t> TriangleBin()
{
	std::vector<std::uint8_t> bin;
	const float positions[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const float normals[9] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	const float tangents[12] = {
		1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
	};
	for (float value : positions)
	{
		WriteFloat(bin, value);
	}
	for (float value : normals)
	{
		WriteFloat(bin, value);
	}
	for (float value : tangents)
	{
		WriteFloat(bin, value);
	}
	bin.push_back(0);
	bin.push_back(0);
	bin.push_back(1);
	bin.push_back(0);
	bin.push_back(2);
	bin.push_back(0);
	return bin;
}

[[nodiscard]] std::string TriangleJson(bool externalBuffer)
{
	const char* buffer = externalBuffer
		? R"("buffers":[{"uri":"triangle.bin","byteLength":126}])"
		: R"("buffers":[{"byteLength":126}])";
	return std::string(R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"name":"Triangle","mesh":0,"translation":[1,2,3],"scale":[2,2,2]}],
		"meshes":[{"name":"TriMesh","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TANGENT":2},"indices":3}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},
			{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":36},
			{"buffer":0,"byteOffset":72,"byteLength":48},
			{"buffer":0,"byteOffset":120,"byteLength":6}
		],)") + buffer + "}";
}

[[nodiscard]] bool WriteBytes(const std::filesystem::path& path, const std::uint8_t* data, size_t size)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return false;
	}
	file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
	return static_cast<bool>(file);
}

[[nodiscard]] bool WriteText(const std::filesystem::path& path, const std::string& text)
{
	return WriteBytes(path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

[[nodiscard]] std::vector<std::uint8_t> MakeGlb(const std::string& json, const std::vector<std::uint8_t>& bin)
{
	std::vector<std::uint8_t> jsonChunk(json.begin(), json.end());
	while (jsonChunk.size() % 4 != 0)
	{
		jsonChunk.push_back(static_cast<std::uint8_t>(' '));
	}
	std::vector<std::uint8_t> binChunk = bin;
	while (binChunk.size() % 4 != 0)
	{
		binChunk.push_back(0);
	}

	std::vector<std::uint8_t> glb;
	WriteU32(glb, 0x46546C67u);
	WriteU32(glb, 2u);
	WriteU32(glb, static_cast<std::uint32_t>(12 + 8 + jsonChunk.size() + 8 + binChunk.size()));
	WriteU32(glb, static_cast<std::uint32_t>(jsonChunk.size()));
	WriteU32(glb, 0x4E4F534Au);
	glb.insert(glb.end(), jsonChunk.begin(), jsonChunk.end());
	WriteU32(glb, static_cast<std::uint32_t>(binChunk.size()));
	WriteU32(glb, 0x004E4942u);
	glb.insert(glb.end(), binChunk.begin(), binChunk.end());
	return glb;
}

[[nodiscard]] bool NearlyEqual(float left, float right)
{
	return std::fabs(left - right) <= 0.0001f;
}

[[nodiscard]] Result<void> ExpectTriangle(const ModelAssetData& asset, bool authoredTangents)
{
	if (asset.nodes.size() != 2 || asset.meshes.size() != 1 || asset.materialSlots.size() != 1
		|| asset.materialSlots[0].name != "Default" || asset.meshes[0].name != "TriMesh"
		|| asset.meshes[0].vertices.size() != 3 || asset.meshes[0].indices.size() != 3
		|| asset.meshes[0].submeshes.size() != 1 || asset.nodes[0].name != "Root"
		|| asset.nodes[1].name != "Triangle" || asset.nodes[1].parentIndex != 0
		|| asset.nodes[1].meshIndex != 0 || asset.nodes[0].childNodeIndices.size() != 1
		|| asset.nodes[0].childNodeIndices[0] != 1)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF triangle asset hierarchy is wrong");
	}

	const uint32_t expectedIndices[3] = { 0, 2, 1 };
	if (asset.meshes[0].indices[0] != expectedIndices[0] || asset.meshes[0].indices[1] != expectedIndices[1]
		|| asset.meshes[0].indices[2] != expectedIndices[2])
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF triangle winding was not converted");
	}

	const BasicVertex& vertex = asset.meshes[0].vertices[1];
	if (!NearlyEqual(vertex.position[0], 1.0f) || !NearlyEqual(vertex.position[1], 0.0f)
		|| !NearlyEqual(vertex.position[2], 0.0f) || !NearlyEqual(asset.meshes[0].vertices[2].position[1], 1.0f))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF triangle positions were not preserved");
	}

	for (const BasicVertex& current : asset.meshes[0].vertices)
	{
		if (!NearlyEqual(current.normal[2], -1.0f))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF normals were not converted to DirectX space");
		}
	}

	if (authoredTangents)
	{
		if (!NearlyEqual(vertex.tangent[0], 1.0f) || !NearlyEqual(vertex.tangent[1], 0.0f)
			|| !NearlyEqual(vertex.tangent[2], 0.0f) || !NearlyEqual(vertex.tangent[3], -1.0f))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF tangents were not converted to DirectX space");
		}
	}

	const float* matrix = asset.nodes[1].localTransform;
	const bool matrixMatches = NearlyEqual(matrix[0], 2.0f) && NearlyEqual(matrix[5], 2.0f)
		&& NearlyEqual(matrix[10], 2.0f) && NearlyEqual(matrix[12], 1.0f) && NearlyEqual(matrix[13], 2.0f)
		&& NearlyEqual(matrix[14], -3.0f) && NearlyEqual(matrix[15], 1.0f);
	if (!matrixMatches)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF node transform was not converted to a row-major DirectX matrix");
	}

	return MakeOk();
}

[[nodiscard]] bool MessageContains(const Result<ModelAssetData>& result, const char* text)
{
	return !result && result.error.message.find(text) != std::string::npos;
}

[[nodiscard]] Result<std::vector<std::uint8_t>> ReadCookedBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return FailRuntime<std::vector<std::uint8_t>>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked glTF file could not be read");
	}

	return MakeOk(std::vector<std::uint8_t>{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>() });
}

[[nodiscard]] Result<void> TestCookedGltfAsset()
{
	const std::filesystem::path assetsRoot = std::filesystem::temp_directory_path() / "AetherGltfCook" / "Assets";
	std::error_code errorCode{};
	std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
	std::filesystem::create_directories(assetsRoot / "Models", errorCode);
	if (errorCode)
	{
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF cook test directory could not be created");
	}

	const std::vector<std::uint8_t> bin = TriangleBin();
	const std::string json = R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"images":[{"uri":"albedo.png"}],
		"textures":[{"source":0}],
		"materials":[{"name":"Stone","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},
			{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":24},
			{"buffer":0,"byteOffset":60,"byteLength":6}
		],
		"buffers":[{"uri":"triangle.bin","byteLength":66}]
	})";
	std::vector<std::uint8_t> texturedBin(bin.begin(), bin.begin() + 36);
	const float uvs[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	for (float value : uvs)
	{
		WriteFloat(texturedBin, value);
	}
	texturedBin.insert(texturedBin.end(), bin.end() - 6, bin.end());
	const std::filesystem::path relativePath = "Models/textured.gltf";
	const bool wrote = WriteBytes(assetsRoot / "Models" / "triangle.bin", texturedBin.data(), texturedBin.size())
		&& WriteBytes(assetsRoot / "Models" / "albedo.png", reinterpret_cast<const std::uint8_t*>("png"), 3)
		&& WriteText(assetsRoot / relativePath, json);
	if (!wrote)
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF cook test files could not be written");
	}

	const Result<ModelAssetData> first = LoadModelAsset(assetsRoot, relativePath);
	const Result<ModelAssetData> second = LoadModelAsset(assetsRoot, relativePath);
	const std::filesystem::path cookedPath = CookedModelPath(assetsRoot, relativePath);
	auto cookedBytes = first ? ReadCookedBytes(cookedPath) : FailRuntime<std::vector<std::uint8_t>>(
		LogCategory::Asset, ErrorCode::InvalidArgument, first.error.message);
	if (!first || !second || !cookedBytes || first.value.meshes.size() != 1
		|| !NearlyEqual(first.value.meshes[0].vertices[1].position[0], 1.0f)
		|| first.value.meshes[0].vertices[1].position[0] != second.value.meshes[0].vertices[1].position[0]
		|| first.value.materialSlots.size() != 1 || first.value.materialSlots[0].name != "Stone")
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			first ? "Cooked glTF should load twice from the same source" : first.error.message);
	}

	if (!WriteBytes(assetsRoot / "Models" / "albedo.png", reinterpret_cast<const std::uint8_t*>("changed-png"), 11))
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF cook test image could not be replaced");
	}
	const Result<ModelAssetData> imageChanged = LoadModelAsset(assetsRoot, relativePath);
	auto imageCooked = imageChanged ? ReadCookedBytes(cookedPath) : cookedBytes;
	if (!imageChanged || !imageCooked || imageCooked.value == cookedBytes.value)
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			imageChanged ? "Replacing a glTF image should rebuild the cooked model" : imageChanged.error.message);
	}

	texturedBin[0] = 0;
	texturedBin[1] = 0;
	texturedBin[2] = 0x80;
	texturedBin[3] = 0x40;
	if (!WriteBytes(assetsRoot / "Models" / "triangle.bin", texturedBin.data(), texturedBin.size()))
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF cook test buffer could not be replaced");
	}
	const Result<ModelAssetData> binChanged = LoadModelAsset(assetsRoot, relativePath);
	if (!binChanged || binChanged.value.meshes.empty()
		|| !NearlyEqual(binChanged.value.meshes[0].vertices[0].position[0], 4.0f))
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			binChanged ? "Replacing a glTF buffer should rebuild the cooked mesh" : binChanged.error.message);
	}

	const std::vector<std::uint8_t> glb = MakeGlb(TriangleJson(false), bin);
	if (!WriteBytes(assetsRoot / "Models" / "triangle.glb", glb.data(), glb.size()))
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF cook test GLB could not be written");
	}
	const Result<ModelAssetData> glbAsset = LoadModelAsset(assetsRoot, "Models/triangle.glb");
	if (!glbAsset || glbAsset.value.meshes.size() != 1)
	{
		std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			glbAsset ? "Cooked GLB should contain a mesh" : glbAsset.error.message);
	}

	std::filesystem::remove_all(assetsRoot.parent_path(), errorCode);
	return MakeOk();
}

void MultiplyRowMajor(const float left[16], const float right[16], float out[16])
{
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			out[(row * 4) + column] = left[(row * 4) + 0] * right[column]
				+ left[(row * 4) + 1] * right[4 + column]
				+ left[(row * 4) + 2] * right[8 + column]
				+ left[(row * 4) + 3] * right[12 + column];
		}
	}
}

void WorldMatrix(const ModelAssetData& asset, int32_t nodeIndex, float out[16])
{
	std::memset(out, 0, sizeof(float) * 16);
	out[0] = out[5] = out[10] = out[15] = 1.0f;
	std::vector<int32_t> chain;
	for (int32_t index = nodeIndex; index >= 0; index = asset.nodes[static_cast<size_t>(index)].parentIndex)
	{
		chain.push_back(index);
	}

	for (auto it = chain.rbegin(); it != chain.rend(); ++it)
	{
		float combined[16]{};
		MultiplyRowMajor(asset.nodes[static_cast<size_t>(*it)].localTransform, out, combined);
		std::memcpy(out, combined, sizeof(combined));
	}
}

void TransformPoint(const float matrix[16], const float position[4], float out[3])
{
	out[0] = (position[0] * matrix[0]) + (position[1] * matrix[4]) + (position[2] * matrix[8]) + matrix[12];
	out[1] = (position[0] * matrix[1]) + (position[1] * matrix[5]) + (position[2] * matrix[9]) + matrix[13];
	out[2] = (position[0] * matrix[2]) + (position[1] * matrix[6]) + (position[2] * matrix[10]) + matrix[14];
}

[[nodiscard]] bool TriangleFaceDirection(const ModelMeshData& mesh, const float world[16], float out[3])
{
	if (mesh.indices.size() < 3)
	{
		return false;
	}

	float points[3][3]{};
	for (int corner = 0; corner < 3; ++corner)
	{
		const uint32_t index = mesh.indices[static_cast<size_t>(corner)];
		if (index >= mesh.vertices.size())
		{
			return false;
		}
		TransformPoint(world, mesh.vertices[index].position, points[corner]);
	}

	const float edgeU[3] = {
		points[1][0] - points[0][0],
		points[1][1] - points[0][1],
		points[1][2] - points[0][2],
	};
	const float edgeV[3] = {
		points[2][0] - points[0][0],
		points[2][1] - points[0][1],
		points[2][2] - points[0][2],
	};
	out[0] = (edgeU[1] * edgeV[2]) - (edgeU[2] * edgeV[1]);
	out[1] = (edgeU[2] * edgeV[0]) - (edgeU[0] * edgeV[2]);
	out[2] = (edgeU[0] * edgeV[1]) - (edgeU[1] * edgeV[0]);
	const float length = std::sqrt((out[0] * out[0]) + (out[1] * out[1]) + (out[2] * out[2]));
	if (length < 1.0e-5f)
	{
		return false;
	}

	out[0] /= length;
	out[1] /= length;
	out[2] /= length;
	return true;
}

void ExpandWorldBounds(const ModelMeshData& mesh, const float world[16], MeshBounds& bounds)
{
	for (const uint32_t index : mesh.indices)
	{
		if (index >= mesh.vertices.size())
		{
			continue;
		}

		float point[3]{};
		TransformPoint(world, mesh.vertices[index].position, point);
		ExpandBounds(bounds, point);
	}
}

[[nodiscard]] std::vector<std::uint8_t> ShapeBin()
{
	std::vector<std::uint8_t> bin;
	const float positions[9] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
	const float normals[9] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	const float uvs[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	for (float value : positions)
	{
		WriteFloat(bin, value);
	}
	for (float value : normals)
	{
		WriteFloat(bin, value);
	}
	for (float value : uvs)
	{
		WriteFloat(bin, value);
	}
	bin.push_back(0);
	bin.push_back(0);
	bin.push_back(1);
	bin.push_back(0);
	bin.push_back(2);
	bin.push_back(0);
	return bin;
}

[[nodiscard]] const ModelNodeData* FindNode(const ModelAssetData& asset, const char* name)
{
	for (const ModelNodeData& node : asset.nodes)
	{
		if (node.name == name)
		{
			return &node;
		}
	}

	return nullptr;
}

[[nodiscard]] bool WriteComparisonFbx(FbxSdkContext& context, const std::filesystem::path& path)
{
	fbxsdk::FbxManager* manager = context.GetManager();
	if (manager == nullptr)
	{
		return false;
	}

	fbxsdk::FbxScene* scene = fbxsdk::FbxScene::Create(manager, "Shape");
	if (scene == nullptr)
	{
		return false;
	}

	// FBX ConvertScene rotates the root and does not flip handedness. The file is already in the
	// DirectX space the glTF loader produces, so the importer leaves the triangle unchanged.
	scene->GetGlobalSettings().SetAxisSystem(fbxsdk::FbxAxisSystem::DirectX);
	scene->GetGlobalSettings().SetSystemUnit(fbxsdk::FbxSystemUnit::m);

	fbxsdk::FbxNode* pivot = fbxsdk::FbxNode::Create(scene, "Pivot");
	pivot->LclTranslation.Set(fbxsdk::FbxDouble3(0.0, 1.0, 0.0));
	scene->GetRootNode()->AddChild(pivot);

	fbxsdk::FbxMesh* mesh = fbxsdk::FbxMesh::Create(scene, "TriMesh");
	mesh->InitControlPoints(3);
	fbxsdk::FbxVector4* points = mesh->GetControlPoints();
	points[0] = fbxsdk::FbxVector4(0.0, 0.0, -1.0);
	points[1] = fbxsdk::FbxVector4(1.0, 0.0, -1.0);
	points[2] = fbxsdk::FbxVector4(0.0, 1.0, -1.0);
	mesh->BeginPolygon();
	mesh->AddPolygon(0);
	mesh->AddPolygon(2);
	mesh->AddPolygon(1);
	mesh->EndPolygon();

	fbxsdk::FbxGeometryElementNormal* normalElement = mesh->CreateElementNormal();
	normalElement->SetMappingMode(fbxsdk::FbxGeometryElement::eByControlPoint);
	normalElement->SetReferenceMode(fbxsdk::FbxGeometryElement::eDirect);
	normalElement->GetDirectArray().Add(fbxsdk::FbxVector4(0.0, 0.0, -1.0));
	normalElement->GetDirectArray().Add(fbxsdk::FbxVector4(0.0, 0.0, -1.0));
	normalElement->GetDirectArray().Add(fbxsdk::FbxVector4(0.0, 0.0, -1.0));

	fbxsdk::FbxGeometryElementUV* uvElement = mesh->CreateElementUV("UV0");
	uvElement->SetMappingMode(fbxsdk::FbxGeometryElement::eByControlPoint);
	uvElement->SetReferenceMode(fbxsdk::FbxGeometryElement::eDirect);
	uvElement->GetDirectArray().Add(fbxsdk::FbxVector2(0.0, 0.0));
	uvElement->GetDirectArray().Add(fbxsdk::FbxVector2(1.0, 0.0));
	uvElement->GetDirectArray().Add(fbxsdk::FbxVector2(0.0, 1.0));

	fbxsdk::FbxNode* triangle = fbxsdk::FbxNode::Create(scene, "Triangle");
	triangle->SetNodeAttribute(mesh);
	triangle->LclTranslation.Set(fbxsdk::FbxDouble3(1.0, 2.0, -3.0));
	triangle->LclScaling.Set(fbxsdk::FbxDouble3(2.0, 2.0, 2.0));
	pivot->AddChild(triangle);

	fbxsdk::FbxExporter* exporter = fbxsdk::FbxExporter::Create(manager, "");
	const std::string exportPath = path.string();
	const bool exported = exporter != nullptr
		&& exporter->Initialize(exportPath.c_str(), -1, context.GetIOSettings())
		&& exporter->Export(scene);
	if (exporter != nullptr)
	{
		exporter->Destroy();
	}
	scene->Destroy();
	return exported;
}

[[nodiscard]] Result<void> TestParentChildShape(const std::filesystem::path& tempRoot)
{
	const std::filesystem::path directory = tempRoot / "shape";
	std::error_code errorCode{};
	std::filesystem::create_directories(directory, errorCode);
	if (errorCode)
	{
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF shape test directory could not be created");
	}

	const std::vector<std::uint8_t> bin = ShapeBin();
	const std::string json = R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[
			{"name":"Pivot","translation":[0,1,0],"children":[1]},
			{"name":"Triangle","mesh":0,"translation":[1,2,3],"scale":[2,2,2]}
		],
		"images":[{"uri":"albedo.png"}],
		"textures":[{"source":0}],
		"materials":[{"name":"Stone","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
		"meshes":[{"name":"TriMesh","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
			{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":36},
			{"buffer":0,"byteOffset":72,"byteLength":24},
			{"buffer":0,"byteOffset":96,"byteLength":6}
		],
		"buffers":[{"uri":"shape.bin","byteLength":102}]
	})";
	const std::filesystem::path gltfPath = directory / "shape.gltf";
	const bool wrote = WriteBytes(directory / "shape.bin", bin.data(), bin.size())
		&& WriteBytes(directory / "albedo.png", reinterpret_cast<const std::uint8_t*>("png"), 3)
		&& WriteText(gltfPath, json);
	if (!wrote)
	{
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF shape test files could not be written");
	}

	const Result<ModelAssetData> gltf = LoadGltfModelAssetDataFromFile(gltfPath);
	if (!gltf)
	{
		return FailRuntime(LogCategory::Asset, ErrorCode::InvalidArgument, "glTF parent-child shape should load: " + gltf.error.message);
	}

	const ModelAssetData& asset = gltf.value;
	const ModelNodeData* pivot = FindNode(asset, "Pivot");
	const ModelNodeData* triangle = FindNode(asset, "Triangle");
	const ModelNodeData* root = FindNode(asset, "Root");
	const MeshBounds& bounds = asset.meshes.empty() ? MeshBounds{} : asset.meshes[0].bounds;
	const bool hierarchy = asset.meshes.size() == 1 && asset.materialSlots.size() == 1 && root != nullptr && pivot != nullptr
		&& triangle != nullptr && pivot->parentIndex == 0 && triangle->parentIndex == static_cast<int32_t>(pivot - asset.nodes.data())
		&& pivot->meshIndex < 0 && triangle->meshIndex == 0 && pivot->childNodeIndices.size() == 1
		&& root->childNodeIndices.size() == 1;
	const bool triangulated = hierarchy && asset.meshes[0].indices.size() == 3 && asset.meshes[0].indices.size() % 3 == 0
		&& asset.meshes[0].submeshes.size() == 1 && asset.meshes[0].submeshes[0].indexStart == 0
		&& asset.meshes[0].submeshes[0].indexCount == 3 && asset.meshes[0].indices[0] == 0
		&& asset.meshes[0].indices[1] == 2 && asset.meshes[0].indices[2] == 1;
	const BasicVertex& uvVertex = asset.meshes.empty() || asset.meshes[0].vertices.size() < 3
		? BasicVertex{}
		: asset.meshes[0].vertices[1];
	const BasicVertex& uvCorner = asset.meshes.empty() || asset.meshes[0].vertices.size() < 3
		? BasicVertex{}
		: asset.meshes[0].vertices[2];
	const bool uvs = NearlyEqual(uvVertex.uv[0], 1.0f) && NearlyEqual(uvVertex.uv[1], 0.0f)
		&& NearlyEqual(uvCorner.uv[0], 0.0f) && NearlyEqual(uvCorner.uv[1], 1.0f);
	const bool localBounds = NearlyEqual(bounds.minX, 0.0f) && NearlyEqual(bounds.maxX, 1.0f)
		&& NearlyEqual(bounds.minY, 0.0f) && NearlyEqual(bounds.maxY, 1.0f)
		&& NearlyEqual(bounds.minZ, -1.0f) && NearlyEqual(bounds.maxZ, -1.0f);
	const float* child = triangle != nullptr ? triangle->localTransform : nullptr;
	const float* parent = pivot != nullptr ? pivot->localTransform : nullptr;
	const bool matrices = child != nullptr && parent != nullptr && NearlyEqual(parent[0], 1.0f) && NearlyEqual(parent[5], 1.0f)
		&& NearlyEqual(parent[10], 1.0f) && NearlyEqual(parent[12], 0.0f) && NearlyEqual(parent[13], 1.0f)
		&& NearlyEqual(parent[14], 0.0f) && NearlyEqual(parent[15], 1.0f) && NearlyEqual(child[0], 2.0f)
		&& NearlyEqual(child[5], 2.0f) && NearlyEqual(child[10], 2.0f) && NearlyEqual(child[12], 1.0f)
		&& NearlyEqual(child[13], 2.0f) && NearlyEqual(child[14], -3.0f) && NearlyEqual(child[15], 1.0f);
	const std::filesystem::path albedo = CanonicalizeIfPossible(directory / "albedo.png");
	const bool material = asset.materialSlots.size() == 1 && asset.materialSlots[0].name == "Stone"
		&& asset.materialSlots[0].diffuseTexturePath == albedo && asset.materialSlots[0].normalTexturePath.empty()
		&& asset.materialSlots[0].specularTexturePath.empty();
	if (!hierarchy || !triangulated || !uvs || !localBounds || !matrices || !material)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF parent-child shape should keep a triangle, UVs, bounds, node matrices, and the base color path");
	}

	Result<FbxSdkContext> sdk = FbxSdkContext::Create();
	const std::filesystem::path fbxPath = directory / "shape.fbx";
	if (!sdk || !WriteComparisonFbx(sdk.value, fbxPath))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::RuntimeError,
			sdk ? "Comparison FBX could not be written" : sdk.error.message);
	}

	const Result<ModelAssetData> fbx = LoadModelAssetDataFromFile(fbxPath, sdk.value);
	if (!fbx || fbx.value.meshes.size() != 1)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			fbx ? "Comparison FBX should contain one mesh" : fbx.error.message);
	}

	const ModelNodeData* fbxMeshNode = nullptr;
	for (const ModelNodeData& node : fbx.value.nodes)
	{
		if (node.meshIndex == 0)
		{
			fbxMeshNode = &node;
			break;
		}
	}
	float gltfWorld[16]{};
	float fbxWorld[16]{};
	WorldMatrix(asset, static_cast<int32_t>(triangle - asset.nodes.data()), gltfWorld);
	if (fbxMeshNode != nullptr)
	{
		WorldMatrix(fbx.value, static_cast<int32_t>(fbxMeshNode - fbx.value.nodes.data()), fbxWorld);
	}

	MeshBounds gltfBounds{};
	MeshBounds fbxBounds{};
	gltfBounds.minX = gltfBounds.minY = gltfBounds.minZ = std::numeric_limits<float>::max();
	gltfBounds.maxX = gltfBounds.maxY = gltfBounds.maxZ = std::numeric_limits<float>::lowest();
	fbxBounds = gltfBounds;
	ExpandWorldBounds(asset.meshes[0], gltfWorld, gltfBounds);
	ExpandWorldBounds(fbx.value.meshes[0], fbxWorld, fbxBounds);
	float gltfFacing[3]{};
	float fbxFacing[3]{};
	const bool facing = TriangleFaceDirection(asset.meshes[0], gltfWorld, gltfFacing)
		&& TriangleFaceDirection(fbx.value.meshes[0], fbxWorld, fbxFacing);
	const bool boundsMatch = NearlyEqual(gltfBounds.minX, fbxBounds.minX) && NearlyEqual(gltfBounds.minY, fbxBounds.minY)
		&& NearlyEqual(gltfBounds.minZ, fbxBounds.minZ) && NearlyEqual(gltfBounds.maxX, fbxBounds.maxX)
		&& NearlyEqual(gltfBounds.maxY, fbxBounds.maxY) && NearlyEqual(gltfBounds.maxZ, fbxBounds.maxZ);
	const bool facingMatches = facing && NearlyEqual(gltfFacing[0], fbxFacing[0]) && NearlyEqual(gltfFacing[1], fbxFacing[1])
		&& NearlyEqual(gltfFacing[2], fbxFacing[2]) && gltfFacing[2] < 0.0f;
	const MeshBounds& fbxLocal = fbx.value.meshes[0].bounds;
	const bool localBoundsMatch = NearlyEqual(bounds.minX, fbxLocal.minX) && NearlyEqual(bounds.minY, fbxLocal.minY)
		&& NearlyEqual(bounds.minZ, fbxLocal.minZ) && NearlyEqual(bounds.maxX, fbxLocal.maxX)
		&& NearlyEqual(bounds.maxY, fbxLocal.maxY) && NearlyEqual(bounds.maxZ, fbxLocal.maxZ);
	if (fbxMeshNode == nullptr || !boundsMatch || !facingMatches || !localBoundsMatch)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF and FBX copies of the same triangle should share bounds and face direction");
	}

	return MakeOk();
}

[[nodiscard]] std::string SpecularMaterialJson(
	const char* documentPrefix,
	const char* images,
	const char* materialBody)
{
	return std::string(R"({
		"asset":{"version":"2.0"},
		)") + documentPrefix + R"(
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"images":[)" + images + R"(],
		"textures":[{"source":0},{"source":1},{"source":2},{"source":3}],
		"materials":[{)" + materialBody + R"(}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},
			{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":24},
			{"buffer":0,"byteOffset":60,"byteLength":6}
		],
		"buffers":[{"uri":"textured.bin","byteLength":66}]
	})";
}

[[nodiscard]] Result<void> ExpectSpecularTexture(
	const std::filesystem::path& tempRoot,
	const char* fileName,
	const std::string& json,
	const std::filesystem::path& expectedSpecular)
{
	if (!WriteText(tempRoot / fileName, json))
	{
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF specular test file could not be written");
	}

	const Result<ModelAssetData> loaded = LoadGltfModelAssetDataFromFile(tempRoot / fileName);
	if (!loaded || loaded.value.materialSlots.size() != 1
		|| loaded.value.materialSlots[0].specularTexturePath != expectedSpecular)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF specular extension should resolve the specular color texture");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestSpecularExtensions(const std::filesystem::path& tempRoot)
{
	const bool wroteImages = WriteBytes(tempRoot / "specular.png", reinterpret_cast<const std::uint8_t*>("png"), 3)
		&& WriteBytes(tempRoot / "gloss.png", reinterpret_cast<const std::uint8_t*>("png"), 3)
		&& WriteBytes(tempRoot / "factor.png", reinterpret_cast<const std::uint8_t*>("png"), 3);
	if (!wroteImages)
	{
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF specular images could not be written");
	}

	const std::filesystem::path specularPath = CanonicalizeIfPossible(tempRoot / "specular.png");
	const std::filesystem::path glossPath = CanonicalizeIfPossible(tempRoot / "gloss.png");
	const char* colorImages =
		R"({"uri":"albedo.png"},{"uri":"normal.png"},{"uri":"specular.png"},{"uri":"gloss.png"})";
	const char* colorAndGloss = R"(
		"name":"Stone",
		"extensions":{
			"KHR_materials_specular":{"specularColorTexture":{"index":2},"specularTexture":{"index":3}},
			"KHR_materials_pbrSpecularGlossiness":{"specularGlossinessTexture":{"index":3}}
		})";
	if (auto color = ExpectSpecularTexture(
			tempRoot,
			"specular-color.gltf",
			SpecularMaterialJson("", colorImages, colorAndGloss),
			specularPath);
		!color)
	{
		return color;
	}

	const char* glossImages =
		R"({"uri":"albedo.png"},{"uri":"normal.png"},{"uri":"gloss.png"},{"uri":"factor.png"})";
	const char* glossOnly = R"(
		"name":"Stone",
		"extensions":{"KHR_materials_pbrSpecularGlossiness":{"specularGlossinessTexture":{"index":2}}})";
	if (auto gloss = ExpectSpecularTexture(
			tempRoot,
			"specular-gloss.gltf",
			SpecularMaterialJson("", glossImages, glossOnly),
			glossPath);
		!gloss)
	{
		return gloss;
	}

	const char* factorOnly = R"("name":"Stone","extensions":{"KHR_materials_specular":{"specularTexture":{"index":2}}})";
	return ExpectSpecularTexture(
		tempRoot,
		"specular-factor.gltf",
		SpecularMaterialJson(
			R"("extensionsUsed":["KHR_materials_specular"],"extensionsRequired":["KHR_materials_specular"],)",
			glossImages,
			factorOnly),
		{});
}
} // namespace

Result<void> RunGltfModelFileLoaderTests()
{
	const std::filesystem::path tempRoot = std::filesystem::temp_directory_path() / "AetherGltfLoader";
	std::error_code errorCode{};
	std::filesystem::remove_all(tempRoot, errorCode);
	std::filesystem::create_directories(tempRoot / "Models", errorCode);
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"glTF loader test directory could not be created");
	}

	const std::vector<std::uint8_t> bin = TriangleBin();
	const std::vector<std::uint8_t> glb = MakeGlb(TriangleJson(false), bin);
	const std::filesystem::path glbPath = tempRoot / "triangle.glb";
	const std::filesystem::path gltfPath = tempRoot / "triangle.gltf";
	const bool wroteTriangle = WriteBytes(glbPath, glb.data(), glb.size())
		&& WriteBytes(tempRoot / "triangle.bin", bin.data(), bin.size())
		&& WriteText(gltfPath, TriangleJson(true));
	if (!wroteTriangle)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF loader test files could not be written");
	}

	const Result<ModelAssetData> glbAsset = LoadGltfModelAssetDataFromFile(glbPath);
	const Result<ModelAssetData> gltfAsset = LoadGltfModelAssetDataFromFile(gltfPath);
	if (!glbAsset || !gltfAsset)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF triangle file should load: "
				+ (glbAsset ? gltfAsset.error.message : glbAsset.error.message));
	}
	if (auto triangle = ExpectTriangle(glbAsset.value, true); !triangle)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return triangle;
	}
	if (auto triangle = ExpectTriangle(gltfAsset.value, true); !triangle)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return triangle;
	}

	const std::string generatedJson = R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":120,"byteLength":6}
		],
		"buffers":[{"uri":"triangle.bin","byteLength":126}]
	})";
	const std::filesystem::path generatedPath = tempRoot / "generated.gltf";
	if (!WriteText(generatedPath, generatedJson))
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF generated-normal test file could not be written");
	}
	const Result<ModelAssetData> generated = LoadGltfModelAssetDataFromFile(generatedPath);
	if (!generated || generated.value.meshes.size() != 1 || generated.value.meshes[0].vertices.size() != 3
		|| !NearlyEqual(generated.value.meshes[0].vertices[0].normal[2], -1.0f))
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF file without normals should generate DirectX-space normals");
	}

	const std::string sharedJson = R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0,1]}],
		"nodes":[{"name":"A","mesh":0},{"name":"B","mesh":0,"translation":[4,0,0]}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":120,"byteLength":6}
		],
		"buffers":[{"uri":"triangle.bin","byteLength":126}]
	})";
	if (!WriteText(tempRoot / "shared.gltf", sharedJson))
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF shared-mesh test file could not be written");
	}
	const Result<ModelAssetData> shared = LoadGltfModelAssetDataFromFile(tempRoot / "shared.gltf");
	if (!shared || shared.value.meshes.size() != 1 || shared.value.nodes.size() != 3
		|| shared.value.nodes[1].meshIndex != 0 || shared.value.nodes[2].meshIndex != 0
		|| !NearlyEqual(shared.value.nodes[2].localTransform[12], 4.0f))
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF mesh referenced by two nodes should be stored once");
	}

	std::vector<std::uint8_t> texturedBin;
	const float uvs[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	texturedBin.insert(texturedBin.end(), bin.begin(), bin.begin() + 36);
	for (float value : uvs)
	{
		WriteFloat(texturedBin, value);
	}
	texturedBin.insert(texturedBin.end(), bin.end() - 6, bin.end());
	const std::string texturedJson = R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"images":[{"uri":"albedo.png"},{"uri":"normal.png"}],
		"textures":[{"source":0},{"source":1}],
		"materials":[{"name":"Stone","pbrMetallicRoughness":{"baseColorTexture":{"index":0}},"normalTexture":{"index":1}}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},
			{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":24},
			{"buffer":0,"byteOffset":60,"byteLength":6}
		],
		"buffers":[{"uri":"textured.bin","byteLength":66}]
	})";
	const bool wroteTextures = WriteBytes(tempRoot / "textured.bin", texturedBin.data(), texturedBin.size())
		&& WriteBytes(tempRoot / "albedo.png", reinterpret_cast<const std::uint8_t*>("png"), 3)
		&& WriteBytes(tempRoot / "normal.png", reinterpret_cast<const std::uint8_t*>("png"), 3)
		&& WriteText(tempRoot / "textured.gltf", texturedJson);
	if (!wroteTextures)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF material test files could not be written");
	}
	const Result<ModelAssetData> textured = LoadGltfModelAssetDataFromFile(tempRoot / "textured.gltf");
	const std::filesystem::path albedoPath = CanonicalizeIfPossible(tempRoot / "albedo.png");
	const std::filesystem::path normalPath = CanonicalizeIfPossible(tempRoot / "normal.png");
	if (!textured || textured.value.materialSlots.size() != 1 || textured.value.materialSlots[0].name != "Stone"
		|| textured.value.materialSlots[0].diffuseTexturePath != albedoPath
		|| textured.value.materialSlots[0].normalTexturePath != normalPath
		|| !textured.value.materialSlots[0].specularTexturePath.empty()
		|| !NearlyEqual(textured.value.meshes[0].vertices[1].uv[0], 1.0f))
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF material textures should resolve beside the file");
	}

	if (auto specular = TestSpecularExtensions(tempRoot); !specular)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return specular;
	}

	const std::string dataUriJson = R"({
		"asset":{"version":"2.0"},
		"images":[{"uri":"data:image/png;base64,AA=="}],
		"textures":[{"source":0}],
		"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},
			{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":24},
			{"buffer":0,"byteOffset":60,"byteLength":6}
		],
		"buffers":[{"uri":"textured.bin","byteLength":66}]
	})";
	const std::string ktxJson = R"({
		"asset":{"version":"2.0"},
		"images":[{"uri":"color.ktx2"}],
		"textures":[{"source":0}],
		"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},
			{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":24},
			{"buffer":0,"byteOffset":60,"byteLength":6}
		],
		"buffers":[{"uri":"textured.bin","byteLength":66}]
	})";
	const std::string escapeJson = R"({
		"asset":{"version":"2.0"},
		"buffers":[{"uri":"../outside.bin","byteLength":126}]
	})";
	const std::string encodedEscapeJson = R"({
		"asset":{"version":"2.0"},
		"buffers":[{"uri":"%2E%2E/outside.bin","byteLength":126}]
	})";
	const std::string pointsJson = R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":120,"byteLength":6}
		],
		"buffers":[{"uri":"triangle.bin","byteLength":126}]
	})";
	const std::string animationJson = R"({
		"asset":{"version":"2.0"},
		"animations":[{"samplers":[],"channels":[]}]
	})";
	const std::string absoluteUriJson = R"({
		"asset":{"version":"2.0"},
		"images":[{"uri":"C:/Outside/albedo.png"}],
		"textures":[{"source":0}],
		"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},
			{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":36,"byteLength":24},
			{"buffer":0,"byteOffset":60,"byteLength":6}
		],
		"buffers":[{"uri":"textured.bin","byteLength":66}]
	})";
	const std::string stripJson = R"({
		"asset":{"version":"2.0"},
		"scene":0,
		"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":5}]}],
		"accessors":[
			{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
			{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
		],
		"bufferViews":[
			{"buffer":0,"byteOffset":0,"byteLength":36},
			{"buffer":0,"byteOffset":120,"byteLength":6}
		],
		"buffers":[{"uri":"triangle.bin","byteLength":126}]
	})";
	const bool wroteRejections = WriteText(tempRoot / "data-uri.gltf", dataUriJson)
		&& WriteText(tempRoot / "ktx.gltf", ktxJson)
		&& WriteBytes(tempRoot / "color.ktx2", reinterpret_cast<const std::uint8_t*>("ktx"), 3)
		&& WriteBytes(tempRoot / "outside.bin", bin.data(), bin.size())
		&& WriteText(tempRoot / "Models" / "escape.gltf", escapeJson)
		&& WriteText(tempRoot / "encoded-escape.gltf", encodedEscapeJson)
		&& WriteText(tempRoot / "points.gltf", pointsJson)
		&& WriteText(tempRoot / "animation.gltf", animationJson)
		&& WriteText(tempRoot / "absolute.gltf", absoluteUriJson)
		&& WriteText(tempRoot / "strip.gltf", stripJson)
		&& WriteText(tempRoot / "broken.gltf", "{\"asset\":{\"version\":\"2.0\"}, \"meshes\": [");
	if (!wroteRejections)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(LogCategory::Asset, ErrorCode::FileNotFound, "glTF rejection test files could not be written");
	}

	const Result<ModelAssetData> dataUri = LoadGltfModelAssetDataFromFile(tempRoot / "data-uri.gltf");
	const Result<ModelAssetData> ktx = LoadGltfModelAssetDataFromFile(tempRoot / "ktx.gltf");
	const Result<ModelAssetData> escape = LoadGltfModelAssetDataFromFile(tempRoot / "Models" / "escape.gltf");
	const Result<ModelAssetData> encodedEscape = LoadGltfModelAssetDataFromFile(tempRoot / "encoded-escape.gltf");
	const Result<ModelAssetData> points = LoadGltfModelAssetDataFromFile(tempRoot / "points.gltf");
	const Result<ModelAssetData> animation = LoadGltfModelAssetDataFromFile(tempRoot / "animation.gltf");
	const Result<ModelAssetData> absoluteUri = LoadGltfModelAssetDataFromFile(tempRoot / "absolute.gltf");
	const Result<ModelAssetData> strip = LoadGltfModelAssetDataFromFile(tempRoot / "strip.gltf");
	const Result<ModelAssetData> broken = LoadGltfModelAssetDataFromFile(tempRoot / "broken.gltf");
	const Result<ModelAssetData> wrongExtension = LoadGltfModelAssetDataFromFile(tempRoot / "triangle.bin");
	const Result<ModelAssetData>* rejections[] = {
		&dataUri, &ktx, &escape, &encodedEscape, &points, &animation, &absoluteUri, &strip, &broken, &wrongExtension,
	};
	const char* rejectionNames[] = {
		"data URI", "png, jpg", "beside the file", "beside the file", "TRIANGLES", "animations",
		"beside the file", "TRIANGLES", "JSON", ".gltf or .glb",
	};
	std::string rejectionFailure{};
	for (size_t index = 0; index < 10; ++index)
	{
		if (!MessageContains(*rejections[index], rejectionNames[index]))
		{
			rejectionFailure += rejectionNames[index];
			rejectionFailure += " got: ";
			rejectionFailure += rejections[index]->error.message.empty() ? "loaded" : rejections[index]->error.message;
			rejectionFailure += "; ";
		}
	}
	if (!rejectionFailure.empty())
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF loader should reject unsupported files: " + rejectionFailure);
	}

	if (auto shape = TestParentChildShape(tempRoot); !shape)
	{
		std::filesystem::remove_all(tempRoot, errorCode);
		return shape;
	}

	std::filesystem::remove_all(tempRoot, errorCode);
	if (auto cooked = TestCookedGltfAsset(); !cooked)
	{
		return cooked;
	}

	LOG_INFO(LogCategory::Asset, "GltfModelFileLoaderTests passed");
	return MakeOk();
}
