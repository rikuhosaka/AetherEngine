#pragma once

#include "Runtime/Renderer/Mesh/MeshUpload.h"
#include "Runtime/Renderer/Texture/TextureTypes.h"

#include <cstdint>
#include <string>
#include <vector>

// Slot value when a model material slot has no texture.
inline constexpr uint32_t kNoSceneTexture = ~uint32_t{ 0 };

// Spans inside the upload descs stay valid only while the caller keeps the CPU bytes alive.
struct SceneNodeUpload
{
	std::string name{};
	float localTransform[16]{};
	int32_t parentIndex = -1;
	int32_t meshIndex = -1;
};

struct SceneMeshUpload
{
	std::string key{};
	MeshUploadDesc mesh{};
};

struct SceneTextureUpload
{
	std::string cacheKey{};
	std::string debugName{};
	TextureUploadDesc image{};
};

struct SceneModelUpload
{
	uint32_t documentId = 0;
	std::vector<std::string> meshNames{};
	std::vector<MeshUploadDesc> meshes{};
	std::vector<SceneNodeUpload> nodes{};
	std::vector<uint32_t> baseColorBySlot{};
	std::vector<uint32_t> normalBySlot{};
};

struct SceneGpuUpload
{
	std::vector<SceneMeshUpload> meshes{};
	std::vector<SceneTextureUpload> textures{};
	std::vector<SceneModelUpload> models{};
};
