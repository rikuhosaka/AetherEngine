#pragma once

#include "Runtime/AssetData/PrimitiveMeshData.h"
#include "Runtime/Renderer/Mesh/MeshUpload.h"

[[nodiscard]] MeshUploadDesc MakePrimitiveMeshUploadDesc(
	const PrimitiveMeshData& mesh,
	const char* debugName);
