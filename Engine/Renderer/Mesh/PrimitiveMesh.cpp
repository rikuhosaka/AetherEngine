#include "Engine/Renderer/Mesh/PrimitiveMesh.h"

#include <cstddef>
#include <cstdint>

MeshUploadDesc MakePrimitiveMeshUploadDesc(const PrimitiveMeshData& mesh, const char* debugName)
{
	MeshUploadDesc desc{};
	desc.vertices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(mesh.vertices.data()),
		mesh.vertices.size() * sizeof(BasicVertex));
	desc.indices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(mesh.indices.data()),
		mesh.indices.size() * sizeof(uint32_t));
	desc.vertexStride = sizeof(BasicVertex);
	desc.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
	desc.indexCount = static_cast<uint32_t>(mesh.indices.size());
	desc.indexFormat = IndexFormat::R32_UINT;
	desc.layoutId = VertexLayoutId::Basic;
	desc.submeshes.push_back(SubmeshRange{ 0, desc.indexCount, 0 });
	desc.bounds = mesh.bounds;
	desc.DebugName = debugName;
	return desc;
}
