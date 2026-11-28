#include "Runtime/AssetSystem/Model/Loader/GltfModelFileLoader.h"

#include "Runtime/AssetSystem/Model/VertexTangents.h"
#include "Runtime/Core/Hash/Fnv.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Core/IO/BinaryFile.h"
#include "Runtime/Core/IO/Path.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"

#include "ThirdParty/cgltf/cgltf.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
constexpr cgltf_size kMaxAccessorElements = 5000000;
constexpr int kMaxNodeDepth = 256;

extern "C" cgltf_result ReadGltfFile(
	const cgltf_memory_options* memoryOptions,
	const cgltf_file_options* fileOptions,
	const char* path,
	cgltf_size* size,
	void** data)
{
	(void)fileOptions;
	if (memoryOptions == nullptr || path == nullptr || size == nullptr || data == nullptr)
	{
		return cgltf_result_invalid_options;
	}

	*data = nullptr;
	const std::filesystem::path filePath = PathFromUtf8(path);
	std::error_code errorCode{};
	const std::uintmax_t fileSizeValue = std::filesystem::file_size(filePath, errorCode);
	if (errorCode || fileSizeValue > (std::numeric_limits<cgltf_size>::max)())
	{
		return errorCode ? cgltf_result_file_not_found : cgltf_result_io_error;
	}

	const cgltf_size fileSize = static_cast<cgltf_size>(fileSizeValue);
	const cgltf_size bytesToRead = *size == 0 ? fileSize : *size;
	if (bytesToRead == 0 || bytesToRead > fileSize)
	{
		return cgltf_result_data_too_short;
	}

	void* buffer = memoryOptions->alloc_func != nullptr
		? memoryOptions->alloc_func(memoryOptions->user_data, bytesToRead)
		: std::malloc(bytesToRead);
	if (buffer == nullptr)
	{
		return cgltf_result_out_of_memory;
	}

	std::ifstream file(filePath, std::ios::binary);
	if (!file.read(static_cast<char*>(buffer), static_cast<std::streamsize>(bytesToRead))
		|| static_cast<cgltf_size>(file.gcount()) != bytesToRead)
	{
		if (memoryOptions->free_func != nullptr)
		{
			memoryOptions->free_func(memoryOptions->user_data, buffer);
		}
		else
		{
			std::free(buffer);
		}
		return cgltf_result_io_error;
	}

	*size = bytesToRead;
	*data = buffer;
	return cgltf_result_success;
}

extern "C" void ReleaseGltfFile(
	const cgltf_memory_options* memoryOptions,
	const cgltf_file_options* fileOptions,
	void* data)
{
	(void)fileOptions;
	if (data == nullptr)
	{
		return;
	}

	if (memoryOptions != nullptr && memoryOptions->free_func != nullptr)
	{
		memoryOptions->free_func(memoryOptions->user_data, data);
		return;
	}

	std::free(data);
}

struct GltfData
{
	cgltf_data* data = nullptr;

	~GltfData()
	{
		if (data != nullptr)
		{
			cgltf_free(data);
		}
	}

	GltfData() = default;
	GltfData(const GltfData&) = delete;
	GltfData& operator=(const GltfData&) = delete;
};

[[nodiscard]] const char* DescribeGltfResult(cgltf_result result)
{
	switch (result)
	{
	case cgltf_result_data_too_short:
		return "data is too short";
	case cgltf_result_unknown_format:
		return "unknown format";
	case cgltf_result_invalid_json:
		return "invalid JSON";
	case cgltf_result_invalid_gltf:
		return "invalid glTF";
	case cgltf_result_file_not_found:
		return "file not found";
	case cgltf_result_io_error:
		return "file could not be read";
	case cgltf_result_out_of_memory:
		return "out of memory";
	case cgltf_result_legacy_gltf:
		return "glTF 1.0 is not supported";
	default:
		return "parse failed";
	}
}

[[nodiscard]] bool HasGltfExtension(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	for (char& character : extension)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension == ".gltf" || extension == ".glb";
}

[[nodiscard]] const char* NameOr(const char* name, const char* fallback)
{
	if (name == nullptr || name[0] == '\0')
	{
		return fallback;
	}

	return name;
}

[[nodiscard]] std::string DecodeUri(const char* uri)
{
	std::string decoded = uri != nullptr ? uri : "";
	const cgltf_size length = cgltf_decode_uri(decoded.data());
	decoded.resize(length);
	return decoded;
}

[[nodiscard]] bool IsSafeRelativeFileUri(const std::string& decoded)
{
	if (decoded.empty() || decoded.find(':') != std::string::npos || decoded.find('\\') != std::string::npos)
	{
		return false;
	}

	const std::filesystem::path path = PathFromUtf8(decoded);
	return !path.empty() && !path.is_absolute() && !ContainsParentReference(path);
}

[[nodiscard]] bool HasSupportedTextureExtension(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	for (char& character : extension)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp"
		|| extension == ".dds";
}

// glTF writes column-major column-vector matrices. That memory is already the transpose, which is
// the row-vector layout used by the FBX importer. Negate the Z row and Z column, except the ZZ
// element, to mirror the right-handed scene into DirectX.
void StoreEngineMatrix(float destination[16], const float columnMajor[16], bool convertAxis)
{
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			float value = columnMajor[(row * 4) + column];
			if (convertAxis && (row == 2) != (column == 2))
			{
				value = -value;
			}
			destination[(row * 4) + column] = value;
		}
	}
}

[[nodiscard]] bool IsFiniteVector(const float* values, size_t count)
{
	for (size_t index = 0; index < count; ++index)
	{
		if (!std::isfinite(values[index]))
		{
			return false;
		}
	}

	return true;
}

void GenerateNormalsForRange(
	std::vector<BasicVertex>& vertices,
	const std::vector<uint32_t>& indices,
	uint32_t vertexBegin,
	uint32_t vertexCount,
	uint32_t indexBegin,
	uint32_t indexCount)
{
	for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		float* normal = vertices[static_cast<size_t>(vertexBegin) + vertexIndex].normal;
		normal[0] = 0.0f;
		normal[1] = 0.0f;
		normal[2] = 0.0f;
	}

	for (uint32_t index = 0; index + 2 < indexCount; index += 3)
	{
		const uint32_t index0 = indices[static_cast<size_t>(indexBegin) + index];
		const uint32_t index1 = indices[static_cast<size_t>(indexBegin) + index + 1];
		const uint32_t index2 = indices[static_cast<size_t>(indexBegin) + index + 2];
		const BasicVertex& vertex0 = vertices[index0];
		const BasicVertex& vertex1 = vertices[index1];
		const BasicVertex& vertex2 = vertices[index2];
		const float edge1[3] = {
			vertex1.position[0] - vertex0.position[0],
			vertex1.position[1] - vertex0.position[1],
			vertex1.position[2] - vertex0.position[2],
		};
		const float edge2[3] = {
			vertex2.position[0] - vertex0.position[0],
			vertex2.position[1] - vertex0.position[1],
			vertex2.position[2] - vertex0.position[2],
		};
		const float faceNormal[3] = {
			(edge1[1] * edge2[2]) - (edge1[2] * edge2[1]),
			(edge1[2] * edge2[0]) - (edge1[0] * edge2[2]),
			(edge1[0] * edge2[1]) - (edge1[1] * edge2[0]),
		};
		for (const uint32_t vertexIndex : { index0, index1, index2 })
		{
			float* normal = vertices[vertexIndex].normal;
			normal[0] += faceNormal[0];
			normal[1] += faceNormal[1];
			normal[2] += faceNormal[2];
		}
	}

	for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		float* normal = vertices[static_cast<size_t>(vertexBegin) + vertexIndex].normal;
		const float length = std::sqrt((normal[0] * normal[0]) + (normal[1] * normal[1]) + (normal[2] * normal[2]));
		if (length < 1.0e-8f)
		{
			normal[0] = 0.0f;
			normal[1] = 1.0f;
			normal[2] = 0.0f;
			continue;
		}

		const float inverse = 1.0f / length;
		normal[0] *= inverse;
		normal[1] *= inverse;
		normal[2] *= inverse;
	}
}

void GenerateTangentsForRange(
	std::vector<BasicVertex>& vertices,
	const std::vector<uint32_t>& indices,
	uint32_t vertexBegin,
	uint32_t vertexCount,
	uint32_t indexBegin,
	uint32_t indexCount)
{
	std::vector<BasicVertex> localVertices(
		vertices.begin() + static_cast<std::ptrdiff_t>(vertexBegin),
		vertices.begin() + static_cast<std::ptrdiff_t>(vertexBegin) + static_cast<std::ptrdiff_t>(vertexCount));
	std::vector<uint32_t> localIndices(indexCount);
	for (uint32_t index = 0; index < indexCount; ++index)
	{
		localIndices[index] = indices[static_cast<size_t>(indexBegin) + index] - vertexBegin;
	}

	GenerateVertexTangents(localVertices, localIndices);
	for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		const float* tangent = localVertices[vertexIndex].tangent;
		float* destination = vertices[static_cast<size_t>(vertexBegin) + vertexIndex].tangent;
		destination[0] = tangent[0];
		destination[1] = tangent[1];
		destination[2] = tangent[2];
		destination[3] = tangent[3];
	}
}

[[nodiscard]] Result<std::vector<float>> ReadAccessorFloats(
	const cgltf_accessor* accessor,
	cgltf_type type,
	const char* label)
{
	if (accessor == nullptr || accessor->type != type || accessor->count == 0)
	{
		return FailRuntime<std::vector<float>>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			std::string("glTF accessor is missing or has the wrong type: ") + label);
	}

	if (accessor->count > kMaxAccessorElements)
	{
		return FailRuntime<std::vector<float>>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			std::string("glTF accessor is too large: ") + label);
	}

	const cgltf_size components = cgltf_num_components(type);
	std::vector<float> values(static_cast<size_t>(accessor->count * components));
	const cgltf_size readCount = cgltf_accessor_unpack_floats(accessor, values.data(), values.size());
	if (readCount != values.size())
	{
		return FailRuntime<std::vector<float>>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			std::string("glTF accessor could not be read: ") + label);
	}

	return MakeOk(std::move(values));
}

// Specular color, matching an FBX specular map. The specular factor texture is a scalar and is left unused.
[[nodiscard]] const cgltf_texture_view* SpecularColorTextureView(const cgltf_material& material)
{
	if (material.has_specular && material.specular.specular_color_texture.texture != nullptr)
	{
		return &material.specular.specular_color_texture;
	}

	if (material.has_pbr_specular_glossiness
		&& material.pbr_specular_glossiness.specular_glossiness_texture.texture != nullptr)
	{
		return &material.pbr_specular_glossiness.specular_glossiness_texture;
	}

	return nullptr;
}

[[nodiscard]] bool IsSupportedRequiredExtension(const char* name)
{
	return name != nullptr
		&& (std::strcmp(name, "KHR_materials_unlit") == 0
			|| std::strcmp(name, "KHR_mesh_quantization") == 0
			|| std::strcmp(name, "KHR_materials_specular") == 0
			|| std::strcmp(name, "KHR_materials_pbrSpecularGlossiness") == 0);
}

struct GltfImporter
{
	std::filesystem::path absolutePath{};
	std::filesystem::path directory{};
	FbxModelLoadOptions importOptions{};
	cgltf_data* data = nullptr;
	ModelAssetData asset{};
	std::unordered_map<const cgltf_mesh*, int32_t> meshIndices{};
	std::unordered_map<const cgltf_material*, uint32_t> materialSlots{};
	bool hasDefaultMaterial = false;
	uint32_t defaultMaterialSlot = 0;
	std::unordered_set<const cgltf_node*> visited{};

	[[nodiscard]] Result<void> Import()
	{
		if (auto rejected = RejectUnsupportedFeatures(); !rejected)
		{
			return rejected;
		}

		if (auto buffers = ValidateBufferUris(); !buffers)
		{
			return buffers;
		}

		cgltf_options options{};
		options.file.read = &ReadGltfFile;
		options.file.release = &ReleaseGltfFile;
		const std::string utf8Path = PathToUtf8(absolutePath);
		const cgltf_result loaded = cgltf_load_buffers(&options, data, utf8Path.c_str());
		if (loaded != cgltf_result_success)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::RuntimeError,
				"Failed to load glTF buffers for " + absolutePath.string() + ": " + DescribeGltfResult(loaded));
		}

		const cgltf_result validated = cgltf_validate(data);
		if (validated != cgltf_result_success)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF file is invalid: " + absolutePath.string() + ": " + DescribeGltfResult(validated));
		}

		asset.sourcePath = absolutePath;
		ModelNodeData rootNode{};
		rootNode.name = "Root";
		Aether::Math::MakeIdentityMatrix(rootNode.localTransform);
		asset.rootNodeIndex = 0;
		asset.nodes.push_back(std::move(rootNode));

		const cgltf_scene* scene = data->scene;
		if (scene == nullptr && data->scenes_count > 0)
		{
			scene = &data->scenes[0];
		}
		if (scene == nullptr)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF file has no scene: " + absolutePath.string());
		}

		for (cgltf_size nodeIndex = 0; nodeIndex < scene->nodes_count; ++nodeIndex)
		{
			if (scene->nodes[nodeIndex] == nullptr)
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF scene node is missing: " + absolutePath.string());
			}

			if (auto visitedNode = VisitNode(scene->nodes[nodeIndex], 0, 1); !visitedNode)
			{
				return visitedNode;
			}
		}

		if (asset.meshes.empty())
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF file contains no meshes: " + absolutePath.string());
		}

		return MakeOk();
	}

	[[nodiscard]] Result<void> RejectUnsupportedFeatures()
	{
		for (cgltf_size index = 0; index < data->extensions_required_count; ++index)
		{
			const char* name = data->extensions_required[index];
			if (!IsSupportedRequiredExtension(name))
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					std::string("glTF required extension is not supported: ") + (name != nullptr ? name : ""));
			}
		}

		if (data->animations_count > 0)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF animations are not supported: " + absolutePath.string());
		}

		if (data->skins_count > 0)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF skins are not supported: " + absolutePath.string());
		}

		for (cgltf_size index = 0; index < data->buffer_views_count; ++index)
		{
			if (data->buffer_views[index].has_meshopt_compression)
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF meshopt compression is not supported: " + absolutePath.string());
			}
		}

		for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
		{
			const cgltf_mesh& mesh = data->meshes[meshIndex];
			if (mesh.weights_count > 0)
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF morph targets are not supported: " + absolutePath.string());
			}

			for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
			{
				const cgltf_primitive& primitive = mesh.primitives[primitiveIndex];
				if (primitive.type != cgltf_primitive_type_triangles)
				{
					return FailRuntime(
						LogCategory::Asset,
						ErrorCode::InvalidArgument,
						"glTF primitive must be TRIANGLES: " + absolutePath.string());
				}
				if (primitive.targets_count > 0)
				{
					return FailRuntime(
						LogCategory::Asset,
						ErrorCode::InvalidArgument,
						"glTF morph targets are not supported: " + absolutePath.string());
				}
				if (primitive.has_draco_mesh_compression)
				{
					return FailRuntime(
						LogCategory::Asset,
						ErrorCode::InvalidArgument,
						"glTF Draco compression is not supported: " + absolutePath.string());
				}

				for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
				{
					const cgltf_attribute_type type = primitive.attributes[attributeIndex].type;
					if (type == cgltf_attribute_type_joints || type == cgltf_attribute_type_weights)
					{
						return FailRuntime(
							LogCategory::Asset,
							ErrorCode::InvalidArgument,
							"glTF skins are not supported: " + absolutePath.string());
					}
				}
			}
		}

		for (cgltf_size nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex)
		{
			const cgltf_node& node = data->nodes[nodeIndex];
			if (node.skin != nullptr)
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF skins are not supported: " + absolutePath.string());
			}
			if (node.weights_count > 0)
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF morph targets are not supported: " + absolutePath.string());
			}
			if (node.has_mesh_gpu_instancing)
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF mesh instancing is not supported: " + absolutePath.string());
			}
		}

		return MakeOk();
	}

	[[nodiscard]] Result<void> ValidateBufferUris()
	{
		for (cgltf_size index = 0; index < data->buffers_count; ++index)
		{
			const char* uri = data->buffers[index].uri;
			if (uri == nullptr)
			{
				continue;
			}
			if (std::strncmp(uri, "data:", 5) == 0)
			{
				continue;
			}
			if (std::strstr(uri, "://") != nullptr || !IsSafeRelativeFileUri(DecodeUri(uri)))
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF buffer URI must stay beside the file: " + absolutePath.string());
			}
		}

		return MakeOk();
	}

	[[nodiscard]] Result<std::filesystem::path> ResolveImage(const cgltf_image& image)
	{
		if (image.uri == nullptr || image.uri[0] == '\0' || image.buffer_view != nullptr)
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF embedded images are not supported: " + absolutePath.string());
		}
		if (std::strncmp(image.uri, "data:", 5) == 0)
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF image data URIs are not supported: " + absolutePath.string());
		}

		const std::string decoded = DecodeUri(image.uri);
		if (!IsSafeRelativeFileUri(decoded))
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF image URI must stay beside the file: " + absolutePath.string());
		}

		const std::filesystem::path relativePath = PathFromUtf8(decoded);
		if (!HasSupportedTextureExtension(relativePath))
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF texture image must be png, jpg, jpeg, bmp, or dds: " + relativePath.string());
		}

		const std::filesystem::path absoluteImagePath = directory / relativePath;
		std::error_code errorCode{};
		if (!std::filesystem::exists(absoluteImagePath, errorCode) || errorCode)
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::FileNotFound,
				"glTF texture file not found: " + absoluteImagePath.string());
		}

		const std::filesystem::path resolved = CanonicalizeIfPossible(absoluteImagePath);
		if (!IsPathUnderRoot(resolved, CanonicalDirectory(directory)))
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF image URI must stay beside the file: " + absolutePath.string());
		}

		return MakeOk(resolved);
	}

	[[nodiscard]] Result<std::filesystem::path> ResolveTextureView(
		const cgltf_texture_view& view,
		bool isNormal)
	{
		if (view.texture == nullptr)
		{
			return MakeOk(std::filesystem::path{});
		}
		if (view.texcoord != 0 || view.has_transform)
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF texture transforms and extra UV sets are not supported: " + absolutePath.string());
		}
		if (isNormal && std::fabs(view.scale - 1.0f) > 1.0e-4f)
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF normal texture scale is not supported: " + absolutePath.string());
		}

		const cgltf_texture* texture = view.texture;
		if (texture->image == nullptr)
		{
			return FailRuntime<std::filesystem::path>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				texture->has_basisu || texture->has_webp
					? "glTF compressed textures are not supported: " + absolutePath.string()
					: "glTF texture image is missing: " + absolutePath.string());
		}

		return ResolveImage(*texture->image);
	}

	[[nodiscard]] Result<uint32_t> MaterialSlotFor(const cgltf_material* material)
	{
		if (material == nullptr)
		{
			if (!hasDefaultMaterial)
			{
				defaultMaterialSlot = static_cast<uint32_t>(asset.materialSlots.size());
				asset.materialSlots.push_back(ModelMaterialSlotData{ "Default", {}, {} });
				hasDefaultMaterial = true;
			}
			return MakeOk(defaultMaterialSlot);
		}

		const auto existing = materialSlots.find(material);
		if (existing != materialSlots.end())
		{
			return MakeOk(existing->second);
		}

		ModelMaterialSlotData slot{};
		slot.name = NameOr(material->name, "Material");
		if (material->has_pbr_metallic_roughness)
		{
			auto diffuse = ResolveTextureView(material->pbr_metallic_roughness.base_color_texture, false);
			if (!diffuse)
			{
				return MakeFail<uint32_t>(diffuse.error.code, diffuse.error.message);
			}
			slot.diffuseTexturePath = std::move(diffuse.value);
		}

		auto normal = ResolveTextureView(material->normal_texture, true);
		if (!normal)
		{
			return MakeFail<uint32_t>(normal.error.code, normal.error.message);
		}
		slot.normalTexturePath = std::move(normal.value);

		if (const cgltf_texture_view* specularView = SpecularColorTextureView(*material))
		{
			auto specular = ResolveTextureView(*specularView, false);
			if (!specular)
			{
				return MakeFail<uint32_t>(specular.error.code, specular.error.message);
			}
			slot.specularTexturePath = std::move(specular.value);
		}

		const uint32_t slotIndex = static_cast<uint32_t>(asset.materialSlots.size());
		asset.materialSlots.push_back(std::move(slot));
		materialSlots.emplace(material, slotIndex);
		return MakeOk(slotIndex);
	}

	[[nodiscard]] Result<void> AppendPrimitive(const cgltf_primitive& primitive, ModelMeshData& mesh)
	{
		const auto materialSlot = MaterialSlotFor(primitive.material);
		if (!materialSlot)
		{
			return MakeFail(materialSlot.error.code, materialSlot.error.message);
		}

		const cgltf_accessor* positionAccessor =
			cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
		auto positions = ReadAccessorFloats(positionAccessor, cgltf_type_vec3, "POSITION");
		if (!positions)
		{
			return MakeFail(positions.error.code, positions.error.message);
		}

		const cgltf_accessor* normalAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
		const cgltf_accessor* tangentAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0);
		const cgltf_accessor* uvAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
		const ModelMaterialSlotData& slot = asset.materialSlots[materialSlot.value];
		const bool needsUv = !slot.diffuseTexturePath.empty() || !slot.normalTexturePath.empty()
			|| !slot.specularTexturePath.empty();
		if (needsUv && (uvAccessor == nullptr || uvAccessor->count != positionAccessor->count))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF textured primitive is missing TEXCOORD_0: " + mesh.name);
		}

		std::vector<float> normals{};
		if (normalAccessor != nullptr)
		{
			auto readNormals = ReadAccessorFloats(normalAccessor, cgltf_type_vec3, "NORMAL");
			if (!readNormals || normalAccessor->count != positionAccessor->count)
			{
				return FailRuntime(
					LogCategory::Asset,
					readNormals ? ErrorCode::InvalidArgument : readNormals.error.code,
					readNormals ? "glTF NORMAL count does not match POSITION: " + mesh.name : readNormals.error.message);
			}
			normals = std::move(readNormals.value);
		}

		std::vector<float> tangents{};
		if (tangentAccessor != nullptr)
		{
			auto readTangents = ReadAccessorFloats(tangentAccessor, cgltf_type_vec4, "TANGENT");
			if (!readTangents || tangentAccessor->count != positionAccessor->count)
			{
				return FailRuntime(
					LogCategory::Asset,
					readTangents ? ErrorCode::InvalidArgument : readTangents.error.code,
					readTangents ? "glTF TANGENT count does not match POSITION: " + mesh.name
								 : readTangents.error.message);
			}
			tangents = std::move(readTangents.value);
		}

		std::vector<float> uvs{};
		if (uvAccessor != nullptr)
		{
			auto readUvs = ReadAccessorFloats(uvAccessor, cgltf_type_vec2, "TEXCOORD_0");
			if (!readUvs || uvAccessor->count != positionAccessor->count)
			{
				return FailRuntime(
					LogCategory::Asset,
					readUvs ? ErrorCode::InvalidArgument : readUvs.error.code,
					readUvs ? "glTF TEXCOORD_0 count does not match POSITION: " + mesh.name : readUvs.error.message);
			}
			uvs = std::move(readUvs.value);
		}

		if (mesh.vertices.size() > static_cast<size_t>((std::numeric_limits<uint32_t>::max)()) - positionAccessor->count)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF mesh is too large: " + mesh.name);
		}

		const uint32_t vertexBegin = static_cast<uint32_t>(mesh.vertices.size());
		const uint32_t vertexCount = static_cast<uint32_t>(positionAccessor->count);
		mesh.vertices.resize(static_cast<size_t>(vertexBegin) + vertexCount);
		for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			BasicVertex& vertex = mesh.vertices[static_cast<size_t>(vertexBegin) + vertexIndex];
			const float axisSign = importOptions.convertAxisToDirectX ? -1.0f : 1.0f;
			const float* sourcePosition = positions.value.data() + (static_cast<size_t>(vertexIndex) * 3);
			vertex.position[0] = sourcePosition[0];
			vertex.position[1] = sourcePosition[1];
			vertex.position[2] = sourcePosition[2] * axisSign;
			vertex.position[3] = 1.0f;
			if (!normals.empty())
			{
				const float* sourceNormal = normals.data() + (static_cast<size_t>(vertexIndex) * 3);
				vertex.normal[0] = sourceNormal[0];
				vertex.normal[1] = sourceNormal[1];
				vertex.normal[2] = sourceNormal[2] * axisSign;
			}
			if (!uvs.empty())
			{
				const float* sourceUv = uvs.data() + (static_cast<size_t>(vertexIndex) * 2);
				vertex.uv[0] = sourceUv[0];
				vertex.uv[1] = sourceUv[1];
			}
			if (!tangents.empty())
			{
				const float* sourceTangent = tangents.data() + (static_cast<size_t>(vertexIndex) * 4);
				vertex.tangent[0] = sourceTangent[0];
				vertex.tangent[1] = sourceTangent[1];
				vertex.tangent[2] = sourceTangent[2] * axisSign;
				vertex.tangent[3] = sourceTangent[3] * axisSign;
			}

			if (!IsFiniteVector(vertex.position, 4) || !IsFiniteVector(vertex.normal, 3)
				|| !IsFiniteVector(vertex.uv, 2) || !IsFiniteVector(vertex.tangent, 4))
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF mesh contains non-finite vertex data: " + mesh.name);
			}

			ExpandBounds(mesh.bounds, vertex.position);
		}

		const uint32_t indexBegin = static_cast<uint32_t>(mesh.indices.size());
		if (primitive.indices == nullptr)
		{
			if (vertexCount % 3 != 0)
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF triangle list is not divisible by 3: " + mesh.name);
			}
			mesh.indices.reserve(mesh.indices.size() + vertexCount);
			for (uint32_t index = 0; index < vertexCount; index += 3)
			{
				mesh.indices.push_back(vertexBegin + index);
				if (importOptions.convertAxisToDirectX)
				{
					mesh.indices.push_back(vertexBegin + index + 2);
					mesh.indices.push_back(vertexBegin + index + 1);
				}
				else
				{
					mesh.indices.push_back(vertexBegin + index + 1);
					mesh.indices.push_back(vertexBegin + index + 2);
				}
			}
		}
		else
		{
			const cgltf_accessor* indexAccessor = primitive.indices;
			if (indexAccessor->is_sparse || indexAccessor->type != cgltf_type_scalar || indexAccessor->count == 0
				|| indexAccessor->count > kMaxAccessorElements || indexAccessor->count % 3 != 0
				|| (indexAccessor->component_type != cgltf_component_type_r_8u
					&& indexAccessor->component_type != cgltf_component_type_r_16u
					&& indexAccessor->component_type != cgltf_component_type_r_32u))
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF index accessor must be a triangle list of unsigned scalars: " + mesh.name);
			}

			std::vector<uint32_t> indices(static_cast<size_t>(indexAccessor->count));
			const cgltf_size readCount = cgltf_accessor_unpack_indices(
				indexAccessor,
				indices.data(),
				sizeof(uint32_t),
				indices.size());
			if (readCount != indices.size())
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF index accessor could not be read: " + mesh.name);
			}

			if (mesh.indices.size() > static_cast<size_t>((std::numeric_limits<uint32_t>::max)()) - indices.size())
			{
				return FailRuntime(
					LogCategory::Asset,
					ErrorCode::InvalidArgument,
					"glTF mesh is too large: " + mesh.name);
			}

			mesh.indices.reserve(mesh.indices.size() + indices.size());
			for (size_t index = 0; index < indices.size(); index += 3)
			{
				const uint32_t triangle[3] = {
					indices[index],
					importOptions.convertAxisToDirectX ? indices[index + 2] : indices[index + 1],
					importOptions.convertAxisToDirectX ? indices[index + 1] : indices[index + 2],
				};
				for (const uint32_t localIndex : triangle)
				{
					if (localIndex >= vertexCount)
					{
						return FailRuntime(
							LogCategory::Asset,
							ErrorCode::InvalidArgument,
							"glTF index is out of range: " + mesh.name);
					}
					mesh.indices.push_back(vertexBegin + localIndex);
				}
			}
		}

		ModelSubMeshData submesh{};
		submesh.indexStart = indexBegin;
		submesh.indexCount = static_cast<uint32_t>(mesh.indices.size() - indexBegin);
		submesh.materialSlot = materialSlot.value;
		mesh.submeshes.push_back(submesh);

		if (normalAccessor == nullptr && importOptions.generateNormalsIfMissing)
		{
			GenerateNormalsForRange(mesh.vertices, mesh.indices, vertexBegin, vertexCount, indexBegin, submesh.indexCount);
		}
		if (tangentAccessor == nullptr)
		{
			GenerateTangentsForRange(mesh.vertices, mesh.indices, vertexBegin, vertexCount, indexBegin, submesh.indexCount);
		}

		return MakeOk();
	}

	[[nodiscard]] Result<int32_t> MeshIndexFor(const cgltf_mesh* mesh)
	{
		const auto existing = meshIndices.find(mesh);
		if (existing != meshIndices.end())
		{
			return MakeOk(existing->second);
		}

		if (mesh->primitives_count == 0)
		{
			return FailRuntime<int32_t>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF mesh has no primitives: " + absolutePath.string());
		}

		ModelMeshData meshData{};
		meshData.name = NameOr(mesh->name, "Mesh");
		ResetBounds(meshData.bounds);
		for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh->primitives_count; ++primitiveIndex)
		{
			if (auto appended = AppendPrimitive(mesh->primitives[primitiveIndex], meshData); !appended)
			{
				return MakeFail<int32_t>(appended.error.code, appended.error.message);
			}
		}

		if (meshData.vertices.empty() || meshData.indices.empty() || meshData.bounds.minX > meshData.bounds.maxX)
		{
			return FailRuntime<int32_t>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF mesh extraction produced empty geometry: " + meshData.name);
		}

		const int32_t meshIndex = static_cast<int32_t>(asset.meshes.size());
		asset.meshes.push_back(std::move(meshData));
		meshIndices.emplace(mesh, meshIndex);
		return MakeOk(meshIndex);
	}

	[[nodiscard]] Result<void> VisitNode(const cgltf_node* node, int32_t parentIndex, int depth)
	{
		if (node == nullptr || depth > kMaxNodeDepth)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF node hierarchy is too deep: " + absolutePath.string());
		}
		if (!visited.insert(node).second)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"glTF node hierarchy contains a cycle: " + absolutePath.string());
		}

		ModelNodeData nodeData{};
		nodeData.name = NameOr(node->name, "Node");
		nodeData.parentIndex = parentIndex;
		float columnMajor[16]{};
		cgltf_node_transform_local(node, columnMajor);
		StoreEngineMatrix(nodeData.localTransform, columnMajor, importOptions.convertAxisToDirectX);

		const uint32_t nodeIndex = static_cast<uint32_t>(asset.nodes.size());
		asset.nodes.push_back(std::move(nodeData));
		if (parentIndex >= 0)
		{
			asset.nodes[static_cast<size_t>(parentIndex)].childNodeIndices.push_back(nodeIndex);
		}

		if (node->mesh != nullptr)
		{
			auto meshIndex = MeshIndexFor(node->mesh);
			if (!meshIndex)
			{
				return MakeFail(meshIndex.error.code, meshIndex.error.message);
			}
			asset.nodes[nodeIndex].meshIndex = meshIndex.value;
		}

		for (cgltf_size childIndex = 0; childIndex < node->children_count; ++childIndex)
		{
			if (auto child = VisitNode(node->children[childIndex], static_cast<int32_t>(nodeIndex), depth + 1); !child)
			{
				return child;
			}
		}

		return MakeOk();
	}
};

[[nodiscard]] bool ImageIsReferenced(const cgltf_data* data, const cgltf_image* image)
{
	for (cgltf_size materialIndex = 0; materialIndex < data->materials_count; ++materialIndex)
	{
		const cgltf_material& material = data->materials[materialIndex];
		const cgltf_texture* baseColor = material.has_pbr_metallic_roughness
			? material.pbr_metallic_roughness.base_color_texture.texture
			: nullptr;
		const cgltf_texture_view* specularView = SpecularColorTextureView(material);
		const cgltf_texture* specular = specularView != nullptr ? specularView->texture : nullptr;
		const cgltf_texture* textures[] = { baseColor, material.normal_texture.texture, specular };
		for (const cgltf_texture* texture : textures)
		{
			if (texture != nullptr && texture->image == image)
			{
				return true;
			}
		}
	}

	return false;
}

[[nodiscard]] Result<std::filesystem::path> ResolveHashedDependency(
	const std::filesystem::path& directory,
	const std::filesystem::path& modelPath,
	const char* uri)
{
	if (uri == nullptr || uri[0] == '\0' || std::strncmp(uri, "data:", 5) == 0)
	{
		return MakeOk(std::filesystem::path{});
	}
	if (std::strstr(uri, "://") != nullptr || !IsSafeRelativeFileUri(DecodeUri(uri)))
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF dependency URI must stay beside the file: " + modelPath.string());
	}

	const std::filesystem::path relativePath = PathFromUtf8(DecodeUri(uri));
	const std::filesystem::path absoluteDependency = directory / relativePath;
	std::error_code errorCode{};
	if (!std::filesystem::is_regular_file(absoluteDependency, errorCode) || errorCode)
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"glTF dependency file not found: " + absoluteDependency.string());
	}

	const std::filesystem::path resolved = CanonicalizeIfPossible(absoluteDependency);
	if (!IsPathUnderRoot(resolved, CanonicalDirectory(directory)))
	{
		return FailRuntime<std::filesystem::path>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"glTF dependency URI must stay beside the file: " + modelPath.string());
	}

	return MakeOk(resolved);
}

[[nodiscard]] Result<uint64_t> HashGltfModelSourceImpl(
	const std::filesystem::path& absolutePath,
	std::string_view fileBytes)
{
	cgltf_options options{};
	GltfData gltf{};
	const cgltf_result parsed = cgltf_parse(&options, fileBytes.data(), fileBytes.size(), &gltf.data);
	if (parsed != cgltf_result_success || gltf.data == nullptr)
	{
		return FailRuntime<uint64_t>(
			LogCategory::Asset,
			ErrorCode::RuntimeError,
			"Failed to parse glTF file " + absolutePath.string() + ": " + DescribeGltfResult(parsed));
	}

	uint64_t hash = Fnv1a64(fileBytes);
	const auto appendDependency = [&](const std::filesystem::path& path) -> Result<void> {
		if (path.empty())
		{
			return MakeOk();
		}

		const Result<std::string> bytes = ReadBinaryFile(path);
		if (!bytes)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::FileNotFound,
				"glTF dependency file could not be opened: " + path.string());
		}

		hash = Fnv1a64Append(hash, static_cast<uint8_t>(0));
		hash = Fnv1a64Append(hash, std::string_view(bytes.value));
		return MakeOk();
	};

	const std::filesystem::path directory = absolutePath.parent_path();
	for (cgltf_size index = 0; index < gltf.data->buffers_count; ++index)
	{
		auto dependency = ResolveHashedDependency(directory, absolutePath, gltf.data->buffers[index].uri);
		if (!dependency)
		{
			return MakeFail<uint64_t>(dependency.error.code, dependency.error.message);
		}
		if (auto appended = appendDependency(dependency.value); !appended)
		{
			return MakeFail<uint64_t>(appended.error.code, appended.error.message);
		}
	}

	for (cgltf_size index = 0; index < gltf.data->images_count; ++index)
	{
		const cgltf_image& image = gltf.data->images[index];
		if (!ImageIsReferenced(gltf.data, &image))
		{
			continue;
		}

		auto dependency = ResolveHashedDependency(directory, absolutePath, image.uri);
		if (!dependency)
		{
			return MakeFail<uint64_t>(dependency.error.code, dependency.error.message);
		}
		if (auto appended = appendDependency(dependency.value); !appended)
		{
			return MakeFail<uint64_t>(appended.error.code, appended.error.message);
		}
	}

	return MakeOk(hash);
}
} // namespace

Result<uint64_t> HashGltfModelSource(
	const std::filesystem::path& absolutePath,
	std::string_view fileBytes)
{
	return HashGltfModelSourceImpl(absolutePath, fileBytes);
}

Result<ModelAssetData> LoadGltfModelAssetDataFromFile(
	const std::filesystem::path& absolutePath,
	const FbxModelLoadOptions& loadOptions)
{
	if (absolutePath.empty())
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model file path is empty");
	}

	if (!HasGltfExtension(absolutePath))
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Model path must use the .gltf or .glb extension: " + absolutePath.string());
	}

	std::error_code errorCode{};
	if (!std::filesystem::exists(absolutePath, errorCode) || errorCode)
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Model file not found: " + absolutePath.string());
	}

	cgltf_options options{};
	options.file.read = &ReadGltfFile;
	options.file.release = &ReleaseGltfFile;
	const std::string utf8Path = PathToUtf8(absolutePath);

	GltfData gltf{};
	const cgltf_result parsed = cgltf_parse_file(&options, utf8Path.c_str(), &gltf.data);
	if (parsed != cgltf_result_success || gltf.data == nullptr)
	{
		return FailRuntime<ModelAssetData>(
			LogCategory::Asset,
			ErrorCode::RuntimeError,
			"Failed to parse glTF file " + absolutePath.string() + ": " + DescribeGltfResult(parsed));
	}

	GltfImporter importer{};
	importer.absolutePath = absolutePath;
	importer.directory = absolutePath.parent_path();
	importer.importOptions = loadOptions;
	importer.data = gltf.data;
	if (auto imported = importer.Import(); !imported)
	{
		return MakeFail<ModelAssetData>(imported.error.code, imported.error.message);
	}

	return MakeOk(std::move(importer.asset));
}
