#pragma once

#include "Engine/AssetSystem/Mesh/PrimitiveMesh.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"

[[nodiscard]] MeshUploadDesc MakePrimitiveMeshUploadDesc(
	const PrimitiveMeshData& mesh,
	const char* debugName);
