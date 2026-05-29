#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/RHI/Common/RHIInput.h"

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
