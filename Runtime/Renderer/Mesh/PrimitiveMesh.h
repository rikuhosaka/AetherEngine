#pragma once

#include "Runtime/Engine/AssetSystem/Mesh/PrimitiveMesh.h"
#include "Runtime/Renderer/Mesh/MeshUpload.h"

[[nodiscard]] MeshUploadDesc MakePrimitiveMeshUploadDesc(
	const PrimitiveMeshData& mesh,
	const char* debugName);
