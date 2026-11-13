#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/Mesh/MeshTypes.h"
#include "Runtime/RHI/Common/RHIInput.h"

#include <memory>
#include <span>

class RHIDevice;

struct MeshUploadDesc
{
	std::span<const std::byte> vertices{};
	std::span<const std::byte> indices{};
	uint32_t vertexStride = 0;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	IndexFormat indexFormat = IndexFormat::R32_UINT;
	VertexLayoutId layoutId = VertexLayoutId::PositionTex;
	std::vector<SubmeshRange> submeshes{};
	MeshBounds bounds{};

	// Optional mesh label used for GPU buffer debug names. UTF-8.
	const char* DebugName = nullptr;
};

class MeshUpload
{
public:
	explicit MeshUpload(RHIDevice* device);

	[[nodiscard]] Result<std::unique_ptr<Mesh>> CreateMesh(
		const MeshUploadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList) const;

private:
	RHIDevice* m_device = nullptr;
};
