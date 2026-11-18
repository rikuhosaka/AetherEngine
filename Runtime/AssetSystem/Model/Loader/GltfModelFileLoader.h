#pragma once

#include "Runtime/AssetSystem/Model/Loader/ModelLoadTypes.h"
#include "Runtime/Core/Log/Result.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

// Reads one glTF 2.0 .gltf or .glb file into ModelAssetData.
// Static meshes only. convertAxisToDirectX mirrors glTF's right-handed Y-up space into the
// row-major DirectX space written by the FBX importer. generateNormalsIfMissing fills normals
// when a primitive does not have them. Triangulation and unit conversion do not apply: glTF
// primitives must already be triangles, and the unit is meters.
[[nodiscard]] Result<ModelAssetData> LoadGltfModelAssetDataFromFile(
	const std::filesystem::path& absolutePath,
	const FbxModelLoadOptions& options = {});

// Hash of the glTF bytes plus external buffers and images referenced by the imported materials.
[[nodiscard]] Result<uint64_t> HashGltfModelSource(
	const std::filesystem::path& absolutePath,
	std::string_view fileBytes);
